#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

#include "CaptureRing.h"
#include "FreezeService.h"
#include "Generators/BeatSequencer.h"
#include "Generators/MeterTimeline.h"
#include "LoopStorageService.h"
#include "LooperTimingController.h"
#include "Sampler/LoopRecorder.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SequencerEngine.h"
#include "Sampler/SliceLibrary.h"

// Transport pulse dispatch (record/play/overdub/clear/freeze/seq-play/seq-clear)
// plus bar-locked recording: quantized record start/stop, pre-roll capture,
// pending-stop handling, and auto-stop. Everything here runs synchronously on
// the audio thread; there is no worker thread of its own (contrast
// FreezeService/LoopStorageService).
//
// All state referenced here is still owned by LooperImpl (read directly by
// its own public accessors and processBlock()); this controller only holds
// references to it, same as LooperTimingController.
template <size_t BlockSize>
class LooperTransportController
{
  public:
    struct Deps
    {
        AbacDsp::LoopRecorder<BlockSize>& recorder;
        AbacDsp::BeatSequencer& seq;
        LooperTimingController& timing;
        AbacDsp::MeterTimeline& meterTimeline;
        CaptureRing<BlockSize>& captureRing;
        FreezeService<BlockSize>& freezeService;
        LoopStorageService<BlockSize>& loopStorage;
        AbacDsp::SliceLibrary& sliceLibrary;
        AbacDsp::SequencePattern& pattern;
        AbacDsp::SequencerEngine<>& sequencer;

        bool& armed;
        bool& countingIn;
        bool& autoStopArmed;
        size_t& autoStopBarTarget;
        bool& pendingStop;
        uint64_t& pendingStopTickAbs;
        size_t& pendingStopLoopLength;
        bool& barLockedTake;
        long& startOffset;
        uint64_t& tickAbs;
        size_t& takeBarIndex;
        bool& suppressNextBarIndexIncrement;
        bool& sequencerPlaying;
        int& appliedTimeSignature;
        int& pendingTimeSignature;
        size_t& finalizedBarCount;
        uint64_t& absPos;

        bool& freeRecord;
        int& countInBars;
        int& recordBars;
        bool& autoStopEnabled;

        std::atomic<bool>& clearPulse;
        std::atomic<bool>& recordPulse;
        std::atomic<bool>& playPulse;
        std::atomic<bool>& overdubPulse;
        std::atomic<bool>& freezePulse;
        std::atomic<bool>& seqPlayPulse;
        std::atomic<bool>& clearSeqPulse;
        std::atomic<bool>& threshRecReq;
    };

    explicit LooperTransportController(Deps deps)
        : m_deps(deps)
    {
    }

