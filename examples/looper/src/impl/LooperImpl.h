#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
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
#include "Sampler/SequencePattern.h"
#include "Sampler/SequencerEngine.h"
#include "Sampler/SliceLibrary.h"

// Traditional-style slicing looper: captures audio, quantizes the loop to whole
// bars, and plays it back locked to a metronome click. The four transport
// controls are momentary pulses that toggle the real state; the editor reads the
// state back for labels, so there is no toggle-vs-state desync. Slicing is not
// performed here; it will be reintroduced as an on-demand sample sequencer layer
// on top of this plain looper.
//
// Phase 8b (see PLAN.md): threshold-armed record start/stop beat-lock to the
// nearest beat tick, within an eighth-note tolerance. The beat clock
// (BeatSequencer) is the single source of truth for timing: nothing in this
// class ever resets or repositions it (host sync excepted -- syncToPpq keeps
// it aligned to the host transport, which is the source of truth in that
// mode); every transport action locks onto wherever the clock already is
// instead. Recording always starts the instant the trigger fires -- nothing
// ever waits for the tick -- a pickup played early is relocated onto the loop
// tail afterward instead of delaying capture; a small always-on capture ring
// covers the case where the tick had already passed (real audio the buffer
// itself couldn't reach). A short real wait past a stop lets it reach forward
// (a trailing note played late) since that audio genuinely hasn't happened
// yet. Outside the tolerance, record start/stop still happen immediately,
// unquantized, but still without moving the clock.
template <size_t BlockSize>
class LooperImpl final : public EffectBase
{
  public:
    static constexpr size_t kBeatsPerBar = 4;
    static constexpr size_t kWaveformPoints = 512;
    // Phase 10g: the on-demand sequencer's step grid (16ths) and the slice
    // library's total pool size (shared across every frozen track).
    static constexpr size_t kSequencerStepsPerBeat = 4;
    static constexpr float kSliceLibrarySeconds = 120.f;

