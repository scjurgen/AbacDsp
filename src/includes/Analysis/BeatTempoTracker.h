#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace AbacDsp
{

/**
 * @ingroup analysis
 * @brief Tempo of a steady pulse from the onset times of its most recent beats.
 *
 * Meant for a click, a drum machine or a played pulse, not for music in general. The median
 * inter-onset interval sets the beat grid; every onset is then given a beat number relative
 * to an anchor onset, and onsets too far off the grid (an extra or flammed hit) are ignored.
 * A skipped hit simply advances the beat number by two. The beat period is the least-squares
 * slope of onset time over beat number, and the RMS of the residuals is the timing jitter.
 * The anchor is the onset that puts the most onsets on the grid. After a silence of a few
 * beats the history is cleared. Fixed-size storage, no allocation: safe on the audio thread.
 */
class BeatTempoTracker
{
  public:
    static constexpr size_t kMaxOnsets{17};
    static constexpr size_t kMinBeatsForReading{4};

    struct Reading
    {
        float bpm{0.f};
        float jitterMs{0.f};
        size_t beats{0};
    };

    explicit BeatTempoTracker(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void reset() noexcept
    {
        m_count = 0;
        m_reading.reset();
    }

    // Positions must not decrease between calls.
    void addOnset(const uint64_t samplePosition) noexcept
    {
        if (m_count == kMaxOnsets)
        {
            std::shift_left(m_onsets.begin(), m_onsets.end(), 1);
            --m_count;
        }
        m_onsets[m_count++] = samplePosition;
        m_reading = evaluate();
    }

    void advanceTo(const uint64_t samplePosition) noexcept
    {
        if (m_count == 0)
        {
            return;
        }
        const double beatSeconds = m_reading ? 60.0 / static_cast<double>(m_reading->bpm) : 0.0;
        const double timeoutSeconds = std::max(kSilenceSeconds, kSilenceBeats * beatSeconds);
        if (static_cast<double>(samplePosition - m_onsets[m_count - 1]) > timeoutSeconds * m_sampleRate)
        {
            reset();
        }
    }

    [[nodiscard]] std::optional<Reading> reading() const noexcept
    {
        return m_reading;
    }

  private:
    static constexpr double kGridTolerance{0.25}; // max distance from the grid, in beats
    static constexpr double kSilenceSeconds{3.0};
    static constexpr double kSilenceBeats{3.0};

    struct GridPoint
    {
        double beat{0.0};
        double time{0.0};
    };

    struct Grid
    {
        std::array<GridPoint, kMaxOnsets> points{};
        size_t count{0};
    };

    [[nodiscard]] double medianInterval() const noexcept
    {
        std::array<uint64_t, kMaxOnsets - 1> intervals{};
        const size_t numIntervals = m_count - 1;
        for (size_t i = 0; i < numIntervals; ++i)
        {
            intervals[i] = m_onsets[i + 1] - m_onsets[i];
        }
        const auto middle = intervals.begin() + static_cast<std::ptrdiff_t>(numIntervals / 2);
        std::nth_element(intervals.begin(), middle, intervals.begin() + static_cast<std::ptrdiff_t>(numIntervals));
        return static_cast<double>(*middle);
    }

    [[nodiscard]] Grid gridFrom(const size_t anchor, const double period) const noexcept
    {
        Grid grid{};
        grid.points[grid.count++] = {0.0, 0.0};
        double beat{0.0};
        uint64_t lastOnGrid = m_onsets[anchor];
        for (size_t i = anchor + 1; i < m_count; ++i)
        {
            const double beats = static_cast<double>(m_onsets[i] - lastOnGrid) / period;
            const double nearest = std::round(beats);
            if (nearest < 1.0 || std::abs(beats - nearest) > kGridTolerance)
            {
                continue;
            }
            beat += nearest;
            lastOnGrid = m_onsets[i];
            grid.points[grid.count++] = {beat, static_cast<double>(m_onsets[i] - m_onsets[anchor])};
        }
        return grid;
    }

    [[nodiscard]] std::optional<Reading> evaluate() const noexcept
    {
        if (m_count < kMinBeatsForReading)
        {
            return std::nullopt;
        }
        const double period = medianInterval();
        if (period <= 0.0)
        {
            return std::nullopt;
        }
        Grid best{};
        for (size_t anchor = 0; anchor + kMinBeatsForReading <= m_count; ++anchor)
        {
            const Grid grid = gridFrom(anchor, period);
            if (grid.count > best.count)
            {
                best = grid;
            }
        }
        if (best.count < kMinBeatsForReading)
        {
            return std::nullopt;
        }
        return fit(best);
    }

    [[nodiscard]] Reading fit(const Grid& grid) const noexcept
    {
        const auto n = static_cast<double>(grid.count);
        double meanBeat{0.0};
        double meanTime{0.0};
        for (size_t i = 0; i < grid.count; ++i)
        {
            meanBeat += grid.points[i].beat / n;
            meanTime += grid.points[i].time / n;
        }
        double covariance{0.0};
        double variance{0.0};
        for (size_t i = 0; i < grid.count; ++i)
        {
            covariance += (grid.points[i].beat - meanBeat) * (grid.points[i].time - meanTime);
            variance += (grid.points[i].beat - meanBeat) * (grid.points[i].beat - meanBeat);
        }
        const double samplesPerBeat = covariance / variance;
        double squaredResiduals{0.0};
        for (size_t i = 0; i < grid.count; ++i)
        {
            const double predicted = meanTime + samplesPerBeat * (grid.points[i].beat - meanBeat);
            squaredResiduals += (grid.points[i].time - predicted) * (grid.points[i].time - predicted);
        }
        const double jitterSamples = std::sqrt(squaredResiduals / n);
        return {static_cast<float>(60.0 * m_sampleRate / samplesPerBeat),
                static_cast<float>(1000.0 * jitterSamples / m_sampleRate), grid.count};
    }

    double m_sampleRate;
    std::array<uint64_t, kMaxOnsets> m_onsets{};
    size_t m_count{0};
    std::optional<Reading> m_reading;
};

}
