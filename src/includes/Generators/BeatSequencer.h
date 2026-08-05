#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace AbacDsp
{

/// @ingroup generators
/// @brief Where subdivision hits fall inside a beat.
/// Shuffle and Triplet both divide by three but place their hits differently: Triplet is even, Shuffle skips the
/// middle.
enum class SubdivType : uint8_t
{
    None,
    Eighth,
    Sixteenth,
    Triplet,
    Shuffle,
    Compound3
};

/**
 * @ingroup generators
 * @brief Sample-accurate musical grid, emitting one event per sample.
 *
 * Reporting per sample rather than per beat is what makes the grid usable as a
 * clock: a caller learns the exact frame a beat starts on, so a triggered event
 * lands on the sample rather than at the next block boundary.
 *
 * Position is carried as an integer sample count within the beat instead of an
 * accumulated float phase, so a long run cannot drift away from the bar line.
 * The grid can be realigned to a host quarter-note position at any time.
 */
class BeatSequencer
{
  public:
    /// @brief What one sample lands on: its position in the beat, and whether it starts a beat, subdivision or bar.
    struct GridEvent
    {
        size_t beatSamplePos{0};  // position within the beat for this sample (pre-advance)
        size_t beatIndexInBar{0}; // beat index within the bar for this sample
        bool beatStart{false};    // first sample of a beat
        bool subdivision{false};  // sample lands on a subdivision hit
        bool barWrapped{false};   // advancing past this sample wrapped to a new bar
    };

    explicit BeatSequencer(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        applyBpm(m_bpm);
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
        applyBpm(m_bpm);
    }

    void setBpm(const float bpm)
    {
        applyBpm(bpm);
    }

    void setBeatsPerBar(const size_t beatsPerBar) noexcept
    {
        m_beatsPerBar = beatsPerBar;
    }

    void setSubdivType(const SubdivType type)
    {
        m_subdivType = type;
        updateSubPositions();
    }

    void setSwingRatio(const float ratio)
    {
        m_swingRatio = ratio;
        updateSubPositions();
    }

    void reset() noexcept
    {
        m_beatSamplePos = 0;
        m_beatIndexInBar = 0;
        m_barIndex = 0;
    }

    void resetBarPosition() noexcept
    {
        m_beatIndexInBar = 0;
    }

    [[nodiscard]] GridEvent advance() noexcept
    {
        GridEvent event{};
        event.beatSamplePos = m_beatSamplePos;
        event.beatIndexInBar = m_beatIndexInBar;
        event.beatStart = (m_beatSamplePos == 0);
        for (const size_t subPos : m_subPositions)
        {
            if (m_beatSamplePos == subPos)
            {
                event.subdivision = true;
                break;
            }
        }
        if (++m_beatSamplePos >= m_samplesPerBeat)
        {
            m_beatSamplePos = 0;
            if (++m_beatIndexInBar >= m_beatsPerBar)
            {
                m_beatIndexInBar = 0;
                event.barWrapped = true;
                ++m_barIndex;
            }
        }
        return event;
    }

    // Align phase to a host quarter-note position; no-op until the grid is valid.
    void syncToPpq(const double ppqPosition) noexcept
    {
        if (m_beatsPerBar == 0 || m_samplesPerBeat == 0)
        {
            return;
        }
        const double barBeats = static_cast<double>(m_beatsPerBar);
        double phaseInBar = std::fmod(ppqPosition, barBeats);
        if (phaseInBar < 0.0)
        {
            phaseInBar += barBeats;
        }
        const auto beatIndex = static_cast<size_t>(phaseInBar);
        const double beatFraction = phaseInBar - static_cast<double>(beatIndex);
        m_beatIndexInBar = beatIndex;
        m_beatSamplePos = static_cast<size_t>(beatFraction * static_cast<double>(m_samplesPerBeat));
    }

    [[nodiscard]] float bpm() const noexcept
    {
        return m_bpm;
    }

    [[nodiscard]] size_t samplesPerBeat() const noexcept
    {
        return m_samplesPerBeat;
    }

    [[nodiscard]] size_t beatSamplePos() const noexcept
    {
        return m_beatSamplePos;
    }

    [[nodiscard]] size_t beatIndexInBar() const noexcept
    {
        return m_beatIndexInBar;
    }

    // Absolute bar count since the last reset() (never wraps). Together with
    // beatIndexInBar(), gives a generic bar.beat position for any beatsPerBar.
    [[nodiscard]] size_t barIndex() const noexcept
    {
        return m_barIndex;
    }

    [[nodiscard]] size_t beatsPerBar() const noexcept
    {
        return m_beatsPerBar;
    }

    [[nodiscard]] float barPhase() const noexcept
    {
        if (m_samplesPerBeat == 0 || m_beatsPerBar == 0)
        {
            return 0.f;
        }
        const float beatPhase = static_cast<float>(m_beatSamplePos) / static_cast<float>(m_samplesPerBeat);
        return (static_cast<float>(m_beatIndexInBar) + beatPhase) / static_cast<float>(m_beatsPerBar);
    }

    [[nodiscard]] const std::vector<size_t>& subPositions() const noexcept
    {
        return m_subPositions;
    }

    // Signed distance in samples from the current position to the nearest beat
    // boundary: negative if that boundary already passed, positive if it is
    // still ahead. Ties (exactly half a beat) resolve to the boundary behind.
    // Used to beat-lock an event (e.g. a threshold crossing) within a tolerance
    // window without moving the clock itself.
    [[nodiscard]] long samplesToNearestBeat() const noexcept
    {
        if (m_samplesPerBeat == 0)
        {
            return 0;
        }
        const auto pos = static_cast<long>(m_beatSamplePos);
        const auto spb = static_cast<long>(m_samplesPerBeat);
        return (pos * 2 <= spb) ? -pos : (spb - pos);
    }

    // Same convention as samplesToNearestBeat(), against the bar grid instead.
    [[nodiscard]] long samplesToNearestBar() const noexcept
    {
        if (m_samplesPerBeat == 0 || m_beatsPerBar == 0)
        {
            return 0;
        }
        const auto pos = static_cast<long>(m_beatIndexInBar) * static_cast<long>(m_samplesPerBeat) +
                         static_cast<long>(m_beatSamplePos);
        const auto spBar = static_cast<long>(m_beatsPerBar) * static_cast<long>(m_samplesPerBeat);
        return (pos * 2 <= spBar) ? -pos : (spBar - pos);
    }

  private:
    void applyBpm(const float bpm)
    {
        m_bpm = bpm;
        m_samplesPerBeat = beatsToSamples(bpm);
        updateSubPositions();
    }

    void updateSubPositions()
    {
        m_subPositions.clear();
        if (m_samplesPerBeat == 0)
        {
            return;
        }
        const size_t spb = m_samplesPerBeat;
        switch (m_subdivType)
        {
            case SubdivType::None:
                break;
            case SubdivType::Eighth:
                m_subPositions = {spb / 2};
                break;
            case SubdivType::Sixteenth:
                m_subPositions = {spb / 4, spb / 2, 3 * spb / 4};
                break;
            case SubdivType::Triplet:
                [[fallthrough]];
            case SubdivType::Compound3:
                m_subPositions = {spb / 3, 2 * spb / 3};
                break;
            case SubdivType::Shuffle:
            {
                const auto longPart =
                    static_cast<size_t>(static_cast<float>(spb) * m_swingRatio / (1.f + m_swingRatio));
                m_subPositions = {longPart};
                break;
            }
        }
    }

    [[nodiscard]] size_t beatsToSamples(const float bpm) const noexcept
    {
        return static_cast<size_t>(m_sampleRate * 60.f / bpm);
    }

    float m_sampleRate{48000.f};
    float m_bpm{120.f};
    float m_swingRatio{1.f};
    size_t m_beatsPerBar{4};
    SubdivType m_subdivType{SubdivType::None};

    size_t m_samplesPerBeat{0};
    size_t m_beatSamplePos{0};
    size_t m_beatIndexInBar{0};
    size_t m_barIndex{0};

    std::vector<size_t> m_subPositions;
};

}