    void handleTransportPulses()
    {
        const bool clearReq = m_deps.clearPulse.exchange(false, std::memory_order_relaxed);
        const bool recordReq = m_deps.recordPulse.exchange(false, std::memory_order_relaxed);
        const bool playReq = m_deps.playPulse.exchange(false, std::memory_order_relaxed);
        const bool overdubReq = m_deps.overdubPulse.exchange(false, std::memory_order_relaxed);
        const bool freezeReq = m_deps.freezePulse.exchange(false, std::memory_order_relaxed);
        const bool seqPlayReq = m_deps.seqPlayPulse.exchange(false, std::memory_order_relaxed);
        const bool clearSeqReq = m_deps.clearSeqPulse.exchange(false, std::memory_order_relaxed);

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
        if (m_deps.pendingStop || m_deps.freezeService.isPending() || m_deps.loopStorage.isSavePending() ||
            m_deps.loopStorage.isLoadPending())
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

    // Remembers which path this take used, independent of later toggling.
    // Also reached directly from processBlock() (threshold-arm crossing,
    // count-in elapsing), not just via toggleRecord().
    void startRecording()
    {
        m_deps.barLockedTake = !m_deps.freeRecord;
        if (m_deps.barLockedTake)
        {
            beginBarLockedRecord();
        }
        else
        {
            // Free Record is unquantized: no bar grid to hang a meter timeline on.
            m_deps.meterTimeline.clear();
            m_deps.finalizedBarCount = 0;
            m_deps.recorder.beginRecord();
        }
    }

    // A bar-locked take locks to the nearest tick; see the pendingStop check
    // in LooperImpl::processBlock() for how an ahead-of-us tick gets waited for.
    void requestStop()
    {
        if (!m_deps.barLockedTake)
        {
            finishRecording();
            return;
        }
        const long off = m_deps.seq.samplesToNearestBar();
        const uint64_t stopTickAbs = m_deps.absPos + static_cast<uint64_t>(off);
        if (stopTickAbs <= m_deps.tickAbs)
        {
            // Degenerate near-instant take: nothing sensible to fold.
            m_deps.barLockedTake = false;
            finishRecording();
            return;
        }
        m_deps.pendingStop = true;
        m_deps.pendingStopLoopLength = static_cast<size_t>(stopTickAbs - m_deps.tickAbs);
        m_deps.pendingStopTickAbs = stopTickAbs;
    }

    void commitPendingStop()
    {
        m_deps.pendingStop = false;
        const std::span<const float> preRoll = m_deps.captureRing.preRoll();
        // Block-boundary slop past the tick; playback resumes from here, not frame 0.
        const auto catchUpFrames = static_cast<size_t>(m_deps.absPos + BlockSize - m_deps.pendingStopTickAbs);
        m_deps.recorder.stopRecordBarLocked(m_deps.pendingStopLoopLength, preRoll, m_deps.startOffset, catchUpFrames);
        m_deps.barLockedTake = false;
        m_deps.timing.finalizeMeterTimeline(m_deps.pendingStopLoopLength);
        // stopRecordBarLocked() auto-transitions straight into playback (no
        // separate Play press): resync here too, not just in togglePlay().
        m_deps.timing.resyncTimekeeperToLoopStart();
    }

  private:
    [[nodiscard]] bool isRecording() const noexcept
    {
        return m_deps.recorder.state() == AbacDsp::LooperState::Recording;
    }

    [[nodiscard]] bool isPlaying() const noexcept
    {
        const auto s = m_deps.recorder.state();
        return s == AbacDsp::LooperState::Playing || s == AbacDsp::LooperState::Overdubbing;
    }

    [[nodiscard]] bool isOverdubbing() const noexcept
    {
        return m_deps.recorder.state() == AbacDsp::LooperState::Overdubbing;
    }

    // Clears only the looper's own recording; the frozen slice library, the
    // sequencer's pattern, and its play/stop state are untouched (they
    // persist independently of the base looper's Clear/re-record).
    void clearAll()
    {
        m_deps.armed = false;
        m_deps.countingIn = false;
        m_deps.autoStopArmed = false;
        m_deps.pendingStop = false;
        m_deps.barLockedTake = false;
        m_deps.recorder.clear();
    }

    // Clears only the sequencer's own audio: every frozen track/slice in the
    // library and the current pattern, and stops playback. Independent of
    // clearAll(), which only clears the looper's own recording.
    void clearSequencer()
    {
        m_deps.sliceLibrary.clear();
        m_deps.pattern.clear();
        m_deps.sequencerPlaying = false;
        m_deps.sequencer.setEnabled(false);
    }

    void finishRecording()
    {
        const bool wasBarLocked = m_deps.barLockedTake;
        m_deps.recorder.stopRecordFree();
        m_deps.barLockedTake = false;
        if (wasBarLocked)
        {
            // A bar-locked take can also end up here (degenerate near-instant
            // stop in requestStop(), or punching straight into overdub): finalize
            // whatever meter timeline it accumulated instead of just discarding it.
            m_deps.timing.finalizeMeterTimeline(m_deps.recorder.loopLengthFrames());
        }
        else
        {
            m_deps.meterTimeline.clear();
            m_deps.finalizedBarCount = 0;
        }
    }

    void toggleRecord()
    {
        if (isRecording())
        {
            requestStop();
        }
        else if (m_deps.countingIn)
        {
            m_deps.countingIn = false; // pressing Record again while counting in cancels it
        }
        else if (m_deps.armed)
        {
            m_deps.armed = false; // pressing Record again while armed disarms
        }
        else if (m_deps.countInBars > 0)
        {
            m_deps.timing.resetTimekeeper(false); // count-in needs its beat 1 click to actually count something
            m_deps.timing.beginCountIn(m_deps.countInBars, m_deps.absPos);
        }
        else if (m_deps.threshRecReq.load(std::memory_order_relaxed))
        {
            m_deps.timing.resetTimekeeper(false); // the performer needs an audible downbeat while waiting to play in
            m_deps.armed = true;                  // wait for the input to cross the threshold
        }
        else
        {
            m_deps.timing.resetTimekeeper(false); // beat 1 of a fresh take is a real downbeat, not a resync artifact
            startRecording();
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
            m_deps.recorder.stop();
        }
        else if (m_deps.recorder.hasLoop())
        {
            // stop() rewound the loop to frame 0; resync the timekeeper to match.
            m_deps.timing.resyncTimekeeperToLoopStart();
            m_deps.recorder.play();
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
            m_deps.recorder.beginOverdub();
        }
        else if (isOverdubbing())
        {
            m_deps.recorder.endOverdub();
        }
        else if (isPlaying())
        {
            m_deps.recorder.beginOverdub();
        }
    }

    // Starts/stops playback of whatever is currently in the pattern (populated
    // by the last freeze). LooperImpl::processBlock() mutes the base loop while
    // this is on.
    void toggleSequencerPlayback()
    {
        m_deps.sequencerPlaying = !m_deps.sequencerPlaying;
        m_deps.sequencer.setEnabled(m_deps.sequencerPlaying);
    }

    // Refused while the loop isn't stable (recording/overdubbing) or empty;
    // just snapshots and bumps the request generation, the worker does the rest.
    void requestFreeze()
    {
        if (isRecording() || isOverdubbing() || m_deps.recorder.loopLengthFrames() == 0)
        {
            return;
        }
        m_deps.freezeService.requestFreeze(m_deps.seq.samplesPerBeat());
    }

    // Recording starts immediately; the nearest bar tick is stored for the
    // stop-time finalize to relocate/backfill around (no tolerance cutoff).
    void beginBarLockedRecord()
    {
        // Force-install the live control's current meter: guarantees the take
        // starts from exactly what the performer dialed in, regardless of
        // whatever meter the sequencer was left at by a previous take/playback cycle.
        m_deps.timing.installTimeSignature(m_deps.pendingTimeSignature);
        m_deps.takeBarIndex = 0;
        m_deps.suppressNextBarIndexIncrement = true;
        m_deps.meterTimeline.clear();
        const auto& sig0 = LooperTimingController::kTimeSignatures[static_cast<size_t>(m_deps.appliedTimeSignature)];
        m_deps.meterTimeline.addSegment(0, sig0.beatsPerBar, sig0.eighthUnit);

        const size_t spb = m_deps.seq.samplesPerBeat();
        const long off = (spb > 0) ? m_deps.seq.samplesToNearestBar() : 0;
        m_deps.startOffset = off;
        m_deps.tickAbs = m_deps.absPos + static_cast<uint64_t>(off);
        m_deps.captureRing.snapshotPreRoll(m_deps.tickAbs, m_deps.absPos);
        m_deps.recorder.beginRecord();

        // A live bar-wrap count (takeBarIndex), not a precomputed sample tick:
        // a precomputed tick would assume a constant bar length for the whole
        // target, which a mid-take meter change can invalidate.
        const size_t recordBars = m_deps.autoStopEnabled ? static_cast<size_t>(m_deps.recordBars) : 0;
        m_deps.autoStopArmed = recordBars > 0;
        m_deps.autoStopBarTarget = recordBars;
    }

    Deps m_deps;
};
