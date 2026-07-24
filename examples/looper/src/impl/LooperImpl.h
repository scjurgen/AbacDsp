#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <thread>
#include <vector>

#include "Analysis/Slicer.h"
#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
#include "Sampler/LoopRecorder.h"
#include "Sampler/SlicePlayer.h"

// Slicing looper: captures audio, quantizes the loop to whole bars, slices it
// (grid or transient), and replays the slices locked to the loop position with a
// metronome click. The four transport controls are momentary pulses that toggle
// the real state; the editor reads the state back for labels, so there is no
// toggle-vs-state desync.
template <size_t BlockSize>
class LooperImpl final : public EffectBase
{
  public:
    static constexpr size_t kBeatsPerBar = 4;
    static constexpr size_t kWaveformPoints = 512;

    explicit LooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_recorder(sampleRate)
        , m_slicePlayer(sampleRate)
        , m_seq(sampleRate)
        , m_click(sampleRate)
    {
        m_seq.setBeatsPerBar(kBeatsPerBar);
        m_seq.setBpm(m_appliedBpm);
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        m_slicePlayer.setFadeMs(3.f);
        m_workerMono.reserve(static_cast<size_t>(sampleRate) * 4);
        // One bar (4 beats) at the lowest tempo (50 BPM) is ~4.8 s; size generously.
        m_visualWave.assign(static_cast<size_t>(sampleRate * 5.f) + 16, 0.f);
        m_preparedWave.reserve(m_visualWave.size());
        m_recordSpectrogram.setSampleRate(sampleRate);
        // Cover the whole recordable span (~60 s) so a long loop's ring is fully
        // painted, not just its tail. hop = fftLength * windowForwardRatio (1024/3).
        m_recordSpectrogram.setSlices(static_cast<size_t>(60.f * sampleRate / (1024.f / 3.f)) + 64);
        m_sliceWorker = std::jthread([this](std::stop_token st) { sliceWorker(st); });
    }