    explicit LooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_recorder(sampleRate)
        , m_seq(sampleRate)
        , m_click(sampleRate)
        , m_sliceLibrary(static_cast<size_t>(sampleRate * kSliceLibrarySeconds))
        , m_sequencer(sampleRate)
        , m_pattern(1, kBeatsPerBar, kSequencerStepsPerBeat)
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
        // Phase 8b: continuous capture ring, sized generously (1s) to comfortably
        // cover the pre-roll/late-gap window even at the slowest supported tempo
        // (50 BPM -> an eighth note is ~150 ms).
        m_ringCapacityFrames = std::max<size_t>(BlockSize, static_cast<size_t>(sampleRate));
        m_captureRing.assign(m_ringCapacityFrames * 2, 0.f);
        m_startPreRoll.assign(m_ringCapacityFrames * 2, 0.f); // only 2*rollFrames actually used

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
    // Loop boundary click-free fade (record-stop wrap seam, and the Phase 8b
    // pre-/post-roll fold edges), milliseconds.
    void setFadeMs(const float value) noexcept
    {
        m_fadeMs.store(value, std::memory_order_relaxed);
    }
    // Not consumed yet: reserved for the future on-demand slice sequencer.
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
    // Manual "freeze": slices the current loop into a new SliceLibrary track
    // (Phase 10g). Not consumed by a sequencer pattern UI yet; see PLAN.md.
    void setFreeze(const bool value) noexcept
    {
        if (value)
        {
            m_freezePulse.store(true, std::memory_order_relaxed);
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
    [[nodiscard]] size_t getActiveSequencerVoices() const noexcept
    {
        return m_sequencer.activeVoiceCount();
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

        m_recorder.setSamplesPerBeat(m_seq.samplesPerBeat());

        // Threshold recording: while armed, wait for the input to cross the level
        // before capture actually begins (this block is then recorded too).
        if (m_armed && blockPeak(in) >= m_recThresholdLinear)
        {
            m_armed = false;
            beginBeatAwareRecord();
        }

        AbacDsp::AudioBuffer<2, BlockSize> recorderOut{};
        m_recorder.processBlock(in, recorderOut);

        // Commit a pending beat-locked stop only after this block's own
        // capture, so the post-roll captured during this block is included.
        if (m_pendingStop && m_pendingStopFinalizeAbs < m_absPos + BlockSize)
        {
            commitPendingStop();
        }
        checkFreezeCompletion();

        m_visualWindowSize = std::min(getSamplesPerBar(), m_visualWave.size());

        std::array<float, BlockSize> click{};
        AbacDsp::AudioBuffer<2, BlockSize> seqOut{};
        renderClickAndSequencer(click, seqOut);

        // Dry input, loop, and sequencer (silent until m_pattern has events) all sum here.
        const float loopGain = m_loopGain;
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

        // A beat-locked stop or a freeze analysis is waiting on its own async
        // completion; drop pulses rather than race a conflicting action against it.
        if (m_pendingStop || m_freezePending)
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

    void clearAll()
    {
        m_armed = false;
        m_pendingStop = false;
        m_beatLockedTake = false;
        m_recorder.clear();
    }

    void finishRecording()
    {
        m_recorder.setSamplesPerBeat(m_seq.samplesPerBeat());
        m_recorder.stopRecord();
        m_beatLockedTake = false;
    }

    void toggleRecord()
    {
        if (isRecording())
        {
            requestStop();
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
        }
    }

    void togglePlay()
    {
        if (isPlaying())
        {
            m_recorder.stop();
        }
        else if (m_recorder.hasLoop())
        {
            // stop() rewound the loop to frame 0; resync the free-running clock
            // to match (the other exception to "never reset it", besides host sync).
            m_seq.reset();
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
            m_freezeDoneGen.store(gen, std::memory_order_release);
            return;
        }
        AbacDsp::Slicer::downmixToMono(loop, loopLen, m_freezeMono);
        const size_t stepFrames = (m_freezeSamplesPerBeat == 0)
                                      ? loopLen
                                      : std::max<size_t>(1, m_freezeSamplesPerBeat / kSequencerStepsPerBeat);
        const auto grid = AbacDsp::Slicer::gridBoundaries(loopLen, stepFrames);
        m_freezeResultSlices = AbacDsp::Slicer::adaptiveTransientSlices(
            m_freezeMono, loopLen, AbacDsp::Slicer::AdaptiveParams{}, AbacDsp::Slicer::TransientParams{}, grid);
        m_freezeDoneGen.store(gen, std::memory_order_release);
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
            m_sliceLibrary.extractTrack(m_recorder.loopView(), m_freezeResultSlices);
            rebuildPatternForCurrentLoop();
        }
    }

    // Pattern length always matches the loop; no events to preserve yet (no UI writes them).
    void rebuildPatternForCurrentLoop()
    {
        const size_t spb = m_seq.samplesPerBeat();
        const size_t loopLen = m_recorder.loopLengthFrames();
        const size_t framesPerBar = spb * kBeatsPerBar;
        const size_t bars = (framesPerBar == 0) ? 1 : std::max<size_t>(1, loopLen / framesPerBar);
        m_pattern = AbacDsp::SequencePattern(bars, kBeatsPerBar, kSequencerStepsPerBeat);
    }

    // Threshold-armed record start (Phase 8b): recording starts immediately,
    // right here, regardless of timing -- nothing ever waits for a tick. If
    // the crossing lands within an eighth note of a beat tick, this take is
    // beat-locked: the tick is stored (and, if the ring buffer has anything
    // useful for the tail fold, snapshotted) so the eventual stop-time finalize
    // can relocate/backfill around it. Outside the tolerance, this is just an
    // immediate, unquantized start -- but the beat clock is the single source
    // of truth throughout: nothing here ever moves it, in or out of tolerance.
    void beginBeatAwareRecord()
    {
        const size_t spb = m_seq.samplesPerBeat();
        const size_t roll = spb / 8;
        const long off = (spb > 0) ? m_seq.samplesToNearestBeat() : 0;
        const bool canLock =
            spb > 0 && roll > 0 && 2 * roll <= m_ringCapacityFrames && static_cast<size_t>(std::abs(off)) <= roll;
        m_beatLockedTake = canLock;
        if (canLock)
        {
            m_rollFrames = roll;
            m_startOffset = off;
            m_tickAbs = m_absPos + static_cast<uint64_t>(off);
            snapshotStartPreRoll(m_tickAbs, m_absPos, roll);
        }
        m_recorder.beginRecord();
    }

    // Record stop: mirrors beginBeatAwareRecord. A non-beat-locked take (or a
    // stop that doesn't land near any tick) falls back to the plain 8a
    // finalize immediately; a beat-locked take waits for the real post-roll to
    // happen before folding it in.
    void requestStop()
    {
        if (!m_beatLockedTake)
        {
            finishRecording();
            return;
        }
        const size_t spb = m_seq.samplesPerBeat();
        const long off = (spb > 0) ? m_seq.samplesToNearestBeat() : 0;
        const bool canLock = spb > 0 && static_cast<size_t>(std::abs(off)) <= m_rollFrames;
        if (!canLock)
        {
            m_beatLockedTake = false;
            finishRecording();
            return;
        }
        const uint64_t stopTickAbs = m_absPos + static_cast<uint64_t>(off);
        if (stopTickAbs <= m_tickAbs)
        {
            // Degenerate near-instant take: nothing sensible to fold.
            m_beatLockedTake = false;
            finishRecording();
            return;
        }
        m_pendingStop = true;
        m_pendingStopLoopLength = static_cast<size_t>(stopTickAbs - m_tickAbs);
        m_pendingStopFinalizeAbs = stopTickAbs + m_rollFrames;
    }

    void commitPendingStop()
    {
        m_pendingStop = false;
        const std::span<const float> preRoll{m_startPreRoll.data(), m_preRollLen * 2};
        // How far real time has already moved past the stop tick by the time this
        // actually commits (post-roll wait plus block-boundary slop); playback
        // must resume from this offset, not frame 0, to stay phase-locked to the beat.
        const uint64_t stopTickAbs = m_pendingStopFinalizeAbs - static_cast<uint64_t>(m_rollFrames);
        const auto catchUpFrames = static_cast<size_t>(m_absPos + BlockSize - stopTickAbs);
        m_recorder.stopRecordBeatLocked(m_pendingStopLoopLength, preRoll, m_startOffset, m_rollFrames, catchUpFrames);
        m_beatLockedTake = false;
    }

    // Writes this block's raw input into the always-on capture ring
    // (independent of recorder state), so a beat-locked start can reach back
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

    // Snapshots [tickAbs-rollFrames, trigAbs) from the capture ring into
    // m_startPreRoll (see PLAN.md Phase 8b), taken immediately at record-start
    // since the ring buffer's history is bounded and won't still hold this by
    // the time the take finalizes. Length is rollFrames-startOffset: the part
    // of the fixed roll window this take's own capture (starting at trigAbs)
    // cannot reach itself -- zero if the tick was already rollFrames behind
    // trigAbs (an early start right at the tolerance edge), up to 2*rollFrames
    // if the tick was still rollFrames ahead (a late start at the edge).
    void snapshotStartPreRoll(const uint64_t tickAbs, const uint64_t trigAbs, const size_t rollFrames) noexcept
    {
        const uint64_t fromAbs = (tickAbs >= rollFrames) ? tickAbs - rollFrames : 0;
        const uint64_t toAbs = std::max(fromAbs, trigAbs);
        m_preRollLen = std::min(m_startPreRoll.size() / 2, static_cast<size_t>(toAbs - fromAbs));
        for (size_t f = 0; f < m_preRollLen; ++f)
        {
            const auto ringPos = static_cast<size_t>((fromAbs + f) % m_ringCapacityFrames);
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
        const bool active = isRecording() || isPlaying() || m_armed;
        const size_t samplesPerBeat = m_seq.samplesPerBeat();
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto event = m_seq.advance();
            m_barPos[i] = event.beatIndexInBar * samplesPerBeat + event.beatSamplePos;
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

            const auto seqSample = m_sequencer.advanceSample(event, samplesPerBeat);
            seqOut(i, 0) = seqSample[0];
            seqOut(i, 1) = seqSample[1];
        }
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

    AbacDsp::LoopRecorder<BlockSize> m_recorder;
    AbacDsp::BeatSequencer m_seq;
    AbacDsp::ClickGenerator m_click;
    AbacDsp::SimpleSpectrogram m_recordSpectrogram;
    AbacDsp::SliceLibrary m_sliceLibrary;
    AbacDsp::SequencerEngine<> m_sequencer;
    AbacDsp::SequencePattern m_pattern;

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
    std::atomic<float> m_fadeMs{5.f};
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
    float m_appliedFadeMs{-1.f};
    bool m_hostSync{false};
    uint64_t m_lastSyncedUpdateCount{0};

    // Phase 8b: beat-locked recording state.
    std::vector<float> m_captureRing; // always-on raw input capture, interleaved stereo
    size_t m_ringCapacityFrames{0};
    uint64_t m_absPos{0}; // free-running sample position, never reset

    std::vector<float> m_startPreRoll; // snapshot taken immediately when a beat-locked take starts
    size_t m_preRollLen{0};            // valid length within m_startPreRoll (rollFrames - startOffset)
    bool m_beatLockedTake{false};
    size_t m_rollFrames{0}; // fixed for the take's whole lifetime (start's spb/8)
    long m_startOffset{0};  // signed: tick - trigger (see beginBeatAwareRecord)
    uint64_t m_tickAbs{0};  // this take's start tick (s)

    bool m_pendingStop{false};
    uint64_t m_pendingStopFinalizeAbs{0};
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
    std::mutex m_freezeWaitMutex;                     // guards only the worker's own condvar wait
    std::condition_variable_any m_freezeCv;
    std::jthread m_freezeThread;
};
