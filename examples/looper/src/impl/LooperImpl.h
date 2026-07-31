#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
#include "Generators/MeterTimeline.h"
#include "Sampler/LoopFile.h"
#include "Sampler/LoopRecorder.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SequencerEngine.h"
#include "Sampler/SliceLibrary.h"

// ADL hooks so LoopFile<nlohmann::json> can (de)serialize AbacDsp::LoopMetadata;
// kept here (not in core) since the core library must stay JSON-library-free.
namespace AbacDsp
{
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LoopMetadata, version, bpm, bars, beats)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SequenceEvent, stepPosition, track, sliceIndex, gain, pitchRatio,
                                                reverse, randomizeSlice, timingOffsetFrames, humanizeAmountFrames)
}

// SequencePattern has no default constructor, so it needs nlohmann's
// adl_serializer specialization hook instead of free to_json/from_json.
namespace nlohmann
{
template <>
struct adl_serializer<AbacDsp::SequencePattern>
{
    static void to_json(json& j, const AbacDsp::SequencePattern& p)
    {
        j = json{{"lengthBars", p.lengthBars()},
                 {"beatsPerBar", p.beatsPerBar()},
                 {"stepsPerBeat", p.stepsPerBeat()},
                 {"events", p.events()}};
    }

    static AbacDsp::SequencePattern from_json(const json& j)
    {
        AbacDsp::SequencePattern pattern(j.at("lengthBars").get<size_t>(), j.at("beatsPerBar").get<size_t>(),
                                         j.at("stepsPerBeat").get<size_t>());
        for (const auto& eventJson : j.at("events"))
        {
            pattern.addEvent(eventJson.get<AbacDsp::SequenceEvent>());
        }
        return pattern;
    }
};
}

#include "CaptureRing.h"
#include "FreezeService.h"
#include "LoopStorageService.h"
#include "LooperTimingController.h"
#include "LooperTransportController.h"
#include "LooperViewModel.h"
#include "SequencerPatternBuilder.h"

// Traditional-style slicing looper: captures audio, quantizes the loop to whole
// bars, and plays it back locked to a metronome click. The four transport
// controls are momentary pulses that toggle the real state; the editor reads the
// state back for labels, so there is no toggle-vs-state desync. Slicing is not
// performed here; it will be reintroduced as an on-demand sample sequencer layer
// on top of this plain looper.
//
// Per-take mode (setFreeRecord): bar-locked snaps start/stop to the nearest
// bar tick; Free Record captures exactly where triggered, unquantized.
template <size_t BlockSize>
class LooperImpl final : public EffectBase
{
  public:
    static constexpr float kSliceLibrarySeconds = 120.f;
    // Primitive (no anti-alias filter) decimation feeding the record spectrogram,
    // trading some aliasing for a display cut around 6 kHz and 4x fewer FFT frames.
    static constexpr size_t kSpectrogramDecimation = 4;
    // Hop/fftLength ratio: high overlap for fine time resolution. Regen for the
    // longest loops still finishes in well under 20 ms, so there's ample headroom.
    static constexpr float kSpectrogramWindowForward = 1.f / 12.f;

