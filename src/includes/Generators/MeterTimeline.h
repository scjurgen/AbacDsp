#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace AbacDsp
{

/// @ingroup generators
/// @brief One time signature and the bar index at which it takes effect.
struct MeterSegment
{
    size_t startBar{0}; ///< 0-based bar index where this meter begins.
    size_t beatsPerBar{4};
    bool eighthUnit{false}; ///< false: beat is a quarter note; true: beat is an eighth.
};

/**
 * @ingroup generators
 * @brief Sequence of time-signature changes, with cumulative frame positions.
 *
 * A single bar length cannot describe a passage whose meter changes partway
 * through, so bar boundaries are accumulated across segments instead of being
 * computed from one constant. That is what lets a position in frames be mapped
 * back to the correct bar and beat when the meter is not uniform.
 *
 * The cumulative table is rebuilt on edit rather than maintained incrementally,
 * so lookups stay a binary search and never depend on edit order.
 */
class MeterTimeline
{
  public:
    void clear() noexcept
    {
        m_segments.clear();
        m_cumulativeFrames.clear();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_segments.empty();
    }

    [[nodiscard]] size_t segmentCount() const noexcept
    {
        return m_segments.size();
    }

    [[nodiscard]] const std::vector<MeterSegment>& segments() const noexcept
    {
        return m_segments;
    }

    // Segments must be appended in increasing startBar order. A segment
    // identical to the previous one (same beatsPerBar/eighthUnit) is dropped,
    // collapsing consecutive duplicates.
    void addSegment(const size_t startBar, const size_t beatsPerBar, const bool eighthUnit)
    {
        if (!m_segments.empty())
        {
            const MeterSegment& last = m_segments.back();
            if (last.beatsPerBar == beatsPerBar && last.eighthUnit == eighthUnit)
            {
                return;
            }
        }
        m_segments.push_back({startBar, beatsPerBar, eighthUnit});
    }

    // The meter in effect at barIndex: the last segment whose startBar <= barIndex.
    // An empty timeline reads as a constant 4/4 default.
    [[nodiscard]] const MeterSegment& segmentForBar(const size_t barIndex) const noexcept
    {
        static constexpr MeterSegment kDefault{};
        if (m_segments.empty())
        {
            return kDefault;
        }
        size_t idx = 0;
        for (size_t i = 0; i < m_segments.size(); ++i)
        {
            if (m_segments[i].startBar > barIndex)
            {
                break;
            }
            idx = i;
        }
        return m_segments[idx];
    }

    // Builds the cumulative frame-offset-per-bar table for [0, totalBars), given
    // the duration (in samples) of one quarter-note beat at the take's bpm. Must
    // be called once totalBars is known (take finalized, or loop loaded) before
    // frameOffsetForBar()/totalFrames() are used.
    void buildFrameMap(const size_t totalBars, const float samplesPerQuarterBeat)
    {
        m_cumulativeFrames.assign(totalBars + 1, 0);
        for (size_t bar = 0; bar < totalBars; ++bar)
        {
            const MeterSegment& seg = segmentForBar(bar);
            const float samplesPerBeat = seg.eighthUnit ? samplesPerQuarterBeat * 0.5f : samplesPerQuarterBeat;
            const auto samplesPerBar = static_cast<size_t>(samplesPerBeat * static_cast<float>(seg.beatsPerBar));
            m_cumulativeFrames[bar + 1] = m_cumulativeFrames[bar] + samplesPerBar;
        }
    }

    [[nodiscard]] size_t totalFrames() const noexcept
    {
        return m_cumulativeFrames.empty() ? 0 : m_cumulativeFrames.back();
    }

    // Start frame of barIndex, clamped to the last built bar boundary.
    [[nodiscard]] size_t frameOffsetForBar(const size_t barIndex) const noexcept
    {
        if (m_cumulativeFrames.empty())
        {
            return 0;
        }
        const size_t idx = std::min(barIndex, m_cumulativeFrames.size() - 1);
        return m_cumulativeFrames[idx];
    }

    // Derives how many bars fit in totalFrames, rounding to the nearest whole
    // bar (counted only once at least half of it lies within totalFrames) so
    // bar-lock stop/catch-up slop past a boundary isn't a whole extra bar.
    [[nodiscard]] size_t barCountForFrames(const size_t totalFrames, const float samplesPerQuarterBeat) const noexcept
    {
        size_t bar = 0;
        size_t framesSoFar = 0;
        while (true)
        {
            const MeterSegment& seg = segmentForBar(bar);
            const float samplesPerBeat = seg.eighthUnit ? samplesPerQuarterBeat * 0.5f : samplesPerQuarterBeat;
            const auto barFrames = static_cast<size_t>(samplesPerBeat * static_cast<float>(seg.beatsPerBar));
            if (barFrames == 0 || framesSoFar + barFrames / 2 >= totalFrames)
            {
                break;
            }
            framesSoFar += barFrames;
            ++bar;
        }
        return bar;
    }

  private:
    std::vector<MeterSegment> m_segments;
    std::vector<size_t> m_cumulativeFrames; // size totalBars+1; [i] = start frame of bar i
};

}