    // Parameter setters (message thread): store into atomics, apply on the audio thread.
    void setBpm(const float value) noexcept
    {
        m_bpm.store(value, std::memory_order_relaxed);
    }
    void setSwing(const float value) noexcept
    {
        m_swing.store(value, std::memory_order_relaxed);
    }
    void setClickVolume(const float value) noexcept
    {
        m_clickVol.store(value, std::memory_order_relaxed);
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
    void setSliceMode(const int value) noexcept
    {
        m_sliceMode.store(value, std::memory_order_relaxed);
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
    [[nodiscard]] const char* getStateLabel() const noexcept
    {
        if (m_armed)
        {
            return "Armed";
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

    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        std::vector<float> normalized;
        const size_t len = m_recorder.loopLengthFrames();
        if (len == 0)
        {
            return normalized;
        }
        const std::vector<AbacDsp::Slice>& slices = m_frontSet->loopSlices;
        normalized.reserve(slices.size());
        for (const AbacDsp::Slice& slice : slices)
        {
            normalized.push_back(static_cast<float>(slice.startFrame) / static_cast<float>(len));
        }
        return normalized;
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
        handleTransportPulses();

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

        m_recorder.setSamplesPerBar(m_seq.samplesPerBeat() * m_seq.beatsPerBar());

        // Threshold recording: while armed, wait for the input to cross the level
        // before capture actually begins (this block is then recorded too).
        if (m_armed && blockPeak(in) >= m_recThresholdLinear)
        {
            m_armed = false;
            m_recorder.beginRecord();
            m_seq.reset();
        }

        AbacDsp::AudioBuffer<2, BlockSize> recorderOut{};
        m_recorder.processBlock(in, recorderOut);

        tryConsumeSlices();

        AbacDsp::AudioBuffer<2, BlockSize> sliceOut{};
        renderSlices(sliceOut);

        m_visualWindowSize = std::min(getSamplesPerBar(), m_visualWave.size());

        std::array<float, BlockSize> click{};
        renderClick(click);

        // Dry input is always monitored; the loop plays back at its own volume.
        // The raw recorder output (sequential loop playback) is used while
        // overdubbing (slice voices would comb the live input back in) and while a
        // fresh slice extraction is still running on the worker (fallback until the
        // bank is published), otherwise the beat-locked slice voices are used.
        const bool useRawLoop = isOverdubbing() || !m_slicesReady;
        const float loopGain = m_loopGain;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float loopL = (useRawLoop ? recorderOut(i, 0) : sliceOut(i, 0)) * loopGain;
            const float loopR = (useRawLoop ? recorderOut(i, 1) : sliceOut(i, 1)) * loopGain;
            out(i, 0) = in(i, 0) + loopL + click[i];
            out(i, 1) = in(i, 1) + loopR + click[i];
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
        const float swing = m_swing.load(std::memory_order_relaxed);
        if (std::not_equal_to<float>{}(swing, m_appliedSwing))
        {
            m_seq.setSwingRatio(swingPercentToRatio(swing));
            m_appliedSwing = swing;
        }
        const int division = m_sliceDivision.load(std::memory_order_relaxed);
        if (division != m_appliedDivision)
        {
            m_seq.setSubdivType(divisionToSubdiv(division));
            m_appliedDivision = division;
        }
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        m_loopGain = std::pow(10.f, m_loopVol.load(std::memory_order_relaxed) / 20.f);
        m_recThresholdLinear = std::pow(10.f, m_recThreshold.load(std::memory_order_relaxed) / 20.f);
        m_hostSync = m_hostSyncReq.load(std::memory_order_relaxed);
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

        // While a slice extraction is in flight the worker is reading the loop
        // buffer, so anything that would rewrite or drop it (record/overdub/clear)
        // is ignored for the ~ms it takes; play/stop stay live.
        if (m_pendingSlice)
        {
            if (playReq)
            {
                togglePlay();
            }
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
    }

    void clearAll()
    {
        m_armed = false;
        m_recorder.clear();
        m_frontSet->loopSlices.clear();
        m_slicePlayer.reset();
        m_slicePlayer.setLoop({}, 0);
        m_slicesReady = false;
        m_lastSliceIndex = kNoSlice;
    }

    // requestSlices == false when punching straight into overdub: the buffer is
    // about to be written again, so slicing waits until overdub ends.
    void finishRecording(const bool requestSlices = true)
    {
        m_recorder.setSamplesPerBar(m_seq.samplesPerBeat() * m_seq.beatsPerBar());
        m_recorder.stopRecord();
        m_seq.reset();
        m_lastSliceIndex = kNoSlice;
        if (requestSlices)
        {
            requestSlice();
        }
    }

    void toggleRecord()
    {
        if (isRecording())
        {
            finishRecording();
        }
        else if (m_armed)
        {
            m_armed = false; // pressing Record again while armed disarms
        }
        else if (m_threshRecReq.load(std::memory_order_relaxed))
        {
            m_armed = true; // wait for the input to cross the threshold
        }
        else
        {
            m_recorder.beginRecord();
            m_seq.reset();
        }
    }

    void togglePlay()
    {
        if (isPlaying())
        {
            m_recorder.stop();
            m_slicePlayer.reset();
            m_lastSliceIndex = kNoSlice;
        }
        else if (m_recorder.hasLoop())
        {
            m_recorder.play();
            m_seq.reset();
            m_lastSliceIndex = kNoSlice;
        }
    }

    void toggleOverdub()
    {
        if (isRecording())
        {
            // Punch straight from recording into overdub: finish the take, then
            // immediately start layering onto it. Slicing waits until overdub ends.
            finishRecording(false);
            enterOverdub();
        }
        else if (isOverdubbing())
        {
            m_recorder.endOverdub();
            requestSlice();
        }
        else if (isPlaying())
        {
            enterOverdub();
        }
    }

    // Slices play in place from the loop buffer, so stop any ringing voices before
    // overdub writes into it (their output is discarded during overdub anyway).
    void enterOverdub()
    {
        m_recorder.beginOverdub();
        m_slicePlayer.reset();
        m_lastSliceIndex = kNoSlice;
    }

    // Slice triggering is position-driven: whenever the loop playhead enters a new
    // slice, that slice is launched. Identity order for now; shuffle/reverse later.
    // The playhead runs in loop space; loopSlices carry the loop-space boundaries,
    // while the SlicePlayer reads the matching extracted audio from the front bank.
    void renderSlices(AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        const std::vector<AbacDsp::Slice>& loopSlices = m_frontSet->loopSlices;
        if (m_slicesReady && isPlaying() && m_recorder.hasLoop() && !loopSlices.empty())
        {
            const size_t sliceIndex = sliceIndexAt(m_recorder.playPositionFrames());
            if (sliceIndex != m_lastSliceIndex && sliceIndex != kNoSlice)
            {
                m_slicePlayer.triggerSlice(sliceIndex, loopSlices[sliceIndex].lengthFrames);
                m_lastSliceIndex = sliceIndex;
            }
        }
        m_slicePlayer.processBlock(out);
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

    void renderClick(std::array<float, BlockSize>& click)
    {
        const bool active = isRecording() || isPlaying() || m_armed;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto event = m_seq.advance();
            m_barPos[i] = event.beatIndexInBar * m_seq.samplesPerBeat() + event.beatSamplePos;
            if (active)
            {
                if (event.beatStart)
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
        }
    }

    // Audio thread: hand the finalized loop to the worker to slice + extract, and
    // fall back to raw loop playback until the new bank is published.
    void requestSlice()
    {
        m_reqLoopLen = m_recorder.loopLengthFrames();
        m_reqSliceMode = m_sliceMode.load(std::memory_order_relaxed);
        m_reqDivision = m_appliedDivision;
        m_reqSpb = std::max<size_t>(1, m_seq.samplesPerBeat());
        m_slicesReady = false;
        m_pendingSlice = true;
        ++m_pendingReqGen;
        m_sliceReqGen.store(m_pendingReqGen, std::memory_order_release);
    }

    // Audio thread: once the worker has filled the back set, swap it in at a loop
    // boundary (or immediately if playback has not started) so slice playback does
    // not jump mid-slice.
    void tryConsumeSlices()
    {
        if (!m_pendingSlice || m_sliceDoneGen.load(std::memory_order_acquire) != m_pendingReqGen)
        {
            return;
        }
        const bool atLoopStart = !isPlaying() || m_recorder.playPositionFrames() < BlockSize;
        if (!atLoopStart)
        {
            return;
        }
        std::swap(m_frontSet, m_backSet);
        m_slicePlayer.setLoop(m_recorder.loopView(), m_recorder.loopLengthFrames());
        m_slicePlayer.setSlices(m_frontSet->loopSlices);
        m_slicePlayer.reset();
        m_lastSliceIndex = kNoSlice;
        m_slicesReady = true;
        m_pendingSlice = false;
    }

    void sliceWorker(std::stop_token stopToken)
    {
        uint64_t handled = 0;
        while (!stopToken.stop_requested())
        {
            const uint64_t req = m_sliceReqGen.load(std::memory_order_acquire);
            if (req == handled)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            computeSlicesIntoBack();
            m_sliceDoneGen.store(req, std::memory_order_release);
            handled = req;
        }
    }

    // Worker thread: slice the (immutable while pending) loop into the back set's
    // marker table. The back set is not read by the audio thread until the done
    // handshake is observed and the sets are swapped.
    void computeSlicesIntoBack()
    {
        SliceSet& set = *m_backSet;
        set.loopSlices.clear();
        const size_t len = m_reqLoopLen;
        if (len == 0)
        {
            return;
        }
        const size_t divisionsPerBeat = divisionsPerBeatFor(m_reqDivision);
        const size_t spb = std::max<size_t>(1, m_reqSpb);
        const size_t beats = std::max<size_t>(1, len / spb);
        const size_t sliceCount =
            std::clamp<size_t>(beats * divisionsPerBeat, 1, AbacDsp::SlicePlayer<BlockSize>::kMaxSlices);

        if (m_reqSliceMode == 0)
        {
            set.loopSlices = AbacDsp::Slicer::gridSlices(len, sliceCount);
        }
        else
        {
            AbacDsp::Slicer::downmixToMono(m_recorder.loopView(), len, m_workerMono);
            AbacDsp::Slicer::SpectralParams sp{};
            sp.fftSize = 1024;
            sp.hopSize = 256;
            sp.relativeThreshold = 0.3f;
            sp.minGapFrames = spb / (divisionsPerBeat * 2 + 1);
            AbacDsp::Slicer::TransientParams snap{};
            snap.snapMaxDistance = spb / divisionsPerBeat / 2;
            snap.zeroCrossRadius = 64;
            const std::vector<size_t> grid = AbacDsp::Slicer::gridBoundaries(len, len / sliceCount);
            set.loopSlices = AbacDsp::Slicer::spectralTransientSlices(m_workerMono, len, sp, snap, grid);
        }
        // Keep within the reserved capacity so the audio-thread setSlices() at swap
        // never allocates (spectral onsets are otherwise unbounded).
        if (set.loopSlices.size() > AbacDsp::SlicePlayer<BlockSize>::kMaxSlices)
        {
            set.loopSlices.resize(AbacDsp::SlicePlayer<BlockSize>::kMaxSlices);
        }
    }

    [[nodiscard]] size_t sliceIndexAt(const size_t position) const noexcept
    {
        const std::vector<AbacDsp::Slice>& loopSlices = m_frontSet->loopSlices;
        for (size_t i = 0; i < loopSlices.size(); ++i)
        {
            const AbacDsp::Slice& slice = loopSlices[i];
            if (position >= slice.startFrame && position < slice.startFrame + slice.lengthFrames)
            {
                return i;
            }
        }
        return kNoSlice;
    }

    [[nodiscard]] static float swingPercentToRatio(const float percent) noexcept
    {
        return std::clamp(1.f + (percent - 50.f) / 50.f, 0.5f, 2.5f);
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

    [[nodiscard]] static size_t divisionsPerBeatFor(const int division) noexcept
    {
        switch (division)
        {
            case 0:
                return 1; // 1/4
            case 1:
                return 2; // 1/8
            case 2:
                return 4; // 1/16
            default:
                return 8; // 1/32
        }
    }

    static constexpr size_t kNoSlice = static_cast<size_t>(-1);

    // A published slice result: the loop-space start/length markers into the
    // (immutable) loop buffer. Double-buffered: the worker fills the back set, the
    // audio thread reads the front set, and they are swapped at a loop boundary.
    // Slices are played in place from the loop buffer (no extracted copies); a real
    // copy is only needed later for destructive per-slice work (pitch-shift).
    struct SliceSet
    {
        SliceSet()
        {
            loopSlices.reserve(AbacDsp::SlicePlayer<BlockSize>::kMaxSlices);
        }
        std::vector<AbacDsp::Slice> loopSlices;
    };

    AbacDsp::LoopRecorder<BlockSize> m_recorder;
    AbacDsp::SlicePlayer<BlockSize> m_slicePlayer;
    AbacDsp::BeatSequencer m_seq;
    AbacDsp::ClickGenerator m_click;
    AbacDsp::SimpleSpectrogram m_recordSpectrogram;

    SliceSet m_sliceSetA;
    SliceSet m_sliceSetB;
    SliceSet* m_frontSet{&m_sliceSetA};
    SliceSet* m_backSet{&m_sliceSetB};
    std::vector<float> m_workerMono;
    bool m_slicesReady{false};
    bool m_pendingSlice{false};
    uint64_t m_pendingReqGen{0};
    std::atomic<uint64_t> m_sliceReqGen{0};
    std::atomic<uint64_t> m_sliceDoneGen{0};
    size_t m_reqLoopLen{0};
    int m_reqSliceMode{0};
    int m_reqDivision{1};
    size_t m_reqSpb{1};

    std::vector<float> m_visualWave;
    std::vector<float> m_preparedWave;
    std::array<size_t, BlockSize> m_barPos{};
    size_t m_visualWindowSize{0};

    std::atomic<float> m_bpm{120.f};
    std::atomic<float> m_swing{50.f};
    std::atomic<float> m_clickVol{-12.f};
    std::atomic<float> m_loopVol{0.f};
    std::atomic<float> m_recThreshold{-36.f};
    std::atomic<bool> m_threshRecReq{false};
    float m_loopGain{1.f};
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
    float m_appliedSwing{50.f};
    int m_appliedDivision{1};
    bool m_hostSync{false};
    uint64_t m_lastSyncedUpdateCount{0};
    size_t m_lastSliceIndex{kNoSlice};

    // Declared last so it is destroyed (stop-requested + joined) first, before the
    // recorder / slice sets it reads are torn down.
    std::jthread m_sliceWorker;
};