    explicit LooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_recorder(sampleRate)
        , m_seq(sampleRate)
        , m_click(sampleRate)
        , m_sliceLibrary(static_cast<size_t>(sampleRate * kSliceLibrarySeconds))
        , m_sequencer(sampleRate)
        , m_pattern(1, m_seq.beatsPerBar(), m_seq.samplesPerBeat())
        , m_patternBuilder(m_seq, m_recorder, m_sliceLibrary, m_pattern)
        , m_timingController(m_seq, m_meterTimeline, m_appliedBpm, m_eighthNoteUnit, m_appliedTimeSignature,
                             m_finalizedBarCount, m_countingIn, m_countInBarsOffset, m_countInEndTickAbs,
                             m_suppressNextClick, sampleRate)
        , m_captureRing(sampleRate)
        , m_freezeService(m_recorder)
        , m_loopStorage(m_recorder, m_seq, m_sliceLibrary, m_pattern, m_meterTimeline, m_appliedBpm, m_eighthNoteUnit,
                        sampleRate)
        , m_transportController(typename LooperTransportController<BlockSize>::Deps{
              .recorder = m_recorder,
              .seq = m_seq,
              .timing = m_timingController,
              .meterTimeline = m_meterTimeline,
              .captureRing = m_captureRing,
              .freezeService = m_freezeService,
              .loopStorage = m_loopStorage,
              .sliceLibrary = m_sliceLibrary,
              .pattern = m_pattern,
              .sequencer = m_sequencer,
              .armed = m_armed,
              .countingIn = m_countingIn,
              .autoStopArmed = m_autoStopArmed,
              .autoStopBarTarget = m_autoStopBarTarget,
              .pendingStop = m_pendingStop,
              .pendingStopTickAbs = m_pendingStopTickAbs,
              .pendingStopLoopLength = m_pendingStopLoopLength,
              .barLockedTake = m_barLockedTake,
              .startOffset = m_startOffset,
              .tickAbs = m_tickAbs,
              .takeBarIndex = m_takeBarIndex,
              .suppressNextBarIndexIncrement = m_suppressNextBarIndexIncrement,
              .sequencerPlaying = m_sequencerPlaying,
              .appliedTimeSignature = m_appliedTimeSignature,
              .pendingTimeSignature = m_pendingTimeSignature,
              .finalizedBarCount = m_finalizedBarCount,
              .absPos = m_absPos,
              .freeRecord = m_freeRecord,
              .countInBars = m_countInBars,
              .recordBars = m_recordBars,
              .autoStopEnabled = m_autoStopEnabled,
              .clearPulse = m_clearPulse,
              .recordPulse = m_recordPulse,
              .playPulse = m_playPulse,
              .overdubPulse = m_overdubPulse,
              .freezePulse = m_freezePulse,
              .seqPlayPulse = m_seqPlayPulse,
              .clearSeqPulse = m_clearSeqPulse,
              .threshRecReq = m_threshRecReq,
              .undoPulse = m_undoPulse,
              .mixDownPulse = m_mixDownPulse,
          })
        , m_viewModel(typename LooperViewModel<BlockSize>::Deps{
              .recorder = m_recorder,
              .seq = m_seq,
              .meterTimeline = m_meterTimeline,
              .appliedBpm = m_appliedBpm,
              .sliceLibrary = m_sliceLibrary,
              .pattern = m_pattern,
              .sequencer = m_sequencer,
              .recordSpectrogram = m_recordSpectrogram,
              .visualWave = m_visualWave,
              .preparedWave = m_preparedWave,
              .visualWindowSize = m_visualWindowSize,
              .spectrogramRegenRequestGen = m_spectrogramRegenRequestGen,
              .spectrogramRegenDoneGen = m_spectrogramRegenDoneGen,
              .autoStopEnabled = m_autoStopEnabled,
              .recordBars = m_recordBars,
              .finalizedBarCount = m_finalizedBarCount,
              .countInBarsOffset = m_countInBarsOffset,
              .sequencerPlaying = m_sequencerPlaying,
              .sampleRate = sampleRate,
          })
    {
        m_seq.setBpm(m_appliedBpm);
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        // One bar (4 beats) at the lowest tempo (50 BPM) is ~4.8 s; size generously.
        m_visualWave.assign(static_cast<size_t>(sampleRate * 5.f) + 16, 0.f);
        m_preparedWave.reserve(m_visualWave.size());
        // Sample rate is that of the decimated feed (see kSpectrogramDecimation),
        // so the display's Nyquist axis reflects what's actually analyzed.
        const float spectrogramSampleRate = sampleRate / static_cast<float>(kSpectrogramDecimation);
        m_recordSpectrogram.setSampleRate(spectrogramSampleRate);
        m_recordSpectrogram.setWindowForward(kSpectrogramWindowForward);
        // Cover the whole recordable span so a long loop's ring is fully painted,
        // not just its tail. hop = fftLength * windowForwardRatio.
        const auto decimatedMaxFrames = static_cast<float>(m_recorder.maxFrames()) / kSpectrogramDecimation;
        m_recordSpectrogram.setSlices(static_cast<size_t>(decimatedMaxFrames / (1024.f * kSpectrogramWindowForward)) +
                                      64);
        m_sequencer.setLibrary(&m_sliceLibrary);
        m_sequencer.setPattern(&m_pattern);

        m_spectrogramRegenThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_spectrogramRegenWaitMutex);
                    m_spectrogramRegenCv.wait(
                        lock, stopToken, [this, lastHandled]
                        { return m_spectrogramRegenRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_spectrogramRegenRequestGen.load(std::memory_order_acquire);
                    runSpectrogramRegen(lastHandled);
                }
            });
    }

    // Parameter setters (message thread): store into atomics, apply on the audio thread.
    void setBpm(const float value) noexcept
    {
        m_bpm.store(value, std::memory_order_relaxed);
    }

    void setClickVolume(const float value) noexcept
    {
        m_clickVol.store(value, std::memory_order_relaxed);
    }
    // Volume at which the click gets mixed into what actually gets recorded
    // (not just monitored). -60 dB means nothing is added to the track.
    void setClickRecordVolume(const float value) noexcept
    {
        m_clickRecordVol.store(value, std::memory_order_relaxed);
    }
    void setLoopVolume(const float value) noexcept
    {
        m_loopVol.store(value, std::memory_order_relaxed);
    }
    void setThreshRec(const bool value) noexcept
    {
        m_threshRecReq.store(value, std::memory_order_relaxed);
    }
    void setRecThreshold(const float value) noexcept
    {
        m_recThreshold.store(value, std::memory_order_relaxed);
    }
    // Decided per-take at record-start; toggling mid-take doesn't retroactively change it.
    void setFreeRecord(const bool value) noexcept
    {
        m_freeRecordReq.store(value, std::memory_order_relaxed);
    }
    // Auto Stop off: manual stop (today's behavior, ignoring this value). On:
    // auto-stop after exactly this many bars. Only applies to bar-locked takes
    // (ignored while Free Record is active).
    void setRecordBars(const float value) noexcept
    {
        m_recordBarsReq.store(static_cast<int>(value), std::memory_order_relaxed);
    }
    void setAutoStop(const bool value) noexcept
    {
        m_autoStopEnabledReq.store(value, std::memory_order_relaxed);
    }
    // Index into kTimeSignatures. Applies immediately while stopped/armed/counting
    // in; queues to apply at the next bar boundary while recording; ignored while
    // just playing back or overdubbing (the loop replays its own recorded meter
    // timeline instead, see applyMeterAtBarBoundary()).
    void setTimeSignature(const int value) noexcept
    {
        m_timeSignatureReq.store(value, std::memory_order_relaxed);
    }
    // 0 = off (record starts immediately); N = play N bars of metronome
    // first, then auto-start a bar-locked take with no threshold needed.
    void setCountInBars(const int value) noexcept
    {
        m_countInBarsReq.store(value, std::memory_order_relaxed);
    }
    // Loop boundary click-free fade (record-stop wrap seam, and the
    // bar-locked relocate/fold edges), milliseconds.
    void setFadeMs(const float value) noexcept
    {
        m_fadeMs.store(value, std::memory_order_relaxed);
    }
    void setSliceDivision(const int value) noexcept
    {
        m_sliceDivision.store(value, std::memory_order_relaxed);
    }
    void setHostSync(const bool value) noexcept
    {
        m_hostSyncReq.store(value, std::memory_order_relaxed);
    }
    void setRecord(const bool value) noexcept
    {
        if (value)
        {
            m_recordPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setPlay(const bool value) noexcept
    {
        if (value)
        {
            m_playPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setOverdub(const bool value) noexcept
    {
        if (value)
        {
            m_overdubPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setUndo(const bool value) noexcept
    {
        if (value)
        {
            m_undoPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setMixDown(const bool value) noexcept
    {
        if (value)
        {
            m_mixDownPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setClear(const bool value) noexcept
    {
        if (value)
        {
            m_clearPulse.store(true, std::memory_order_relaxed);
        }
    }
    // Manual "freeze": slices the current loop into a new SliceLibrary track.
    void setFreeze(const bool value) noexcept
    {
        if (value)
        {
            m_freezePulse.store(true, std::memory_order_relaxed);
        }
    }
    // Directory named loop saves/loads live in; set once during setup.
    void setLoopsDirectory(std::string dir)
    {
        m_loopStorage.setLoopsDirectory(std::move(dir));
    }

    [[nodiscard]] std::vector<std::string> listLoopNames() const
    {
        return m_loopStorage.listLoopNames();
    }

    bool deleteLoopNamed(const std::string& name)
    {
        return m_loopStorage.deleteLoopNamed(name);
    }

    bool renameLoopNamed(const std::string& oldName, const std::string& newName)
    {
        return m_loopStorage.renameLoopNamed(oldName, newName);
    }

    // Guarded like requestFreeze(); writes <loopsDirectory>/<name>.wav + .json
    // on a background worker: file I/O is not RT-safe.
    void requestSaveLoopAs(const std::string& name)
    {
        if (isRecording() || isOverdubbing() || m_recorder.loopLengthFrames() == 0)
        {
            return;
        }
        m_loopStorage.requestSave(name);
    }

    [[nodiscard]] bool isLoopSavePending() const noexcept
    {
        return m_loopStorage.isSavePending();
    }

    [[nodiscard]] std::string consumeLastSavedLoopName()
    {
        return m_loopStorage.consumeLastSavedLoopName();
    }

    // Reported back to the UI once a load has decoded and compared metadata.
    using LoopLoadOutcome = typename LoopStorageService<BlockSize>::LoopLoadOutcome;

    // Reads <loopsDirectory>/<name>.wav + .json on a background worker; the
    // audio thread installs the result once resolved.
    void requestLoadLoop(const std::string& name)
    {
        m_loopStorage.requestLoad(name);
    }

    [[nodiscard]] bool isLoopLoadPending() const noexcept
    {
        return m_loopStorage.isLoadPending();
    }

    [[nodiscard]] LoopLoadOutcome consumeLoopLoadOutcome()
    {
        return m_loopStorage.consumeLoadOutcome();
    }

    // Picks which BPM becomes the installed loop's tempo after a conflict was
    // reported; a no-op if no load is currently awaiting resolution.
    void resolveLoopLoadBpm(const float bpm)
    {
        m_loopStorage.resolveLoadBpm(bpm);
    }

    // Host/session state (e.g. a DAW's getStateInformation/setStateInformation, or the
    // standalone app's own properties-file round-trip): the loop's audio and tempo/meter,
    // packed into a self-contained blob so the caller doesn't touch the loops directory.
    // Frozen tracks/sequencer pattern are not included (out of scope for now).
    [[nodiscard]] std::vector<std::byte> captureExtraState() const
    {
        const size_t loopLen = m_recorder.loopLengthFrames();
        if (isRecording() || isOverdubbing() || loopLen == 0)
        {
            return {};
        }

        const std::vector<AbacDsp::MeterSegment> segments =
            m_meterTimeline.empty() ? std::vector<AbacDsp::MeterSegment>{{0, m_seq.beatsPerBar(), m_eighthNoteUnit}}
                                    : m_meterTimeline.segments();

        ExtraStateHeader header{};
        header.magic = kExtraStateMagic;
        header.version = kExtraStateVersion;
        header.sampleRate = sampleRate();
        header.bpm = m_appliedBpm;
        header.segmentCount = static_cast<uint32_t>(segments.size());
        header.loopLengthFrames = static_cast<uint64_t>(loopLen);

        const bool hasOverdub = m_recorder.hasOverdub();
        std::vector<std::byte> blob(sizeof(ExtraStateHeader) + segments.size() * sizeof(SerializedMeterSegment) +
                                    loopLen * 2 * sizeof(float) + sizeof(uint32_t) +
                                    (hasOverdub ? loopLen * 2 * sizeof(float) : 0));
        size_t offset = 0;
        appendPod(blob, offset, header);
        for (const auto& seg : segments)
        {
            appendPod(blob, offset,
                      SerializedMeterSegment{static_cast<uint64_t>(seg.startBar),
                                             static_cast<uint64_t>(seg.beatsPerBar), seg.eighthUnit});
        }
        for (size_t f = 0; f < loopLen; ++f)
        {
            appendPod(blob, offset, m_recorder.sample(f, 0));
            appendPod(blob, offset, m_recorder.sample(f, 1));
        }
        appendPod(blob, offset, static_cast<uint32_t>(hasOverdub ? 1 : 0));
        if (hasOverdub)
        {
            for (size_t f = 0; f < loopLen; ++f)
            {
                appendPod(blob, offset, m_recorder.overdubSample(f, 0));
                appendPod(blob, offset, m_recorder.overdubSample(f, 1));
            }
        }
        return blob;
    }

    // Message thread only: feeds a blob from captureExtraState() into the same
    // gen-counter handshake runLoadLoop() uses on success, so the existing
    // checkLoopLoadCompletion() poll installs it on the audio thread. A no-op on any
    // parse failure or malformed/foreign data, and while a named load is already pending.
    void restoreExtraState(std::span<const std::byte> blob)
    {
        if (isLoopLoadPending())
        {
            return;
        }

        size_t offset = 0;
        ExtraStateHeader header{};
        if (!readPod(blob, offset, header) || header.magic != kExtraStateMagic ||
            (header.version != 1 && header.version != 2) || header.loopLengthFrames == 0)
        {
            return;
        }

        AbacDsp::MeterTimeline timeline;
        for (uint32_t i = 0; i < header.segmentCount; ++i)
        {
            SerializedMeterSegment seg{};
            if (!readPod(blob, offset, seg))
            {
                return;
            }
            timeline.addSegment(static_cast<size_t>(seg.startBar), static_cast<size_t>(seg.beatsPerBar),
                                seg.eighthUnit);
        }

        std::vector<float> left(header.loopLengthFrames);
        std::vector<float> right(header.loopLengthFrames);
        for (uint64_t f = 0; f < header.loopLengthFrames; ++f)
        {
            if (!readPod(blob, offset, left[f]) || !readPod(blob, offset, right[f]))
            {
                return;
            }
        }

        std::vector<float> overdubLeft;
        std::vector<float> overdubRight;
        if (header.version >= 2)
        {
            uint32_t hasOverdub{};
            if (!readPod(blob, offset, hasOverdub))
            {
                return;
            }
            if (hasOverdub != 0)
            {
                overdubLeft.resize(header.loopLengthFrames);
                overdubRight.resize(header.loopLengthFrames);
                for (uint64_t f = 0; f < header.loopLengthFrames; ++f)
                {
                    if (!readPod(blob, offset, overdubLeft[f]) || !readPod(blob, offset, overdubRight[f]))
                    {
                        return;
                    }
                }
            }
        }

        m_loopStorage.injectRestoredLoad(std::move(left), std::move(right), header.bpm, std::move(timeline),
                                         std::move(overdubLeft), std::move(overdubRight));
    }

    // Toggles playing the pattern currently in the sequencer (the frozen
    // material), mimicking the looper's own Play/Stop. While on, this mutes
    // the base loop, matching the looper's earlier auto-mute-on-freeze
    // behavior, but now under explicit user control.
    void setSeqPlay(const bool value) noexcept
    {
        if (value)
        {
            m_seqPlayPulse.store(true, std::memory_order_relaxed);
        }
    }
    // Clears the sequencer's own audio: drops every frozen track/slice from
    // the library and the current pattern, stops playback. Independent of
    // setClear(), which only clears the looper's own recording.
    void setClearSeq(const bool value) noexcept
    {
        if (value)
        {
            m_clearSeqPulse.store(true, std::memory_order_relaxed);
        }
    }

    // State queries (message thread, for editor labels and the display).
    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return m_hostSync;
    }
    [[nodiscard]] bool isRecording() const noexcept
    {
        return m_recorder.state() == AbacDsp::LooperState::Recording;
    }
    [[nodiscard]] bool isPlaying() const noexcept
    {
        const auto s = m_recorder.state();
        return s == AbacDsp::LooperState::Playing || s == AbacDsp::LooperState::Overdubbing;
    }
    [[nodiscard]] bool isOverdubbing() const noexcept
    {
        return m_recorder.state() == AbacDsp::LooperState::Overdubbing;
    }
    [[nodiscard]] bool hasOverdub() const noexcept
    {
        return m_recorder.hasOverdub();
    }
    [[nodiscard]] bool isArmed() const noexcept
    {
        return m_armed;
    }
    [[nodiscard]] bool isCountingIn() const noexcept
    {
        return m_countingIn;
    }
    [[nodiscard]] const char* getStateLabel() const noexcept
    {
        return m_viewModel.getStateLabel(m_armed, m_countingIn);
    }

    // One bar of the musical signal, downbeat at index 0, for the CircularBarDisplay
    // (fed through the generated default "signal" gauge call). SliceWaveDisplay
    // ignores this and uses getLoopWaveform() instead.
    [[nodiscard]] const std::vector<float>& visualizeWaveData()
    {
        return m_viewModel.visualizeWaveData();
    }

    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return m_viewModel.getSamplesPerBar();
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return m_viewModel.getBarBeats();
    }

    // Angular span of the outer loop ring, in whole bars; see LooperViewModel for details.
    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        return m_viewModel.getOuterRingBars();
    }

    // One frame-length entry per bar of the outer ring; see LooperViewModel for details.
    [[nodiscard]] std::vector<float> getBarFrameLengths() const
    {
        return m_viewModel.getBarFrameLengths();
    }

    // "bar.beat" position, 1-based; see LooperViewModel for details.
    [[nodiscard]] std::string getBarBeatLabel() const
    {
        return m_viewModel.getBarBeatLabel();
    }

    // Recording headroom left in the capture buffer, mm:ss; see LooperViewModel for details.
    [[nodiscard]] std::string getRemainingRecordLabel() const
    {
        return m_viewModel.getRemainingRecordLabel();
    }

    [[nodiscard]] float getBarPhase() const noexcept
    {
        return m_viewModel.getBarPhase();
    }

    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return m_viewModel.getSpectrogramData();
    }

    // Frame position of the spectrogram write head; see LooperViewModel for details.
    [[nodiscard]] size_t getSpectrogramHeadFrames() const noexcept
    {
        return m_viewModel.getSpectrogramHeadFrames();
    }

    // True once the loop is finalized: the regen pass has primed a wrap-seam slice
    // from the loop's own tail, so the display can safely show one extra slice there.
    [[nodiscard]] bool isSpectrogramWrapped() const noexcept
    {
        return !isRecording();
    }

    // Total frames continuously fed into the record spectrogram (idle/armed/
    // counting-in/recording), independent of the current take's own recordedFrames.
    [[nodiscard]] size_t getSpectrogramFedFrames() const noexcept
    {
        return m_spectrogramFedFrames;
    }

    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        return m_seq.subPositions();
    }

    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        return m_viewModel.getPlayheadNormalized();
    }

    // About the live loop's own display, not the frozen tracks below; no-op for now.
    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        return m_viewModel.getSliceBoundaries();
    }

    // Phase 10g minimal accessors, for a future sequencer UI (no editing yet).
    [[nodiscard]] bool isFreezePending() const noexcept
    {
        return m_freezeService.isPending();
    }
    [[nodiscard]] size_t getFrozenTrackCount() const noexcept
    {
        return m_viewModel.getFrozenTrackCount();
    }
    [[nodiscard]] size_t getFrozenSliceCount() const noexcept
    {
        return m_viewModel.getFrozenSliceCount();
    }
    [[nodiscard]] bool isSequencerPlaying() const noexcept
    {
        return m_sequencerPlaying;
    }
    [[nodiscard]] std::vector<AbacDsp::SequencerSliceThumbnail> getSequencerSliceThumbnails() const
    {
        return m_viewModel.getSequencerSliceThumbnails();
    }

    [[nodiscard]] std::vector<float> getSequencerSliceBoundaries() const
    {
        return m_viewModel.getSequencerSliceBoundaries();
    }

    // Tracks the shared BeatSequencer clock regardless of isSequencerPlaying(),
    // so the marker previews trigger timing even before Seq Play is pressed.
    [[nodiscard]] float getSequencerPlayheadNormalized() const noexcept
    {
        return m_viewModel.getSequencerPlayheadNormalized();
    }

    [[nodiscard]] const char* getSequencerStateLabel() const noexcept
    {
        return m_viewModel.getSequencerStateLabel();
    }

    // Exact per-sample loop content and length (mirror LoopRecorder's own
    // accessors). Not used by the UI (which only needs the coarse peak
    // waveform below); exposed for precise verification of the Phase 8b
    // beat-lock placement.
    [[nodiscard]] float rawLoopSample(const size_t frame, const size_t channel) const noexcept
    {
        return m_viewModel.rawLoopSample(frame, channel);
    }
    [[nodiscard]] size_t rawLoopLengthFrames() const noexcept
    {
        return m_viewModel.rawLoopLengthFrames();
    }
    [[nodiscard]] float rawOverdubSample(const size_t frame, const size_t channel) const noexcept
    {
        return m_recorder.overdubSample(frame, channel);
    }

    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        return m_viewModel.getLoopWaveform();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        applyParameters();
        updateTiming();
        m_captureRing.update(in, m_absPos);
        handleTransportActions(in);

        // Click/sequencer are rendered before the recorder runs so the optional
        // click-to-track gain (below) can be mixed into what actually gets captured.
        std::array<float, BlockSize> click{};
        AbacDsp::AudioBuffer<2, BlockSize> seqOut{};
        renderClickAndSequencer(click, seqOut);

        const AbacDsp::AudioBuffer<2, BlockSize> recorderOut = processRecorder(in, click);

        commitPendingActions();
        mixOutputsAndUpdateVisualization(in, click, seqOut, recorderOut, out);

        m_absPos += BlockSize;
        m_suppressNextBarIndexIncrement = false;
    }

  private:
    void applyParameters()
    {
        if (!m_hostSync)
        {
            const float bpm = m_bpm.load(std::memory_order_relaxed);
            if (std::not_equal_to<float>{}(bpm, m_appliedBpm))
            {
                m_timingController.applyTimeSignatureAwareBpm(bpm);
            }
        }
        applyTimeSignatureRequest();
        const int division = m_sliceDivision.load(std::memory_order_relaxed);
        if (division != m_appliedDivision)
        {
            m_seq.setSubdivType(divisionToSubdiv(division));
            m_appliedDivision = division;
        }
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        const float clickRecordVol = m_clickRecordVol.load(std::memory_order_relaxed);
        m_clickRecordGain = (clickRecordVol <= -60.f) ? 0.f : std::pow(10.f, clickRecordVol / 20.f);
        m_loopGain = std::pow(10.f, m_loopVol.load(std::memory_order_relaxed) / 20.f);
        m_recThresholdLinear = std::pow(10.f, m_recThreshold.load(std::memory_order_relaxed) / 20.f);
        m_hostSync = m_hostSyncReq.load(std::memory_order_relaxed);
        m_freeRecord = m_freeRecordReq.load(std::memory_order_relaxed);
        m_recordBars = m_recordBarsReq.load(std::memory_order_relaxed);
        m_autoStopEnabled = m_autoStopEnabledReq.load(std::memory_order_relaxed);
        m_countInBars = m_countInBarsReq.load(std::memory_order_relaxed);
        const float fadeMs = m_fadeMs.load(std::memory_order_relaxed);
        if (std::not_equal_to<float>{}(fadeMs, m_appliedFadeMs))
        {
            m_recorder.setFadeFrames(static_cast<size_t>(fadeMs / 1000.f * sampleRate()));
            m_appliedFadeMs = fadeMs;
        }
    }

    void updateTiming()
    {
        if (m_hostSync)
        {
            m_timingController.syncToHostTransport(hostTransport());
        }
    }

    // Bar-locked stop, threshold-arm crossing, and count-in elapsing all funnel
    // through here; must run before m_seq advances (renderClickAndSequencer)
    // so their own samplesToNearestBar()/tick math stays consistent this block.
    void handleTransportActions(const AbacDsp::AudioBuffer<2, BlockSize>& in)
    {
        m_transportController.handleTransportPulses();
        checkSpectrogramRegenOnRecordingStop();

        // Virtual Record press once the preset bar count is reached (checked as of
        // the previous block's last completed bar); must run before m_seq advances
        // so requestStop()'s samplesToNearestBar() stays consistent.
        if (m_autoStopArmed && isRecording() && !m_pendingStop && m_takeBarIndex >= m_autoStopBarTarget)
        {
            m_autoStopArmed = false;
            m_transportController.requestStop();
        }

        feedRecordSpectrogram(in);

        // Threshold recording: while armed, wait for the input to cross the level
        // before capture actually begins (this block is then recorded too).
        if (m_armed && blockPeak(in) >= m_recThresholdLinear)
        {
            m_armed = false;
            m_transportController.startRecording();
        }
        if (m_countingIn && m_absPos + BlockSize > m_countInEndTickAbs)
        {
            m_countingIn = false;
            m_transportController.startRecording();
        }
    }

    // Live recording just finished: rebuild via the same tail-primed regen a loop
    // LOAD uses (see runSpectrogramRegen()), so this ring gets a wrap-seam slice too.
    void checkSpectrogramRegenOnRecordingStop()
    {
        const bool recording = isRecording();
        if (m_spectrogramWasRecording && !recording && m_recorder.loopLengthFrames() > 0)
        {
            requestSpectrogramRegen();
        }
        m_spectrogramWasRecording = recording;
    }

    void requestSpectrogramRegen()
    {
        const uint64_t regenGen = m_spectrogramRegenRequestGen.load(std::memory_order_relaxed) + 1;
        m_spectrogramRegenRequestGen.store(regenGen, std::memory_order_release);
        m_spectrogramRegenCv.notify_one();
    }

    // Feeds the record spectrogram with the dry input whenever we're not frozen
    // on a finalized loop's image: while idle/armed/counting-in too, not just
    // while isRecording(), so a fresh take's window is already warm at frame 0.
    void feedRecordSpectrogram(const AbacDsp::AudioBuffer<2, BlockSize>& in)
    {
        if (!isRecording() && m_recorder.loopLengthFrames() > 0)
        {
            return;
        }
        std::array<float, BlockSize> inMonoDecimated{};
        size_t decimatedCount = 0;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (m_spectrogramDecimatePhase == 0)
            {
                inMonoDecimated[decimatedCount++] = 0.5f * (in(i, 0) + in(i, 1));
            }
            m_spectrogramDecimatePhase = (m_spectrogramDecimatePhase + 1) % kSpectrogramDecimation;
        }
        // Non-blocking: skip this block if the regen thread holds the feed
        // lock, rather than stall the audio thread.
        bool expected = false;
        if (m_spectrogramFeedLock.compare_exchange_strong(expected, true, std::memory_order_acquire))
        {
            m_recordSpectrogram.processBlock(std::span<const float>{inMonoDecimated.data(), decimatedCount});
            m_spectrogramFeedLock.store(false, std::memory_order_release);
            m_spectrogramFedFrames += BlockSize;
        }
    }

    // Prints the click into what actually gets captured (per m_clickRecordGain),
    // then advances the recorder itself.
    [[nodiscard]] AbacDsp::AudioBuffer<2, BlockSize> processRecorder(const AbacDsp::AudioBuffer<2, BlockSize>& in,
                                                                     const std::array<float, BlockSize>& click)
    {
        AbacDsp::AudioBuffer<2, BlockSize> recIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float printedClick = click[i] * m_clickRecordGain;
            recIn(i, 0) = in(i, 0) + printedClick;
            recIn(i, 1) = in(i, 1) + printedClick;
        }
        AbacDsp::AudioBuffer<2, BlockSize> recorderOut{};
        m_recorder.processBlock(recIn, recorderOut);
        return recorderOut;
    }

    // Absolute-time check, not recordedFrames() >= loopLength: an early
    // start's relocate needs loopLength+startOffset frames, not loopLength.
    void commitPendingActions()
    {
        if (m_pendingStop && m_absPos + BlockSize > m_pendingStopTickAbs)
        {
            m_transportController.commitPendingStop();
        }
        checkFreezeCompletion();
        m_loopStorage.checkSaveCompletion();
        checkLoopLoadCompletion();
    }

    // Dry input, loop, and sequencer all sum here. Toggling Seq Play mutes the
    // loop and lets the sequencer replace its playback instead. Also updates the
    // bar display's visualization buffer from the dry input in the same pass.
    void mixOutputsAndUpdateVisualization(const AbacDsp::AudioBuffer<2, BlockSize>& in,
                                          const std::array<float, BlockSize>& click,
                                          const AbacDsp::AudioBuffer<2, BlockSize>& seqOut,
                                          const AbacDsp::AudioBuffer<2, BlockSize>& recorderOut,
                                          AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_visualWindowSize = std::min(getSamplesPerBar(), m_visualWave.size());
        const float loopGain = m_sequencerPlaying ? 0.f : m_loopGain;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float loopL = recorderOut(i, 0) * loopGain;
            const float loopR = recorderOut(i, 1) * loopGain;
            out(i, 0) = in(i, 0) + loopL + click[i] + seqOut(i, 0);
            out(i, 1) = in(i, 1) + loopR + click[i] + seqOut(i, 1);

            // Feed the bar display with the dry input only (no loop, no click).
            // A noise gate keeps the ring flat on quiet sections: the signed
            // sample passes only while the envelope stays above the threshold.
            const float visSignal = in(i, 0) + in(i, 1);
            m_visEnv = std::max(std::abs(visSignal), m_visEnv * kVisualGateRelease);
            if (m_barPos[i] < m_visualWindowSize)
            {
                m_visualWave[m_barPos[i]] = (m_visEnv >= kVisualGate) ? visSignal : 0.f;
            }
        }
    }

    // Stopped/Empty/Armed/CountingIn: nothing is playing yet, apply right away.
    // Recording: queue it; applyMeterAtBarBoundary() applies it at the next bar
    // so an in-progress bar is never disturbed. Playing/Overdubbing: the request
    // is left pending and simply not looked at again until recording resumes -
    // the loop's own recorded meter timeline drives the clock instead.
    void applyTimeSignatureRequest()
    {
        const int req = std::clamp(m_timeSignatureReq.load(std::memory_order_relaxed), 0,
                                   static_cast<int>(LooperTimingController::kTimeSignatures.size()) - 1);
        if (isRecording())
        {
            m_pendingTimeSignature = req;
        }
        else if (!isPlaying() && !isOverdubbing())
        {
            if (req != m_appliedTimeSignature)
            {
                m_timingController.installTimeSignature(req);
            }
            m_pendingTimeSignature = req;
        }
    }

    // Runs once per bar boundary (renderClickAndSequencer, event.barWrapped).
    // Recording: applies any pending time-signature change and logs it into the
    // take's own meter timeline. Playing/Overdubbing: replays the take's recorded
    // timeline instead of the live control, wrapping every m_finalizedBarCount bars.
    void applyMeterAtBarBoundary()
    {
        if (isPlaying() || isOverdubbing())
        {
            if (m_finalizedBarCount > 0)
            {
                const size_t nextBar = m_seq.barIndex() % m_finalizedBarCount;
                const auto& seg = m_meterTimeline.segmentForBar(nextBar);
                if (seg.beatsPerBar != m_seq.beatsPerBar() || seg.eighthUnit != m_eighthNoteUnit)
                {
                    m_seq.setBeatsPerBar(seg.beatsPerBar);
                    m_eighthNoteUnit = seg.eighthUnit;
                    m_timingController.applyTimeSignatureAwareBpm(m_appliedBpm);
                }
            }
        }
        else if (isRecording())
        {
            // A bar-locked take started via Count-In begins exactly on a bar
            // boundary, so the very wrap that ends count-in can fire in the same
            // block that flips isRecording() true (before that block's own
            // render pass), crediting a bar that was never actually recorded.
            // Suppress exactly that one spurious wrap; see beginBarLockedRecord()
            // and the unconditional clear at the end of processBlock().
            if (m_suppressNextBarIndexIncrement)
            {
                m_suppressNextBarIndexIncrement = false;
                return;
            }
            ++m_takeBarIndex;
            if (m_pendingTimeSignature != m_appliedTimeSignature)
            {
                m_timingController.installTimeSignature(m_pendingTimeSignature);
                const auto& sig = LooperTimingController::kTimeSignatures[static_cast<size_t>(m_appliedTimeSignature)];
                m_meterTimeline.addSegment(m_takeBarIndex, sig.beatsPerBar, sig.eighthUnit);
            }
        }
    }

    // Once the worker's done-generation catches up, extracts here on the audio
    // thread (allocation-free, see SliceLibrary.h).
    void checkFreezeCompletion()
    {
        auto result = m_freezeService.pollCompletion();
        if (!result || result->slices.empty())
        {
            return;
        }
        const size_t track = m_sliceLibrary.extractTrack(m_recorder.loopView(), result->slices, result->thumbnails);
        m_patternBuilder.rebuildForCurrentLoop();
        m_patternBuilder.populateFromTrack(track);
        // Re-prime the engine's own bar index for the new pattern, and
        // realign the timekeeper to the pattern's origin (frame 0 = downbeat).
        m_sequencer.setPattern(&m_pattern);
        m_timingController.resetTimekeeper(true);
    }

    // captureExtraState()/restoreExtraState() binary layout: header, then
    // segmentCount SerializedMeterSegment entries, then loopLengthFrames
    // interleaved (left, right) float32 sample pairs. Native layout only
    // (no cross-machine/endian portability needed: it round-trips within a
    // single host's own saved session).
    static constexpr uint32_t kExtraStateMagic = 0x4C504541; // "AELP"
    static constexpr uint32_t kExtraStateVersion = 2;

    struct ExtraStateHeader
    {
        uint32_t magic{};
        uint32_t version{};
        float sampleRate{};
        float bpm{};
        uint32_t segmentCount{};
        uint64_t loopLengthFrames{};
    };

    struct SerializedMeterSegment
    {
        uint64_t startBar{};
        uint64_t beatsPerBar{};
        bool eighthUnit{};
    };

    template <typename T>
    static void appendPod(std::vector<std::byte>& blob, size_t& offset, const T& value)
    {
        std::memcpy(blob.data() + offset, &value, sizeof(T));
        offset += sizeof(T);
    }

    template <typename T>
    [[nodiscard]] static bool readPod(std::span<const std::byte> blob, size_t& offset, T& value)
    {
        if (offset + sizeof(T) > blob.size())
        {
            return false;
        }
        std::memcpy(&value, blob.data() + offset, sizeof(T));
        offset += sizeof(T);
        return true;
    }

    // Audio thread, polled every block; installs once confirmed (immediately
    // when there was no BPM conflict, or once resolveLoopLoadBpm() was called).
    // No resampling: a loop saved at a different sample rate plays back
    // pitched/timed wrong. Not handled in phase 1.
    void checkLoopLoadCompletion()
    {
        auto result = m_loopStorage.pollLoadCompletion();
        if (!result)
        {
            return;
        }
        m_recorder.loadLoop(result->left, result->right);
        if (result->hasOverdub)
        {
            m_recorder.loadOverdub(result->overdubLeft, result->overdubRight);
        }
        requestSpectrogramRegen();
        m_seq.setBpm(result->resolvedBpm);
        m_appliedBpm = result->resolvedBpm;
        m_bpm.store(result->resolvedBpm, std::memory_order_relaxed);
        if (!result->meterTimeline.empty())
        {
            m_meterTimeline = std::move(result->meterTimeline);
            m_timingController.finalizeMeterTimeline(m_recorder.loopLengthFrames());
        }
        if (result->hasSequencerData)
        {
            m_sliceLibrary.clear();
            for (const auto& track : result->tracks)
            {
                m_sliceLibrary.extractTrack(track.interleaved, track.slices);
            }
            m_pattern = *result->pattern;
            m_sequencer.setPattern(&m_pattern);
        }
    }

    // Runs on m_spectrogramRegenThread only: rebuilds m_recordSpectrogram's image
    // from the just-loaded loop, mirroring the live downmix + decimation. Paces
    // against the shared FFT queue and bails out if superseded mid-run.
    void runSpectrogramRegen(const uint64_t gen)
    {
        const auto startTime = std::chrono::steady_clock::now();
        const auto logElapsed = [startTime](const char* outcome)
        {
            const auto ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
            std::cout << "LooperImpl: spectrogram regen " << outcome << " in " << ms << " ms" << std::endl;
        };

        const size_t frames = m_recorder.loopLengthFrames();
        if (frames == 0)
        {
            m_spectrogramRegenDoneGen.store(gen, std::memory_order_release);
            logElapsed("skipped (empty loop)");
            return;
        }
        const auto loop = m_recorder.loopView();
        constexpr size_t kChannels = decltype(m_recorder)::kChannels;

        while (true)
        {
            bool expected = false;
            if (m_spectrogramFeedLock.compare_exchange_weak(expected, true, std::memory_order_acquire))
            {
                break;
            }
            std::this_thread::yield();
        }
        m_recordSpectrogram.reset();
        m_spectrogramFedFrames = 0; // guarded by the same lock as feedRecordSpectrogram()'s writer
        m_spectrogramFeedLock.store(false, std::memory_order_release);

        const unsigned hop = m_recordSpectrogram.forwardLength();
        if (hop == 0)
        {
            m_spectrogramRegenDoneGen.store(gen, std::memory_order_release);
            logElapsed("skipped (zero hop)");
            return;
        }

        primeSpectrogramWindowFromLoopTail(loop, frames, kChannels);

        std::vector<float> chunk(hop);
        size_t chunkFill = 0;
        size_t decimatePhase = 0;
        for (size_t frame = 0; frame < frames; ++frame)
        {
            if (gen != m_spectrogramRegenRequestGen.load(std::memory_order_acquire))
            {
                logElapsed("aborted (superseded)");
                return; // leave m_spectrogramRegenDoneGen alone
            }
            if (decimatePhase == 0)
            {
                chunk[chunkFill++] = 0.5f * (loop[frame * kChannels] + loop[frame * kChannels + 1]);
                if (chunkFill == hop)
                {
                    while (!m_recordSpectrogram.queueHasRoom())
                    {
                        std::this_thread::yield();
                    }
                    feedSpectrogramRegenChunk(std::span<const float>{chunk});
                    chunkFill = 0;
                }
            }
            decimatePhase = (decimatePhase + 1) % kSpectrogramDecimation;
        }
        if (chunkFill > 0)
        {
            while (!m_recordSpectrogram.queueHasRoom())
            {
                std::this_thread::yield();
            }
            feedSpectrogramRegenChunk(std::span<const float>{chunk.data(), chunkFill});
        }
        m_spectrogramRegenDoneGen.store(gen, std::memory_order_release);
        logElapsed("completed");
    }

    // A looper's loop is a torus: feeds one full FFT window from the loop's own tail
    // before frame 0, completing an extra leading slice for the wrap seam.
    void primeSpectrogramWindowFromLoopTail(std::span<const float> loop, const size_t frames, const size_t kChannels)
    {
        const unsigned fftLen = m_recordSpectrogram.fftLength();
        if (fftLen == 0)
        {
            return;
        }
        const size_t primeRawFrames = std::min(frames, static_cast<size_t>(fftLen) * kSpectrogramDecimation);

        std::vector<float> primeChunk;
        primeChunk.reserve(fftLen);
        size_t phase = 0;
        for (size_t frame = frames - primeRawFrames; frame < frames; ++frame)
        {
            if (phase == 0)
            {
                primeChunk.push_back(0.5f * (loop[frame * kChannels] + loop[frame * kChannels + 1]));
            }
            phase = (phase + 1) % kSpectrogramDecimation;
        }
        if (!primeChunk.empty())
        {
            while (!m_recordSpectrogram.queueHasRoom())
            {
                std::this_thread::yield();
            }
            feedSpectrogramRegenChunk(std::span<const float>{primeChunk});
        }
    }

    // Acquires m_spectrogramFeedLock (spinning; this thread isn't realtime) before
    // feeding, so it never races the live audio-thread producer.
    void feedSpectrogramRegenChunk(std::span<const float> chunk)
    {
        while (true)
        {
            bool expected = false;
            if (m_spectrogramFeedLock.compare_exchange_weak(expected, true, std::memory_order_acquire))
            {
                break;
            }
            std::this_thread::yield();
        }
        m_recordSpectrogram.processBlock(chunk);
        m_spectrogramFeedLock.store(false, std::memory_order_release);
    }

    [[nodiscard]] static float blockPeak(const AbacDsp::AudioBuffer<2, BlockSize>& in) noexcept
    {
        float peak = 0.f;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            peak = std::max(peak, std::max(std::abs(in(i, 0)), std::abs(in(i, 1))));
        }
        return peak;
    }

    // No more free-running master clock: m_seq only advances while the transport is
    // actually doing something (recording, playing back, armed, or counting in), or
    // while the independent sequencer is soloing a frozen pattern. Fully stopped/
    // waiting holds the clock (and the bar display feed) frozen at its last position.
    // Click and sequencer share one m_seq.advance() call per sample (it mutates position).
    void renderClickAndSequencer(std::array<float, BlockSize>& click, AbacDsp::AudioBuffer<2, BlockSize>& seqOut)
    {
        const bool active = isRecording() || isPlaying() || m_armed || m_countingIn;
        const bool transportRunning = active || m_sequencerPlaying;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (!transportRunning)
            {
                continue;
            }
            const auto event = m_seq.advance();
            if (event.barWrapped)
            {
                // May change beatsPerBar/bpm for the bar about to start: read
                // samplesPerBeat fresh below rather than caching it per block.
                applyMeterAtBarBoundary();
            }
            const size_t samplesPerBeat = m_seq.samplesPerBeat();
            m_barPos[i] = event.beatIndexInBar * samplesPerBeat + event.beatSamplePos;
            if (active)
            {
                if (event.beatStart && m_suppressNextClick)
                {
                    m_suppressNextClick = false; // an m_seq.reset() resync, not a real downbeat
                }
                else if (event.beatStart)
                {
                    m_click.trigger(event.beatIndexInBar == 0 ? AbacDsp::ClickAccent::Downbeat
                                                              : AbacDsp::ClickAccent::Beat);
                }
                else if (event.subdivision)
                {
                    m_click.triggerSub();
                }
            }
            click[i] = active ? m_click.step0() : 0.f;

            const auto seqSample = m_sequencer.advanceSample(event, samplesPerBeat);
            seqOut(i, 0) = seqSample[0];
            seqOut(i, 1) = seqSample[1];
        }
    }

    [[nodiscard]] static AbacDsp::SubdivType divisionToSubdiv(const int division) noexcept
    {
        switch (division)
        {
            case 0:
                return AbacDsp::SubdivType::None; // 1/4
            case 1:
                return AbacDsp::SubdivType::Eighth;
            default:
                return AbacDsp::SubdivType::Sixteenth; // 2 = 1/16, 3 = 1/32 approximated
        }
    }

    AbacDsp::LoopRecorder<BlockSize> m_recorder;
    AbacDsp::BeatSequencer m_seq;
    AbacDsp::ClickGenerator m_click;
    AbacDsp::SimpleSpectrogram m_recordSpectrogram;
    size_t m_spectrogramDecimatePhase{0};
    bool m_spectrogramWasRecording{false};
    size_t m_spectrogramFedFrames{0};
    AbacDsp::SliceLibrary m_sliceLibrary;
    AbacDsp::SequencerEngine<> m_sequencer;
    AbacDsp::SequencePattern m_pattern;
    SequencerPatternBuilder<BlockSize> m_patternBuilder;

    std::vector<float> m_visualWave;
    std::vector<float> m_preparedWave;
    std::array<size_t, BlockSize> m_barPos{};
    size_t m_visualWindowSize{0};

    std::atomic<float> m_bpm{120.f};
    std::atomic<float> m_clickVol{-12.f};
    std::atomic<float> m_clickRecordVol{-60.f};
    std::atomic<float> m_loopVol{0.f};
    std::atomic<float> m_recThreshold{-36.f};
    std::atomic<bool> m_threshRecReq{false};
    std::atomic<bool> m_freeRecordReq{false};
    bool m_freeRecord{false};
    std::atomic<int> m_recordBarsReq{4};
    int m_recordBars{4};
    std::atomic<bool> m_autoStopEnabledReq{false};
    bool m_autoStopEnabled{false};
    bool m_autoStopArmed{false}; // preset-bars auto-stop; set in beginBarLockedRecord()
    size_t m_autoStopBarTarget{0};
    std::atomic<int> m_countInBarsReq{0};
    int m_countInBars{0};
    bool m_countingIn{false};
    uint64_t m_countInEndTickAbs{0};
    // bar.beat display offset: count-in bars are numbered ..., -1, 0 leading into
    // bar 1 (the real start of the take). Zero for takes with no count-in.
    int m_countInBarsOffset{0};

    std::atomic<int> m_timeSignatureReq{LooperTimingController::kDefaultTimeSignature};
    int m_pendingTimeSignature{LooperTimingController::kDefaultTimeSignature};
    int m_appliedTimeSignature{LooperTimingController::kDefaultTimeSignature};
    bool m_eighthNoteUnit{false};
    AbacDsp::MeterTimeline m_meterTimeline;
    size_t m_takeBarIndex{0};      // bars elapsed since this take's own start (beginBarLockedRecord)
    size_t m_finalizedBarCount{0}; // total bars in the current loop, for playback timeline wraparound
    // One-shot: set whenever a bar-locked take starts, consumed by the first bar
    // wrap applyMeterAtBarBoundary() sees, unconditionally cleared at the end of
    // processBlock() so it can only ever suppress a wrap in that same block.
    bool m_suppressNextBarIndexIncrement{false};
    std::atomic<float> m_fadeMs{5.f};
    float m_loopGain{1.f};
    float m_clickRecordGain{0.f};
    float m_recThresholdLinear{0.0158f};
    bool m_armed{false};

    // Noise gate for the bar display feed: gate opens above ~-40 dBFS, releases slowly.
    static constexpr float kVisualGate{0.01f};
    static constexpr float kVisualGateRelease{0.9997f};
    float m_visEnv{0.f};
    std::atomic<int> m_sliceMode{0};
    std::atomic<int> m_sliceDivision{1};
    std::atomic<bool> m_hostSyncReq{false};

    std::atomic<bool> m_recordPulse{false};
    std::atomic<bool> m_playPulse{false};
    std::atomic<bool> m_overdubPulse{false};
    std::atomic<bool> m_undoPulse{false};
    std::atomic<bool> m_mixDownPulse{false};
    std::atomic<bool> m_clearPulse{false};

    float m_appliedBpm{120.f};
    int m_appliedDivision{1};
    float m_appliedFadeMs{-1.f};
    bool m_hostSync{false};
    bool m_suppressNextClick{false}; // set alongside every m_seq.reset() resync
    LooperTimingController m_timingController;

    // Bar-locked recording state.
    CaptureRing<BlockSize> m_captureRing;
    uint64_t m_absPos{0}; // free-running sample position, never reset

    bool m_barLockedTake{false};
    long m_startOffset{0}; // signed: tick - trigger (see beginBarLockedRecord)
    uint64_t m_tickAbs{0}; // this take's start tick (s)

    bool m_pendingStop{false};
    uint64_t m_pendingStopTickAbs{0};
    size_t m_pendingStopLoopLength{0};

    // Phase 10g manual freeze.
    std::atomic<bool> m_freezePulse{false};
    FreezeService<BlockSize> m_freezeService;

    // Named loop save/load ("Loops" menu); background WAV+JSON(+MIDI)
    // encode/decode, generation-counter handshake, and its own worker threads.
    LoopStorageService<BlockSize> m_loopStorage;

    // Request/done gens: let getSpectrogramHeadFrames() report 0 until regen
    // catches up, and let a stale run notice it's superseded and bail out.
    std::atomic<uint64_t> m_spectrogramRegenRequestGen{0};
    std::atomic<uint64_t> m_spectrogramRegenDoneGen{0};
    std::mutex m_spectrogramRegenWaitMutex;
    std::condition_variable_any m_spectrogramRegenCv;
    std::jthread m_spectrogramRegenThread;
    // Guards m_recordSpectrogram against concurrent producers: audio thread
    // skips its feed if contended, the regen worker spins to acquire it.
    std::atomic<bool> m_spectrogramFeedLock{false};

    // Play/stop toggle for the pattern currently in m_pattern.
    std::atomic<bool> m_seqPlayPulse{false};
    bool m_sequencerPlaying{false};
    // Clears the frozen slice library + pattern, independent of setClear().
    std::atomic<bool> m_clearSeqPulse{false};

    LooperTransportController<BlockSize> m_transportController;
    LooperViewModel<BlockSize> m_viewModel;
};
