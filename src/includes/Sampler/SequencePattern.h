#pragma once

#include <cstddef>
#include <vector>

namespace AbacDsp
{

struct SequenceEvent
{
    size_t stepPosition{0}; // quantized position within the pattern, in steps
    size_t track{0};
    size_t sliceIndex{0};
    float gain{1.f};
    float pitchRatio{1.f}; // 1.0 = unmodified; resampled read rate, no time-stretch
    bool reverse{false};
    bool randomizeSlice{false};      // when true, sliceIndex is re-picked from `track` at trigger time
    long timingOffsetFrames{0};      // shuffle: fixed displacement off the step's grid position
    float humanizeAmountFrames{0.f}; // humanize: max +/- jitter, re-rolled per pattern repeat
};

// Plain event-list data for a loop-synced step sequence. Pattern length is
// expressed in whole bars (matching the base looper's own bar length so a
// pattern always wraps in sync), subdivided into stepsPerBeat steps per beat.
// No audio, no clock: SequencerEngine converts step positions to
// sample-accurate trigger times against the live BeatSequencer.
class SequencePattern
{
  public:
    SequencePattern(const size_t lengthBars, const size_t beatsPerBar, const size_t stepsPerBeat)
        : m_lengthBars(lengthBars)
        , m_beatsPerBar(beatsPerBar)
        , m_stepsPerBeat(stepsPerBeat)
    {
    }

    [[nodiscard]] size_t lengthBars() const noexcept
    {
        return m_lengthBars;
    }

    [[nodiscard]] size_t beatsPerBar() const noexcept
    {
        return m_beatsPerBar;
    }

    [[nodiscard]] size_t stepsPerBeat() const noexcept
    {
        return m_stepsPerBeat;
    }

    [[nodiscard]] size_t totalSteps() const noexcept
    {
        return m_lengthBars * m_beatsPerBar * m_stepsPerBeat;
    }

    // Appends an event; rejected (returns false, nothing added) if
    // stepPosition falls outside the pattern.
    bool addEvent(const SequenceEvent& event)
    {
        if (event.stepPosition >= totalSteps())
        {
            return false;
        }
        m_events.push_back(event);
        return true;
    }

    void removeEvent(const size_t index) noexcept
    {
        if (index < m_events.size())
        {
            m_events.erase(m_events.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    void clear() noexcept
    {
        m_events.clear();
    }

    [[nodiscard]] size_t eventCount() const noexcept
    {
        return m_events.size();
    }

    [[nodiscard]] const SequenceEvent& event(const size_t index) const noexcept
    {
        return m_events[index];
    }

    [[nodiscard]] SequenceEvent& event(const size_t index) noexcept
    {
        return m_events[index];
    }

    [[nodiscard]] const std::vector<SequenceEvent>& events() const noexcept
    {
        return m_events;
    }

    // Indices (in events()) of every event at exactly this step, in insertion
    // order. A step may carry more than one simultaneous event (layered slices).
    [[nodiscard]] std::vector<size_t> eventIndicesAtStep(const size_t step) const
    {
        std::vector<size_t> indices;
        for (size_t i = 0; i < m_events.size(); ++i)
        {
            if (m_events[i].stepPosition == step)
            {
                indices.push_back(i);
            }
        }
        return indices;
    }

  private:
    size_t m_lengthBars;
    size_t m_beatsPerBar;
    size_t m_stepsPerBeat;
    std::vector<SequenceEvent> m_events;
};

}
