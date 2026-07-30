#pragma once

#include <algorithm>
#include <cstddef>

#include "Generators/BeatSequencer.h"
#include "Sampler/LoopRecorder.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SliceLibrary.h"

// Builds/updates the sequencer pattern from a just-frozen track, keeping the
// slice-library-to-pattern mapping in one place.
template <size_t BlockSize>
class SequencerPatternBuilder
{
  public:
    SequencerPatternBuilder(const AbacDsp::BeatSequencer& seq, const AbacDsp::LoopRecorder<BlockSize>& recorder,
                            const AbacDsp::SliceLibrary& sliceLibrary, AbacDsp::SequencePattern& pattern)
        : m_seq(seq)
        , m_recorder(recorder)
        , m_sliceLibrary(sliceLibrary)
        , m_pattern(pattern)
    {
    }

    // Sample-accurate steps (stepsPerBeat = samplesPerBeat), so every slice's
    // own startFrame is directly a valid step position: no re-quantizing. Uses
    // the loop's currently-applied meter as a single grid for the whole pattern
    // (the freeze/sequencer feature doesn't follow a mixed-meter timeline).
    void rebuildForCurrentLoop()
    {
        const size_t spb = m_seq.samplesPerBeat();
        const size_t beatsPerBar = m_seq.beatsPerBar();
        const size_t loopLen = m_recorder.loopLengthFrames();
        const size_t framesPerBar = spb * beatsPerBar;
        const size_t bars = (framesPerBar == 0) ? 1 : std::max<size_t>(1, loopLen / framesPerBar);
        m_pattern = AbacDsp::SequencePattern(bars, beatsPerBar, std::max<size_t>(1, spb));
    }

    // Reconstructs the track's slices at their own original positions, gain
    // set to each slice's peak (cancels the engine's own peak-normalize) so
    // this reproduces the original recording exactly, not a normalized mix.
    void populateFromTrack(const size_t track)
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

  private:
    const AbacDsp::BeatSequencer& m_seq;
    const AbacDsp::LoopRecorder<BlockSize>& m_recorder;
    const AbacDsp::SliceLibrary& m_sliceLibrary;
    AbacDsp::SequencePattern& m_pattern;
};
