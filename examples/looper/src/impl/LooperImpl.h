#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

#include "AudioFile/SaveWav.h"

#include "Analysis/FftMisc.h"
#include "Analysis/Slicer.h"
#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
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
    static constexpr size_t kBeatsPerBar = 4;
    static constexpr size_t kWaveformPoints = 512;
    // Onset-detection snap grid (16ths) used when slicing a frozen loop; the
    // pattern itself is sample-accurate (see rebuildPatternForCurrentLoop()),
    // not on this grid. Also the slice library's total pool size.
    static constexpr size_t kOnsetSnapStepsPerBeat = 4;
    static constexpr float kSliceLibrarySeconds = 120.f;
    static constexpr size_t kThumbFftLength = 2 * AbacDsp::SliceLibrary::kThumbHeight;

    explicit LooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_recorder(sampleRate)
        , m_seq(sampleRate)
        , m_click(sampleRate)
        , m_sliceLibrary(static_cast<size_t>(sampleRate * kSliceLibrarySeconds))
        , m_sequencer(sampleRate)
        , m_pattern(1, kBeatsPerBar, m_seq.samplesPerBeat())
        , m_freezeFft(kThumbFftLength)
    {
        m_seq.setBeatsPerBar(kBeatsPerBar);
        m_seq.setBpm(m_appliedBpm);
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        // One bar (4 beats) at the lowest tempo (50 BPM) is ~4.8 s; size generously.
        m_visualWave.assign(static_cast<size_t>(sampleRate * 5.f) + 16, 0.f);
        m_preparedWave.reserve(m_visualWave.size());
        m_recordSpectrogram.setSampleRate(sampleRate);
        // Cover the whole recordable span (~60 s) so a long loop's ring is fully
        // painted, not just its tail. hop = fftLength * windowForwardRatio (1024/3).
        m_recordSpectrogram.setSlices(static_cast<size_t>(60.f * sampleRate / (1024.f / 3.f)) + 64);
        // Covers half a bar of late-start backfill at the slowest supported tempo.
        m_ringCapacityFrames = std::max<size_t>(BlockSize, static_cast<size_t>(sampleRate * 8.f));
        m_captureRing.assign(m_ringCapacityFrames * 2, 0.f);
        m_startPreRoll.assign(m_ringCapacityFrames * 2, 0.f); // only the actual gap length is used

        m_sequencer.setLibrary(&m_sliceLibrary);
        m_sequencer.setPattern(&m_pattern);

        // Started last, once every member it touches exists.
        m_freezeThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_freezeWaitMutex);
                    m_freezeCv.wait(lock, stopToken, [this, lastHandled]
                                    { return m_freezeRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_freezeRequestGen.load(std::memory_order_acquire);
                    runFreezeAnalysis(lastHandled);
                }
            });

        m_loopSaveThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_loopSaveWaitMutex);
                    m_loopSaveCv.wait(lock, stopToken, [this, lastHandled]
                                      { return m_loopSaveRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_loopSaveRequestGen.load(std::memory_order_acquire);
                    runSaveLoopAs(lastHandled);
                }
            });

        m_loopLoadThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_loopLoadWaitMutex);
                    m_loopLoadCv.wait(lock, stopToken, [this, lastHandled]
                                      { return m_loopLoadRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_loopLoadRequestGen.load(std::memory_order_acquire);
                    runLoadLoop(lastHandled);
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
    // 0 = manual stop (today's behavior); N = auto-stop after exactly N bars.
    // Only applies to bar-locked takes (ignored while Free Record is active).
    void setRecordBars(const int value) noexcept
    {
        m_recordBarsReq.store(value, std::memory_order_relaxed);
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
        m_loopsDirectory = std::move(dir);
    }

    [[nodiscard]] std::vector<std::string> listLoopNames() const
    {
        std::vector<std::string> names;
        if (m_loopsDirectory.empty() || !std::filesystem::exists(m_loopsDirectory))
        {
            return names;
        }
        for (const auto& entry : std::filesystem::directory_iterator(m_loopsDirectory))
        {
            if (entry.path().extension() == ".wav")
            {
                names.push_back(entry.path().stem().string());
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    bool deleteLoopNamed(const std::string& name)
    {
        if (sanitizeLoopName(name).empty())
        {
            return false;
        }
        std::error_code ec;
        const bool removedWav = std::filesystem::remove(loopWavPath(name), ec);
        std::filesystem::remove(loopJsonPath(name), ec);
        return removedWav;
    }

    bool renameLoopNamed(const std::string& oldName, const std::string& newName)
    {
        if (sanitizeLoopName(oldName).empty() || sanitizeLoopName(newName).empty() || oldName == newName)
        {
            return false;
        }
        std::error_code ec;
        std::filesystem::rename(loopWavPath(oldName), loopWavPath(newName), ec);
        if (ec)
        {
            return false;
        }
        std::filesystem::rename(loopJsonPath(oldName), loopJsonPath(newName), ec);
        return true; // sidecar rename failing isn't fatal; the wav already moved
    }

    // Guarded like requestFreeze(); writes <loopsDirectory>/<name>.wav + .json
    // on a background worker (see runSaveLoopAs()): file I/O is not RT-safe.
    void requestSaveLoopAs(const std::string& name)
    {
        if (m_loopSavePending || isRecording() || isOverdubbing() || m_recorder.loopLengthFrames() == 0 ||
            m_loopsDirectory.empty() || sanitizeLoopName(name).empty())
        {
            return;
        }
        m_loopSaveName = name;
        m_loopSaveRequestedGen = m_loopSaveRequestGen.load(std::memory_order_relaxed) + 1;
        m_loopSavePending = true;
        m_loopSaveRequestGen.store(m_loopSaveRequestedGen, std::memory_order_release);
        m_loopSaveCv.notify_one();
    }

    [[nodiscard]] bool isLoopSavePending() const noexcept
    {
        return m_loopSavePending;
    }

    [[nodiscard]] std::string consumeLastSavedLoopName()
    {
        std::lock_guard<std::mutex> lock(m_lastSavedLoopMutex);
        return std::exchange(m_lastSavedLoopName, std::string{});
    }

    // Reported back to the UI once a load has decoded and compared metadata.
    struct LoopLoadOutcome
    {
        bool attempted{false};
        bool success{false};
        bool hasConflict{false};
        float wavBpm{0.f};
        float jsonBpm{0.f};
    };

    // Reads <loopsDirectory>/<name>.wav + .json on a background worker (see
    // runLoadLoop()); the audio thread installs the result once resolved.
    void requestLoadLoop(const std::string& name)
    {
        if (m_loopLoadPending || m_loopsDirectory.empty() || sanitizeLoopName(name).empty())
        {
            return;
        }
        m_loopLoadName = name;
        m_loopLoadRequestedGen = m_loopLoadRequestGen.load(std::memory_order_relaxed) + 1;
        m_loopLoadPending = true;
        m_loopLoadRequestGen.store(m_loopLoadRequestedGen, std::memory_order_release);
        m_loopLoadCv.notify_one();
    }

    [[nodiscard]] bool isLoopLoadPending() const noexcept
    {
        return m_loopLoadPending;
    }

    [[nodiscard]] LoopLoadOutcome consumeLoopLoadOutcome()
    {
        std::lock_guard<std::mutex> lock(m_loopLoadOutcomeMutex);
        return std::exchange(m_loopLoadOutcome, LoopLoadOutcome{});
    }

    // Picks which BPM becomes the installed loop's tempo after a conflict was
    // reported; a no-op if no load is currently awaiting resolution.
    void resolveLoopLoadBpm(const float bpm)
    {
        if (!m_loopLoadPending || !m_loopLoadNeedsResolve.load(std::memory_order_acquire))
        {
            return;
        }
        m_loopLoadNeedsResolve.store(false, std::memory_order_relaxed);
        m_loopLoadResolvedBpm = bpm;
        m_loopLoadConfirmedGen.store(m_loopLoadRequestedGen, std::memory_order_release);
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
        if (m_armed)
        {
            return "Armed";
        }
        if (m_countingIn)
        {
            return "Counting in";
        }
        switch (m_recorder.state())
        {
            case AbacDsp::LooperState::Empty:
                return "Empty";
            case AbacDsp::LooperState::Recording:
                return "Recording";
            case AbacDsp::LooperState::Playing:
                return "Playing";
            case AbacDsp::LooperState::Overdubbing:
                return "Overdub";
            case AbacDsp::LooperState::Stopped:
                return "Stopped";
        }
        return "";
    }

    // One bar of the musical signal, downbeat at index 0, for the CircularBarDisplay
    // (fed through the generated default "signal" gauge call). SliceWaveDisplay
    // ignores this and uses getLoopWaveform() instead.
    [[nodiscard]] const std::vector<float>& visualizeWaveData()
    {
        m_preparedWave.assign(m_visualWave.begin(),
                              std::next(m_visualWave.begin(), static_cast<std::ptrdiff_t>(m_visualWindowSize)));
        return m_preparedWave;
    }

    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return m_seq.samplesPerBeat() * m_seq.beatsPerBar();
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return static_cast<int>(m_seq.beatsPerBar());
    }

    // Angular span of the outer loop ring, in whole bars. Empty shows one bar so
    // the clock still reads; while recording the ring extends one bar ahead of the
    // playhead so the bar in progress is already drawn full.
    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        const size_t spb = getSamplesPerBar();
        if (spb == 0)
        {
            return 1;
        }
        if (isRecording())
        {
            return static_cast<int>(m_recorder.recordedFrames() / spb) + 1;
        }
        const size_t len = m_recorder.loopLengthFrames();
        return (len > 0) ? static_cast<int>(std::max<size_t>(1, len / spb)) : 1;
    }

    [[nodiscard]] float getBarPhase() const noexcept
    {
        return m_seq.barPhase();
    }

    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return m_recordSpectrogram.getImageSet();
    }

    // Frame position of the spectrogram write head within the ring: the live record
    // position while capturing, the finalized loop length once stopped.
    [[nodiscard]] size_t getSpectrogramHeadFrames() const noexcept
    {
        return isRecording() ? m_recorder.recordedFrames() : m_recorder.loopLengthFrames();
    }

    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        return m_seq.subPositions();
    }

    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        const size_t len = m_recorder.loopLengthFrames();
        return (len == 0) ? 0.f : static_cast<float>(m_recorder.playPositionFrames()) / static_cast<float>(len);
    }

    // About the live loop's own display, not the frozen tracks below; no-op for now.
    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        return {};
    }

    // Phase 10g minimal accessors, for a future sequencer UI (no editing yet).
    [[nodiscard]] bool isFreezePending() const noexcept
    {
        return m_freezePending;
    }
    [[nodiscard]] size_t getFrozenTrackCount() const noexcept
    {
        return m_sliceLibrary.trackCount();
    }
    [[nodiscard]] size_t getFrozenSliceCount() const noexcept
    {
        return m_sliceLibrary.sliceCount();
    }
    [[nodiscard]] bool isSequencerPlaying() const noexcept
    {
        return m_sequencerPlaying;
    }
    // totalSteps() is frame-accurate (stepsPerBeat == samplesPerBeat at freeze
    // time), so it doubles directly as each thumbnail's placement denominator.
    [[nodiscard]] std::vector<AbacDsp::SequencerSliceThumbnail> getSequencerSliceThumbnails() const
    {
        const size_t totalSteps = m_pattern.totalSteps();
        std::vector<AbacDsp::SequencerSliceThumbnail> thumbnails;
        if (totalSteps == 0)
        {
            return thumbnails;
        }
        thumbnails.reserve(m_pattern.eventCount());
        for (const auto& event : m_pattern.events())
        {
            if (event.track >= m_sliceLibrary.trackCount() ||
                event.sliceIndex >= m_sliceLibrary.sliceCountInTrack(event.track))
            {
                continue;
            }
            const auto& info = m_sliceLibrary.sliceInfo(event.track, event.sliceIndex);
            const auto image = m_sliceLibrary.thumbnail(event.track, event.sliceIndex);
            thumbnails.push_back({static_cast<float>(event.stepPosition) / static_cast<float>(totalSteps),
                                  static_cast<float>(info.lengthFrames) / static_cast<float>(totalSteps),
                                  AbacDsp::SliceLibrary::kThumbWidth, AbacDsp::SliceLibrary::kThumbHeight, image.data(),
                                  sampleRate()});
        }
        return thumbnails;
    }

    [[nodiscard]] std::vector<float> getSequencerSliceBoundaries() const
    {
        const size_t totalSteps = m_pattern.totalSteps();
        if (totalSteps == 0)
        {
            return {};
        }
        std::vector<float> boundaries;
        boundaries.reserve(m_pattern.eventCount());
        for (const auto& event : m_pattern.events())
        {
            boundaries.push_back(static_cast<float>(event.stepPosition) / static_cast<float>(totalSteps));
        }
        return boundaries;
    }

    // Tracks the shared BeatSequencer clock regardless of isSequencerPlaying(),
    // so the marker previews trigger timing even before Seq Play is pressed.
    [[nodiscard]] float getSequencerPlayheadNormalized() const noexcept
    {
        const size_t lengthBars = m_pattern.lengthBars();
        if (lengthBars == 0)
        {
            return 0.f;
        }
        const float pos = static_cast<float>(m_sequencer.barIndex()) + m_seq.barPhase();
        return std::clamp(pos / static_cast<float>(lengthBars), 0.f, 1.f);
    }

    [[nodiscard]] const char* getSequencerStateLabel() const noexcept
    {
        if (!m_sequencerPlaying)
        {
            return m_pattern.eventCount() == 0 ? "Seq Empty" : "Seq Stopped";
        }
        return "Seq Playing";
    }

    // Exact per-sample loop content and length (mirror LoopRecorder's own
    // accessors). Not used by the UI (which only needs the coarse peak
    // waveform below); exposed for precise verification of the Phase 8b
    // beat-lock placement.
    [[nodiscard]] float rawLoopSample(const size_t frame, const size_t channel) const noexcept
    {
        return m_recorder.sample(frame, channel);
    }
    [[nodiscard]] size_t rawLoopLengthFrames() const noexcept
    {
        return m_recorder.loopLengthFrames();
    }

    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        std::vector<float> peaks;
        const size_t len = m_recorder.loopLengthFrames();
        if (len == 0)
        {
            return peaks;
        }
        peaks.assign(kWaveformPoints, 0.f);
        const size_t step = std::max<size_t>(1, len / kWaveformPoints);
        for (size_t p = 0; p < kWaveformPoints; ++p)
        {
            const size_t begin = p * len / kWaveformPoints;
            const size_t end = std::min(len, begin + step);
            float peak = 0.f;
            for (size_t f = begin; f < end; ++f)
            {
                const float mono = 0.5f * (m_recorder.sample(f, 0) + m_recorder.sample(f, 1));
                peak = std::max(peak, std::abs(mono));
            }
            peaks[p] = peak;
        }
        return peaks;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        applyParameters();
        if (m_hostSync)
        {
            syncToHostTransport();
        }

        updateCaptureRing(in);

        handleTransportPulses();

        // Virtual Record press once the preset bar count is reached; must run
        // before m_seq advances so samplesToNearestBar() stays consistent.
        if (m_autoStopArmed && isRecording() && !m_pendingStop && m_absPos + BlockSize > m_autoStopTickAbs)
        {
            m_autoStopArmed = false;
            requestStop();
        }

        // Feed the record spectrogram with the dry input while capturing; it freezes
        // (stops advancing) once recording stops, so the last image persists.
        if (isRecording())
        {
            std::array<float, BlockSize> inMono{};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                inMono[i] = 0.5f * (in(i, 0) + in(i, 1));
            }
            m_recordSpectrogram.processBlock(std::span<const float>{inMono});
        }

        // Threshold recording: while armed, wait for the input to cross the level
        // before capture actually begins (this block is then recorded too).
        if (m_armed && blockPeak(in) >= m_recThresholdLinear)
        {
            m_armed = false;
            startRecording();
        }
        if (m_countingIn && m_absPos + BlockSize > m_countInEndTickAbs)
        {
            m_countingIn = false;
            startRecording();
        }

        // Click/sequencer are rendered before the recorder runs so the optional
        // click-to-track gain (below) can be mixed into what actually gets captured.
        std::array<float, BlockSize> click{};
        AbacDsp::AudioBuffer<2, BlockSize> seqOut{};
        renderClickAndSequencer(click, seqOut);

        AbacDsp::AudioBuffer<2, BlockSize> recIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float printedClick = click[i] * m_clickRecordGain;
            recIn(i, 0) = in(i, 0) + printedClick;
            recIn(i, 1) = in(i, 1) + printedClick;
        }

        AbacDsp::AudioBuffer<2, BlockSize> recorderOut{};
        m_recorder.processBlock(recIn, recorderOut);

        // Absolute-time check, not recordedFrames() >= loopLength: an early
        // start's relocate needs loopLength+startOffset frames, not loopLength.
        if (m_pendingStop && m_absPos + BlockSize > m_pendingStopTickAbs)
        {
            commitPendingStop();
        }
        checkFreezeCompletion();
        checkLoopSaveCompletion();
        checkLoopLoadCompletion();

        m_visualWindowSize = std::min(getSamplesPerBar(), m_visualWave.size());

        // Dry input, loop, and sequencer all sum here. Toggling Seq Play mutes
        // the loop and lets the sequencer replace its playback instead.
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

        m_absPos += BlockSize;
    }

  private:
    void applyParameters()
    {
        if (!m_hostSync)
        {
            const float bpm = m_bpm.load(std::memory_order_relaxed);
            if (std::not_equal_to<float>{}(bpm, m_appliedBpm))
            {
                m_seq.setBpm(bpm);
                m_appliedBpm = bpm;
            }
        }
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
        m_countInBars = m_countInBarsReq.load(std::memory_order_relaxed);
        const float fadeMs = m_fadeMs.load(std::memory_order_relaxed);
        if (std::not_equal_to<float>{}(fadeMs, m_appliedFadeMs))
        {
            m_recorder.setFadeFrames(static_cast<size_t>(fadeMs / 1000.f * sampleRate()));
            m_appliedFadeMs = fadeMs;
        }
    }

    void syncToHostTransport()
    {
        const auto& transport = hostTransport();
        if (!transport.isPlaying || transport.updateCount == m_lastSyncedUpdateCount)
        {
            return;
        }
        m_lastSyncedUpdateCount = transport.updateCount;
        const float bpm = std::clamp(static_cast<float>(transport.bpm), 20.f, 999.f);
        m_seq.setBpm(bpm);
        m_appliedBpm = bpm;
        m_seq.syncToPpq(transport.ppqPosition);
    }

    void handleTransportPulses()
    {
        const bool clearReq = m_clearPulse.exchange(false, std::memory_order_relaxed);
        const bool recordReq = m_recordPulse.exchange(false, std::memory_order_relaxed);
        const bool playReq = m_playPulse.exchange(false, std::memory_order_relaxed);
        const bool overdubReq = m_overdubPulse.exchange(false, std::memory_order_relaxed);
        const bool freezeReq = m_freezePulse.exchange(false, std::memory_order_relaxed);
        const bool seqPlayReq = m_seqPlayPulse.exchange(false, std::memory_order_relaxed);
        const bool clearSeqReq = m_clearSeqPulse.exchange(false, std::memory_order_relaxed);

        // The sequencer play/stop toggle and its own Clear only touch
        // sequencer-side state, never the recorder's own transport state, so
        // both are exempt from the guard below.
        if (seqPlayReq)
        {
            toggleSequencerPlayback();
        }
        if (clearSeqReq)
        {
            clearSequencer();
        }

        // A bar-locked stop, a freeze, or a loop save/load is waiting on its own
        // async completion; drop pulses rather than race a conflicting action.
        if (m_pendingStop || m_freezePending || m_loopSavePending || m_loopLoadPending)
        {
            return;
        }

        if (clearReq)
        {
            clearAll();
        }
        if (recordReq)
        {
            toggleRecord();
        }
        if (playReq)
        {
            togglePlay();
        }
        if (overdubReq)
        {
            toggleOverdub();
        }
        if (freezeReq)
        {
            requestFreeze();
        }
    }

    // Clears only the looper's own recording; the frozen slice library, the
    // sequencer's pattern, and its play/stop state are untouched (they
    // persist independently of the base looper's Clear/re-record).
    void clearAll()
    {
        m_armed = false;
        m_countingIn = false;
        m_autoStopArmed = false;
        m_pendingStop = false;
        m_barLockedTake = false;
        m_recorder.clear();
    }

    // Clears only the sequencer's own audio: every frozen track/slice in the
    // library and the current pattern, and stops playback. Independent of
    // clearAll(), which only clears the looper's own recording.
    void clearSequencer()
    {
        m_sliceLibrary.clear();
        m_pattern.clear();
        m_sequencerPlaying = false;
        m_sequencer.setEnabled(false);
    }

    void finishRecording()
    {
        m_recorder.stopRecordFree();
        m_barLockedTake = false;
    }

    void toggleRecord()
    {
        if (isRecording())
        {
            requestStop();
        }
        else if (m_countingIn)
        {
            m_countingIn = false; // pressing Record again while counting in cancels it
        }
        else if (m_armed)
        {
            m_armed = false; // pressing Record again while armed disarms
        }
        else if (m_countInBars > 0)
        {
            beginCountIn();
        }
        else if (m_threshRecReq.load(std::memory_order_relaxed))
        {
            m_armed = true; // wait for the input to cross the threshold
        }
        else
        {
            startRecording();
        }
    }

    // Takes priority over threshold-arming: count-in always auto-starts once
    // m_countInBars bar ticks elapse, no input crossing needed.
    void beginCountIn()
    {
        m_countingIn = true;
        const size_t spb = m_seq.samplesPerBeat();
        const size_t samplesPerBar = spb * kBeatsPerBar;
        long off = (spb > 0) ? m_seq.samplesToNearestBar() : 0;
        if (off <= 0)
        {
            // Nearest tick is behind us, or we're sitting right on one: either
            // way, a full bar of count-in must still elapse before the next one.
            off += static_cast<long>(samplesPerBar);
        }
        m_countInEndTickAbs =
            m_absPos + static_cast<uint64_t>(off) + static_cast<uint64_t>(m_countInBars - 1) * samplesPerBar;
    }

    // Remembers which path this take used, independent of later toggling.
    void startRecording()
    {
        m_barLockedTake = !m_freeRecord;
        if (m_barLockedTake)
        {
            beginBarLockedRecord();
        }
        else
        {
            m_recorder.beginRecord();
        }
    }

    void togglePlay()
    {
        if (isRecording())
        {
            requestStop(); // Play also ends an active recording, same as pressing Record again
        }
        else if (isPlaying())
        {
            m_recorder.stop();
        }
        else if (m_recorder.hasLoop())
        {
            // stop() rewound the loop to frame 0; resync the free-running clock
            // to match (the other exception to "never reset it", besides host sync).
            m_seq.reset();
            m_suppressNextClick = true; // the reset creates an artificial beatStart, not a real one
            m_recorder.play();
        }
    }

    void toggleOverdub()
    {
        if (isRecording())
        {
            // Punch straight from recording into overdub: finalize the base take
            // (it keeps playing) and start summing input into it immediately.
            // Not beat-locked (no time to wait for a post-roll anyway -- the
            // performer is already continuing straight into the overdub).
            finishRecording();
            m_recorder.beginOverdub();
        }
        else if (isOverdubbing())
        {
            m_recorder.endOverdub();
        }
        else if (isPlaying())
        {
            m_recorder.beginOverdub();
        }
    }

    // Starts/stops playback of whatever is currently in m_pattern (populated
    // by the last freeze). processBlock mutes the base loop while this is on.
    void toggleSequencerPlayback()
    {
        m_sequencerPlaying = !m_sequencerPlaying;
        m_sequencer.setEnabled(m_sequencerPlaying);
    }

    // Refused while the loop isn't stable (recording/overdubbing) or empty;
    // just snapshots and bumps the request generation, the worker does the rest.
    void requestFreeze()
    {
        if (m_freezePending || isRecording() || isOverdubbing() || m_recorder.loopLengthFrames() == 0)
        {
            return;
        }
        m_freezeSamplesPerBeat = m_seq.samplesPerBeat();
        m_freezePendingGen = m_freezeRequestGen.load(std::memory_order_relaxed) + 1;
        m_freezePending = true;
        m_freezeRequestGen.store(m_freezePendingGen, std::memory_order_release);
        m_freezeCv.notify_one();
    }

    // Runs on m_freezeThread only: Slicer::adaptiveTransientSlices allocates and
    // runs an STFT, never acceptable on the audio thread.
    void runFreezeAnalysis(const uint64_t gen)
    {
        const auto loop = m_recorder.loopView();
        const size_t loopLen = m_recorder.loopLengthFrames();
        if (loopLen == 0)
        {
            m_freezeResultSlices.clear();
            m_freezeResultThumbnails.clear();
            m_freezeDoneGen.store(gen, std::memory_order_release);
            return;
        }
        AbacDsp::Slicer::downmixToMono(loop, loopLen, m_freezeMono);
        const size_t stepFrames = (m_freezeSamplesPerBeat == 0)
                                      ? loopLen
                                      : std::max<size_t>(1, m_freezeSamplesPerBeat / kOnsetSnapStepsPerBeat);
        const auto grid = AbacDsp::Slicer::gridBoundaries(loopLen, stepFrames);
        m_freezeResultSlices = AbacDsp::Slicer::adaptiveTransientSlices(
            m_freezeMono, loopLen, AbacDsp::Slicer::AdaptiveParams{}, AbacDsp::Slicer::TransientParams{}, grid);
        m_freezeResultThumbnails.assign(m_freezeResultSlices.size() * AbacDsp::SliceLibrary::kThumbFloats, 0.f);
        for (size_t i = 0; i < m_freezeResultSlices.size(); ++i)
        {
            computeSliceThumbnail(m_freezeResultSlices[i],
                                  std::span<float>{m_freezeResultThumbnails}.subspan(
                                      i * AbacDsp::SliceLibrary::kThumbFloats, AbacDsp::SliceLibrary::kThumbFloats));
        }
        m_freezeDoneGen.store(gen, std::memory_order_release);
    }

    // Reads only within the slice's own bounds, so it never bleeds into a
    // neighbouring one. Worker thread only, same as runFreezeAnalysis().
    void computeSliceThumbnail(const AbacDsp::Slice& slice, std::span<float> dst)
    {
        constexpr size_t kThumbWidth = AbacDsp::SliceLibrary::kThumbWidth;
        constexpr size_t kThumbHeight = AbacDsp::SliceLibrary::kThumbHeight;
        const size_t sliceEnd = slice.startFrame + slice.lengthFrames;
        const size_t hop = std::max<size_t>(1, slice.lengthFrames / kThumbWidth);
        std::array<float, kThumbFftLength> frame{};
        std::vector<float> magnitudes(kThumbHeight, 0.f);
        for (size_t t = 0; t < kThumbWidth; ++t)
        {
            frame.fill(0.f);
            const size_t start = slice.startFrame + t * hop;
            for (size_t i = 0; i < kThumbFftLength && start + i < sliceEnd && start + i < m_freezeMono.size(); ++i)
            {
                frame[i] = m_freezeMono[start + i];
            }
            m_freezeFft.compute(std::vector<float>{frame.begin(), frame.end()}, magnitudes);
            std::copy_n(magnitudes.data(), kThumbHeight, dst.data() + t * kThumbHeight);
        }
    }

    // Once the worker's done-generation catches up, extracts here on the audio
    // thread (allocation-free, see SliceLibrary.h).
    void checkFreezeCompletion()
    {
        if (!m_freezePending || m_freezeDoneGen.load(std::memory_order_acquire) != m_freezePendingGen)
        {
            return;
        }
        m_freezePending = false;
        if (!m_freezeResultSlices.empty())
        {
            const size_t track =
                m_sliceLibrary.extractTrack(m_recorder.loopView(), m_freezeResultSlices, m_freezeResultThumbnails);
            rebuildPatternForCurrentLoop();
            populatePatternFromTrack(track);
            // Re-prime the engine's own bar index for the new pattern, and
            // realign the clock to the pattern's origin (frame 0 = downbeat) -
            // the third deliberate exception to "never reset it", with host
            // sync and Stop/Play-resume.
            m_sequencer.setPattern(&m_pattern);
            m_seq.reset();
            m_suppressNextClick = true;
        }
    }

    // Named patches sanitize with juce::String; this is JUCE-free (Impl stays
    // library-agnostic), so filesystem-unsafe characters are stripped by hand.
    [[nodiscard]] static std::string sanitizeLoopName(const std::string& name)
    {
        std::string result;
        for (const char c : name)
        {
            if (std::string_view("/\\:*?\"<>|").find(c) == std::string_view::npos)
            {
                result += c;
            }
        }
        const auto first = result.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            return {};
        }
        const auto last = result.find_last_not_of(" \t");
        return result.substr(first, last - first + 1);
    }

    [[nodiscard]] std::filesystem::path loopWavPath(const std::string& name) const
    {
        return std::filesystem::path(m_loopsDirectory) / (sanitizeLoopName(name) + ".wav");
    }

    [[nodiscard]] std::filesystem::path loopJsonPath(const std::string& name) const
    {
        return std::filesystem::path(m_loopsDirectory) / (sanitizeLoopName(name) + ".json");
    }

    [[nodiscard]] std::filesystem::path loopTrackWavPath(const std::string& name, const size_t track) const
    {
        return std::filesystem::path(m_loopsDirectory) /
               (sanitizeLoopName(name) + "_track" + std::to_string(track) + ".wav");
    }

    // Worker thread only: replays a track's slices via the public per-slice
    // accessor, in order, rather than reaching into the pool directly.
    void extractTrackAudioAndLengths(const size_t track, std::vector<float>& left, std::vector<float>& right,
                                     std::vector<size_t>& sliceLengths) const
    {
        const size_t count = m_sliceLibrary.sliceCountInTrack(track);
        sliceLengths.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& info = m_sliceLibrary.sliceInfo(track, i);
            sliceLengths.push_back(info.lengthFrames);
            for (size_t f = 0; f < info.lengthFrames; ++f)
            {
                left.push_back(m_sliceLibrary.sample(track, i, f, 0));
                right.push_back(m_sliceLibrary.sample(track, i, f, 1));
            }
        }
    }

    // Worker thread only: writes <name>.wav (+ iXML metadata) and <name>.json;
    // if any tracks are frozen, also <name>_track<N>.wav per track.
    void runSaveLoopAs(const uint64_t gen)
    {
        const size_t loopLen = m_recorder.loopLengthFrames();
        if (loopLen > 0)
        {
            std::vector<float> left(loopLen);
            std::vector<float> right(loopLen);
            for (size_t f = 0; f < loopLen; ++f)
            {
                left[f] = m_recorder.sample(f, 0);
                right[f] = m_recorder.sample(f, 1);
            }
            const float samplesPerBeat = sampleRate() * 60.f / m_appliedBpm;
            const float beats = (samplesPerBeat > 0.f) ? static_cast<float>(loopLen) / samplesPerBeat : 0.f;
            const float bars = beats / static_cast<float>(kBeatsPerBar);
            const AbacDsp::LoopMetadata meta{1, m_appliedBpm, bars, beats};
            AbacDsp::LoopFile<nlohmann::json>::saveStereoWav(loopWavPath(m_loopSaveName).string(), left, right,
                                                             sampleRate(), meta);

            nlohmann::json j = meta;
            if (m_sliceLibrary.trackCount() > 0)
            {
                j["pattern"] = m_pattern;
                auto tracksJson = nlohmann::json::array();
                for (size_t t = 0; t < m_sliceLibrary.trackCount(); ++t)
                {
                    std::vector<float> trackLeft;
                    std::vector<float> trackRight;
                    std::vector<size_t> sliceLengths;
                    extractTrackAudioAndLengths(t, trackLeft, trackRight, sliceLengths);
                    const auto trackPath = loopTrackWavPath(m_loopSaveName, t).string();
                    AudioUtility::SaveWav::saveStereoAs(trackPath, trackLeft, trackRight, sampleRate());
                    tracksJson.push_back({{"file", std::filesystem::path(trackPath).filename().string()},
                                          {"sliceLengths", sliceLengths}});
                }
                j["tracks"] = tracksJson;
            }
            std::ofstream jsonOut(loopJsonPath(m_loopSaveName));
            if (jsonOut)
            {
                jsonOut << j.dump(2);
            }
            std::lock_guard<std::mutex> lock(m_lastSavedLoopMutex);
            m_lastSavedLoopName = m_loopSaveName;
        }
        m_loopSaveDoneGen.store(gen, std::memory_order_release);
    }

    // Audio thread, polled every block like checkFreezeCompletion().
    void checkLoopSaveCompletion()
    {
        if (!m_loopSavePending || m_loopSaveDoneGen.load(std::memory_order_acquire) != m_loopSaveRequestedGen)
        {
            return;
        }
        m_loopSavePending = false;
    }

    // Worker thread only. Builds into locals first (throws on malformed data,
    // caught by runLoadLoop()) so a partial failure leaves no scratch state.
    void loadSequencerData(const nlohmann::json& j)
    {
        auto pattern = j.at("pattern").get<AbacDsp::SequencePattern>();
        std::vector<LoopLoadTrackData> tracks;
        for (const auto& trackJson : j.at("tracks"))
        {
            const auto trackPath = std::filesystem::path(m_loopsDirectory) / trackJson.at("file").get<std::string>();
            const auto trackLoaded = AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(trackPath.string());
            const auto sliceLengths = trackJson.at("sliceLengths").get<std::vector<size_t>>();

            LoopLoadTrackData data;
            data.interleaved.resize(trackLoaded.left.size() * 2);
            for (size_t f = 0; f < trackLoaded.left.size(); ++f)
            {
                data.interleaved[f * 2] = trackLoaded.left[f];
                data.interleaved[f * 2 + 1] = trackLoaded.right[f];
            }
            size_t offset = 0;
            for (const auto len : sliceLengths)
            {
                data.slices.push_back({offset, len});
                offset += len;
            }
            tracks.push_back(std::move(data));
        }
        m_loopLoadPattern = std::move(pattern);
        m_loopLoadTracks = std::move(tracks);
        m_loopLoadHasSequencerData = true;
    }

    // Worker thread only: decodes <name>.wav + .json and compares their BPM
    // belief (iXML-embedded vs sidecar) rather than picking one silently.
    void runLoadLoop(const uint64_t gen)
    {
        LoopLoadOutcome outcome;
        outcome.attempted = true;
        m_loopLoadHasSequencerData = false;
        m_loopLoadTracks.clear();
        m_loopLoadPattern.reset();
        const auto loaded = AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(loopWavPath(m_loopLoadName).string());
        if (!loaded.left.empty())
        {
            std::optional<AbacDsp::LoopMetadata> sidecarMeta;
            std::ifstream jsonIn(loopJsonPath(m_loopLoadName));
            if (jsonIn)
            {
                try
                {
                    nlohmann::json j;
                    jsonIn >> j;
                    sidecarMeta = j.get<AbacDsp::LoopMetadata>();
                    if (j.contains("pattern") && j.contains("tracks"))
                    {
                        loadSequencerData(j);
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "LooperImpl: failed to parse " << loopJsonPath(m_loopLoadName) << ": " << e.what()
                              << std::endl;
                }
            }

            m_loopLoadLeft = loaded.left;
            m_loopLoadRight = loaded.right;
            outcome.success = true;

            float resolvedBpm = 120.f;
            bool needsResolve = false;
            if (sidecarMeta && loaded.embeddedMetadata)
            {
                if (std::abs(sidecarMeta->bpm - loaded.embeddedMetadata->bpm) > 0.01f)
                {
                    outcome.hasConflict = true;
                    outcome.wavBpm = loaded.embeddedMetadata->bpm;
                    outcome.jsonBpm = sidecarMeta->bpm;
                    needsResolve = true;
                }
                else
                {
                    resolvedBpm = sidecarMeta->bpm;
                }
            }
            else if (sidecarMeta)
            {
                resolvedBpm = sidecarMeta->bpm;
            }
            else if (loaded.embeddedMetadata)
            {
                resolvedBpm = loaded.embeddedMetadata->bpm;
            }
            m_loopLoadResolvedBpm = resolvedBpm;
            m_loopLoadNeedsResolve.store(needsResolve, std::memory_order_release);
        }

        {
            std::lock_guard<std::mutex> lock(m_loopLoadOutcomeMutex);
            m_loopLoadOutcome = outcome;
        }
        m_loopLoadDoneGen.store(gen, std::memory_order_release);
        if (outcome.success && !outcome.hasConflict)
        {
            m_loopLoadConfirmedGen.store(gen, std::memory_order_release);
        }
    }

    // Audio thread, polled every block; installs once confirmed (immediately
    // when there was no BPM conflict, or once resolveLoopLoadBpm() was called).
    // No resampling: a loop saved at a different sample rate plays back
    // pitched/timed wrong. Not handled in phase 1.
    void checkLoopLoadCompletion()
    {
        if (!m_loopLoadPending || m_loopLoadConfirmedGen.load(std::memory_order_acquire) != m_loopLoadRequestedGen)
        {
            return;
        }
        m_loopLoadPending = false;
        m_recorder.loadLoop(m_loopLoadLeft, m_loopLoadRight);
        m_seq.setBpm(m_loopLoadResolvedBpm);
        m_appliedBpm = m_loopLoadResolvedBpm;
        m_bpm.store(m_loopLoadResolvedBpm, std::memory_order_relaxed);
        if (m_loopLoadHasSequencerData)
        {
            m_sliceLibrary.clear();
            for (const auto& track : m_loopLoadTracks)
            {
                m_sliceLibrary.extractTrack(track.interleaved, track.slices);
            }
            m_pattern = *m_loopLoadPattern;
            m_sequencer.setPattern(&m_pattern);
        }
    }

    // Sample-accurate steps (stepsPerBeat = samplesPerBeat), so every slice's
    // own startFrame is directly a valid step position: no re-quantizing.
    void rebuildPatternForCurrentLoop()
    {
        const size_t spb = m_seq.samplesPerBeat();
        const size_t loopLen = m_recorder.loopLengthFrames();
        const size_t framesPerBar = spb * kBeatsPerBar;
        const size_t bars = (framesPerBar == 0) ? 1 : std::max<size_t>(1, loopLen / framesPerBar);
        m_pattern = AbacDsp::SequencePattern(bars, kBeatsPerBar, std::max<size_t>(1, spb));
    }

    // Reconstructs the track's slices at their own original positions, gain
    // set to each slice's peak (cancels the engine's own peak-normalize) so
    // this reproduces the original recording exactly, not a normalized mix.
    void populatePatternFromTrack(const size_t track)
    {
        const size_t count = m_sliceLibrary.sliceCountInTrack(track);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& info = m_sliceLibrary.sliceInfo(track, i);
            AbacDsp::SequenceEvent event{};
            event.stepPosition = info.startFrame;
            event.track = track;
            event.sliceIndex = i;
            event.gain = info.peak;
            m_pattern.addEvent(event);
        }
    }

    // Recording starts immediately; the nearest bar tick is stored for the
    // stop-time finalize to relocate/backfill around (no tolerance cutoff).
    void beginBarLockedRecord()
    {
        const size_t spb = m_seq.samplesPerBeat();
        const long off = (spb > 0) ? m_seq.samplesToNearestBar() : 0;
        m_startOffset = off;
        m_tickAbs = m_absPos + static_cast<uint64_t>(off);
        snapshotStartPreRoll(m_tickAbs, m_absPos);
        m_recorder.beginRecord();

        const size_t recordBars = recordBarsIndexToCount(m_recordBars);
        m_autoStopArmed = recordBars > 0;
        if (m_autoStopArmed)
        {
            const size_t samplesPerBar = spb * kBeatsPerBar;
            m_autoStopTickAbs = m_tickAbs + static_cast<uint64_t>(recordBars) * samplesPerBar;
        }
    }

    // A bar-locked take locks to the nearest tick; see the pendingStop check
    // in processBlock() for how an ahead-of-us tick gets waited for.
    void requestStop()
    {
        if (!m_barLockedTake)
        {
            finishRecording();
            return;
        }
        const long off = m_seq.samplesToNearestBar();
        const uint64_t stopTickAbs = m_absPos + static_cast<uint64_t>(off);
        if (stopTickAbs <= m_tickAbs)
        {
            // Degenerate near-instant take: nothing sensible to fold.
            m_barLockedTake = false;
            finishRecording();
            return;
        }
        m_pendingStop = true;
        m_pendingStopLoopLength = static_cast<size_t>(stopTickAbs - m_tickAbs);
        m_pendingStopTickAbs = stopTickAbs;
    }

    void commitPendingStop()
    {
        m_pendingStop = false;
        const std::span<const float> preRoll{m_startPreRoll.data(), m_preRollLen * 2};
        // Block-boundary slop past the tick; playback resumes from here, not frame 0.
        const auto catchUpFrames = static_cast<size_t>(m_absPos + BlockSize - m_pendingStopTickAbs);
        m_recorder.stopRecordBarLocked(m_pendingStopLoopLength, preRoll, m_startOffset, catchUpFrames);
        m_barLockedTake = false;
    }

    // Writes this block's raw input into the always-on capture ring
    // (independent of recorder state), so a bar-locked start can reach back
    // to audio that arrived before its own trigger.
    void updateCaptureRing(const AbacDsp::AudioBuffer<2, BlockSize>& in) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const size_t pos = static_cast<size_t>((m_absPos + i) % m_ringCapacityFrames);
            m_captureRing[pos * 2] = in(i, 0);
            m_captureRing[pos * 2 + 1] = in(i, 1);
        }
    }

    // Snapshots the late-start gap [tickAbs, trigAbs); empty if not late.
    void snapshotStartPreRoll(const uint64_t tickAbs, const uint64_t trigAbs) noexcept
    {
        if (trigAbs <= tickAbs)
        {
            m_preRollLen = 0;
            return;
        }
        const uint64_t gap = trigAbs - tickAbs;
        m_preRollLen = std::min(m_startPreRoll.size() / 2, static_cast<size_t>(gap));
        for (size_t f = 0; f < m_preRollLen; ++f)
        {
            const auto ringPos = static_cast<size_t>((tickAbs + f) % m_ringCapacityFrames);
            m_startPreRoll[f * 2] = m_captureRing[ringPos * 2];
            m_startPreRoll[f * 2 + 1] = m_captureRing[ringPos * 2 + 1];
        }
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

    // Click and sequencer share one m_seq.advance() call per sample (it mutates position).
    void renderClickAndSequencer(std::array<float, BlockSize>& click, AbacDsp::AudioBuffer<2, BlockSize>& seqOut)
    {
        const bool active = isRecording() || isPlaying() || m_armed || m_countingIn;
        const size_t samplesPerBeat = m_seq.samplesPerBeat();
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto event = m_seq.advance();
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
            case 2:
                return AbacDsp::SubdivType::Sixteenth;
            default:
                return AbacDsp::SubdivType::Sixteenth; // 1/32 approximated
        }
    }

    // recordBars dropdown index -> bar count ("Manual", 1, 2, 4, 8, 16).
    [[nodiscard]] static size_t recordBarsIndexToCount(const int index) noexcept
    {
        switch (index)
        {
            case 1:
                return 1;
            case 2:
                return 2;
            case 3:
                return 4;
            case 4:
                return 8;
            case 5:
                return 16;
            default:
                return 0; // Manual
        }
    }

    AbacDsp::LoopRecorder<BlockSize> m_recorder;
    AbacDsp::BeatSequencer m_seq;
    AbacDsp::ClickGenerator m_click;
    AbacDsp::SimpleSpectrogram m_recordSpectrogram;
    AbacDsp::SliceLibrary m_sliceLibrary;
    AbacDsp::SequencerEngine<> m_sequencer;
    AbacDsp::SequencePattern m_pattern;
    AbacDsp::HannWindowMagnitudesFft m_freezeFft; // worker-owned, see runFreezeAnalysis()

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
    std::atomic<int> m_recordBarsReq{0};
    int m_recordBars{0};
    bool m_autoStopArmed{false}; // preset-bars auto-stop; set in beginBarLockedRecord()
    uint64_t m_autoStopTickAbs{0};
    std::atomic<int> m_countInBarsReq{0};
    int m_countInBars{0};
    bool m_countingIn{false};
    uint64_t m_countInEndTickAbs{0};
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
    std::atomic<bool> m_clearPulse{false};

    float m_appliedBpm{120.f};
    int m_appliedDivision{1};
    float m_appliedFadeMs{-1.f};
    bool m_hostSync{false};
    bool m_suppressNextClick{false}; // set alongside every m_seq.reset() resync
    uint64_t m_lastSyncedUpdateCount{0};

    // Bar-locked recording state.
    std::vector<float> m_captureRing; // always-on raw input capture, interleaved stereo
    size_t m_ringCapacityFrames{0};
    uint64_t m_absPos{0}; // free-running sample position, never reset

    std::vector<float> m_startPreRoll; // snapshot taken immediately when a bar-locked take starts
    size_t m_preRollLen{0};            // valid length within m_startPreRoll (the late-start gap)
    bool m_barLockedTake{false};
    long m_startOffset{0}; // signed: tick - trigger (see beginBarLockedRecord)
    uint64_t m_tickAbs{0}; // this take's start tick (s)

    bool m_pendingStop{false};
    uint64_t m_pendingStopTickAbs{0};
    size_t m_pendingStopLoopLength{0};

    // Phase 10g manual freeze; only the two generation counters are cross-thread.
    std::atomic<bool> m_freezePulse{false};
    bool m_freezePending{false};
    uint64_t m_freezePendingGen{0};
    size_t m_freezeSamplesPerBeat{0};
    std::atomic<uint64_t> m_freezeRequestGen{0};
    std::atomic<uint64_t> m_freezeDoneGen{0};
    std::vector<float> m_freezeMono;                  // worker-owned scratch
    std::vector<AbacDsp::Slice> m_freezeResultSlices; // worker writes, audio thread reads once done
    std::vector<float> m_freezeResultThumbnails;      // one kThumbFloats block per candidate, same order
    std::mutex m_freezeWaitMutex;                     // guards only the worker's own condvar wait
    std::condition_variable_any m_freezeCv;
    std::jthread m_freezeThread;

    // Named loop save/load ("Loops" menu); same generation-counter handshake
    // as freeze, above, plus a load-side conflict-resolution handoff.
    std::string m_loopsDirectory; // set once via setLoopsDirectory(), before any save/load
    std::string m_loopSaveName;
    bool m_loopSavePending{false};
    uint64_t m_loopSaveRequestedGen{0};
    std::atomic<uint64_t> m_loopSaveRequestGen{0};
    std::atomic<uint64_t> m_loopSaveDoneGen{0};
    std::mutex m_loopSaveWaitMutex;
    std::condition_variable_any m_loopSaveCv;
    std::jthread m_loopSaveThread;
    std::mutex m_lastSavedLoopMutex;
    std::string m_lastSavedLoopName;

    std::string m_loopLoadName;
    bool m_loopLoadPending{false};
    uint64_t m_loopLoadRequestedGen{0};
    std::atomic<uint64_t> m_loopLoadRequestGen{0};
    std::atomic<uint64_t> m_loopLoadDoneGen{0};      // worker finished decode + conflict check
    std::atomic<uint64_t> m_loopLoadConfirmedGen{0}; // safe for the audio thread to install
    std::mutex m_loopLoadWaitMutex;
    std::condition_variable_any m_loopLoadCv;
    std::jthread m_loopLoadThread;
    std::vector<float> m_loopLoadLeft; // worker-owned scratch, audio thread reads once confirmed
    std::vector<float> m_loopLoadRight;
    float m_loopLoadResolvedBpm{120.f};
    std::atomic<bool> m_loopLoadNeedsResolve{false};
    std::mutex m_loopLoadOutcomeMutex;
    LoopLoadOutcome m_loopLoadOutcome;

    // One frozen track's audio (interleaved) plus its slices, already laid
    // out contiguously so extractTrack() reconstructs it with no re-slicing.
    struct LoopLoadTrackData
    {
        std::vector<float> interleaved;
        std::vector<AbacDsp::Slice> slices;
    };
    std::vector<LoopLoadTrackData> m_loopLoadTracks; // worker-owned scratch, same handoff as left/right above
    std::optional<AbacDsp::SequencePattern> m_loopLoadPattern;
    bool m_loopLoadHasSequencerData{false};

    // Play/stop toggle for the pattern currently in m_pattern.
    std::atomic<bool> m_seqPlayPulse{false};
    bool m_sequencerPlaying{false};
    // Clears the frozen slice library + pattern, independent of setClear().
    std::atomic<bool> m_clearSeqPulse{false};
};
