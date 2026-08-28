#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace AbacDsp
{
/**
 * @ingroup sequencing
 * @brief BPM-invariant bar/beat/tick position clock for a fixed-length loop, with a
 * per-bar time signature timeline (MIDI-measure-meta-event style).
 *
 * Position is stored as fractional beats since loop start, not frames and not
 * ticks at a fixed PPQN. BPM only converts elapsed real time into elapsed beats
 * during advance(); it is never applied to the stored position, so a BPM change
 * needs no repositioning. A bar-count or time-signature change that shrinks the
 * loop below the current position wraps it back into range via modulo, preserving
 * beat/tick phase; growing the loop needs no adjustment since the old position is
 * already inside the new, larger range.
 *
 * Ticks are always relative to the quarter note (standard MIDI PPQN), regardless
 * of a bar's denominator - a 7/8 bar has 7 beats of a half-quarter-note each.
 */
template <size_t MaxNumTimeSignatureChanges>
class LoopTimeKeeper
{
    static_assert(MaxNumTimeSignatureChanges >= 1, "needs room for at least the default time signature");

  public:
    struct BarBeatTick
    {
        size_t bar;
        size_t beat;
        size_t tick;
    };

    explicit LoopTimeKeeper(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        m_timeSignatures[0] = {1u, 4, 4};
        recomputeLoopBeats();
    }

    void reset() noexcept
    {
        m_positionBeats = 0.0;
    }

    // Position-preserving: BPM only scales future advance() calls, so the stored
    // beat position needs no adjustment here.
    void setBpm(const float bpm) noexcept
    {
        m_bpm = std::max(bpm, kMinBpm);
    }

    void setBars(const unsigned bars) noexcept
    {
        m_bars = std::max(bars, kMinBars);
        recomputeLoopBeats();
        wrapPositionIntoRange();
    }

    // Time signature effective from startBar (1-based) onward. Overwrites an
    // existing entry at startBar, else inserts sorted; returns false (unchanged)
    // if the table is full for a new startBar. Never resets position.
    [[nodiscard]] bool setTimeSignature(const unsigned startBar, const uint16_t numerator,
                                        const uint16_t denominator) noexcept
    {
        const auto safeNumerator = std::max<uint16_t>(numerator, 1);
        const auto safeDenominator = std::max<uint16_t>(denominator, 1);
        for (size_t i = 0; i < m_timeSignatureCount; ++i)
        {
            if (m_timeSignatures[i].startBar == startBar)
            {
                m_timeSignatures[i].numerator = safeNumerator;
                m_timeSignatures[i].denominator = safeDenominator;
                recomputeLoopBeats();
                wrapPositionIntoRange();
                return true;
            }
        }
        if (m_timeSignatureCount >= MaxNumTimeSignatureChanges)
        {
            return false;
        }
        auto insertAt = m_timeSignatureCount;
        while (insertAt > 0 && m_timeSignatures[insertAt - 1].startBar > startBar)
        {
            m_timeSignatures[insertAt] = m_timeSignatures[insertAt - 1];
            --insertAt;
        }
        m_timeSignatures[insertAt] = {startBar, safeNumerator, safeDenominator};
        ++m_timeSignatureCount;
        recomputeLoopBeats();
        wrapPositionIntoRange();
        return true;
    }

    void advance(const size_t numFrames, const float speedRatio) noexcept
    {
        const auto elapsedBeats = static_cast<double>(numFrames) * static_cast<double>(speedRatio) *
                                  static_cast<double>(m_bpm) / (60.0 * static_cast<double>(m_sampleRate));
        m_positionBeats += elapsedBeats;
        wrapPositionIntoRange();
    }

    [[nodiscard]] float bpm() const noexcept
    {
        return m_bpm;
    }

    [[nodiscard]] unsigned bars() const noexcept
    {
        return m_bars;
    }

    [[nodiscard]] double positionBeats() const noexcept
    {
        return m_positionBeats;
    }

    [[nodiscard]] double loopBeats() const noexcept
    {
        return m_cachedLoopBeats;
    }

    // Snapshot only: converts the stored beat position to frames at the *current*
    // BPM. Callers needing a frame-domain position should call this fresh each
    // time, not cache it - a later BPM change would make a cached value stale.
    [[nodiscard]] double positionFrames() const noexcept
    {
        return m_positionBeats * 60.0 * static_cast<double>(m_sampleRate) / static_cast<double>(m_bpm);
    }

    // Walks bar-by-bar to locate the bar containing the current position, then
    // slices its active signature into beat/tick (ticksPerQuarterNote-relative).
    // O(bar count), not O(1), since bar lengths vary with the signature timeline.
    [[nodiscard]] BarBeatTick positionBBT(const size_t ticksPerQuarterNote) const noexcept
    {
        double remaining = m_positionBeats;
        unsigned bar = 1;
        double barLength = barLengthQuarterNotes(bar);
        while (remaining >= barLength && bar < m_bars)
        {
            remaining -= barLength;
            ++bar;
            barLength = barLengthQuarterNotes(bar);
        }
        const auto& sig = activeSignature(bar);
        const auto beatDuration = 4.0 / static_cast<double>(sig.denominator);
        auto beatIndex = static_cast<size_t>(remaining / beatDuration);
        const auto beatOffset = remaining - static_cast<double>(beatIndex) * beatDuration;
        // Rounded, not truncated: a position a hair below an exact tick boundary
        // (frame-domain round-trip error) must still report that boundary tick.
        auto tick =
            static_cast<size_t>(std::lround(beatOffset / beatDuration * static_cast<double>(ticksPerQuarterNote)));
        if (tick >= ticksPerQuarterNote)
        {
            tick = 0;
            ++beatIndex;
        }
        if (beatIndex >= static_cast<size_t>(sig.numerator))
        {
            beatIndex = 0;
            bar = bar < m_bars ? bar + 1 : 1;
        }
        return {bar, beatIndex + 1, tick};
    }

  private:
    struct TimeSignatureChange
    {
        unsigned startBar;
        uint16_t numerator;
        uint16_t denominator;
    };

    void wrapPositionIntoRange() noexcept
    {
        const auto loop = m_cachedLoopBeats;
        if (loop <= 0.0)
        {
            m_positionBeats = 0.0;
            return;
        }
        m_positionBeats = std::fmod(m_positionBeats, loop);
        if (m_positionBeats < 0.0)
        {
            m_positionBeats += loop;
        }
    }

    // Last entry whose startBar is at or before bar - the table is kept sorted
    // ascending, and entry 0 always starts at bar 1, so this always resolves.
    [[nodiscard]] const TimeSignatureChange& activeSignature(const unsigned bar) const noexcept
    {
        size_t active = 0;
        for (size_t i = 0; i < m_timeSignatureCount && m_timeSignatures[i].startBar <= bar; ++i)
        {
            active = i;
        }
        return m_timeSignatures[active];
    }

    [[nodiscard]] double barLengthQuarterNotes(const unsigned bar) const noexcept
    {
        const auto& sig = activeSignature(bar);
        return static_cast<double>(sig.numerator) * 4.0 / static_cast<double>(sig.denominator);
    }

    void recomputeLoopBeats() noexcept
    {
        double total = 0.0;
        for (unsigned bar = 1; bar <= m_bars; ++bar)
        {
            total += barLengthQuarterNotes(bar);
        }
        m_cachedLoopBeats = total;
    }

    static constexpr float kMinBpm = 1.f;
    static constexpr unsigned kMinBars = 1u;

    float m_sampleRate;
    float m_bpm{120.f};
    unsigned m_bars{1u};
    double m_positionBeats{0.0};
    double m_cachedLoopBeats{0.0};
    std::array<TimeSignatureChange, MaxNumTimeSignatureChanges> m_timeSignatures{};
    size_t m_timeSignatureCount{1};
};
}
