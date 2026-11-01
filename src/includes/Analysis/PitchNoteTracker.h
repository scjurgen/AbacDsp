#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Numbers/Convert.h"

namespace AbacDsp
{

/// @ingroup analysis
/// @brief One pitch reading per hop as a detector delivers it. hz <= 0 means unvoiced; confidence is a periodicity
/// measure (1 - CMNDF of YIN), not a probability; level is a linear RMS; stamp is carried through to the event.
struct PitchObservation
{
    float hz{0.0f};
    float confidence{0.0f};
    float level{0.0f};
    uint64_t stamp{0};
};

/// @ingroup analysis
/// @brief An accepted note, or the end of one (released). pitch is the mean over the hops that made it stable, in
/// fractional MIDI notes, note the nearest integer, hz the frequency of pitch. A release carries the note that ended.
struct PitchNote
{
    float hz{0.0f};
    float pitch{0.0f};
    int note{0};
    float confidence{0.0f};
    float level{0.0f};
    uint64_t stamp{0};
    bool released{false};
};

/// @ingroup analysis
/// @brief Thresholds of PitchNoteTracker.
struct PitchNoteTrackerSettings
{
    float minLevelDb{-50.0f};         ///< RMS below this (dBFS) is unvoiced
    float minConfidence{0.85f};       ///< confidence below this is unvoiced
    size_t stableHops{3};             ///< readings within the tolerance that make a note
    float toleranceCents{50.0f};      ///< the spread of readings that still count as one pitch
    float hysteresisSemitones{0.65f}; ///< distance from the playing note beyond which a reading is a new candidate
    size_t releaseHops{3};            ///< unvoiced readings that end a note
    float attackRiseDb{6.0f};         ///< rise over the lowest level since the onset that counts as a new attack
    bool changeNeedsAttack{false};    ///< a pitch away from the playing note starts a note only with a recent attack
    float changeAttackRiseDb{4.0f};   ///< rise over the lowest of the last attackLookbackHops readings
    size_t attackLookbackHops{8};     ///< readings the level is compared with, at most 16
    size_t changeAttackHops{16};      ///< readings after an attack during which a new pitch may start a note
};

/**
 * @ingroup analysis
 * @brief Streaming note finder: turns a stream of pitch readings (one per hop) into note events, a note when a pitch
 * has been stable, a release when the voice has gone, a note again when the same pitch is struck anew. NoteSegmenter
 * does the same offline on a whole recording and finds onsets from the level; this one works one reading at a time.
 *
 * A reading is voiced when its level and confidence pass the thresholds. A pitch becomes a note when stableHops voiced
 * readings in a row stay within toleranceCents of their running mean, so one stray octave reading or a click does not
 * make a note, and the note starts that many hops before the event (the event is that much late). While a note plays,
 * readings within hysteresisSemitones of it belong to it (vibrato, or a pitch between two notes, does not flicker); a
 * reading farther away is a candidate for the next note and needs the same stability. A single unvoiced reading does
 * not end a note, releaseHops in a row do; silence never produces a 0 Hz note. The same note is struck again only
 * after a release, or while it plays when the level rises by attackRiseDb over the lowest level seen since the onset
 * (or the last strike), which a decaying string never does by itself.
 *
 * With changeNeedsAttack a reading away from the playing note is ignored unless the level rose by changeAttackRiseDb
 * over the lowest of the last attackLookbackHops readings no more than changeAttackHops readings ago. A plucked
 * string starts every note with such a rise; an octave glitch of the detector, a harmonic in the tail of a low note, a
 * finger pressing the next fret and a bend do not, so one pluck stays one note. The note that follows a release needs
 * no attack, and the playing note goes on while such readings are ignored. Legato notes without a rise in level
 * (hammer-ons, pull-offs) are ignored too until the first note is released.
 *
 * Plain logic on one reading at a time, no allocation; time is counted in hops and the stamp of the reading that
 * completes an event is passed through to it. Real-time safe.
 */
class PitchNoteTracker
{
  public:
    explicit PitchNoteTracker(const PitchNoteTrackerSettings& settings = {}) noexcept
        : m_settings(settings)
        , m_minLevel(Convert::dbToGain(settings.minLevelDb))
        , m_attackRatio(Convert::dbToGain(settings.attackRiseDb))
        , m_changeAttackRatio(Convert::dbToGain(settings.changeAttackRiseDb))
    {
    }

    void reset() noexcept
    {
        m_playing = false;
        m_note = 0;
        m_candidateCount = 0;
        m_candidateSum = 0.0f;
        m_unvoiced = 0;
        m_lowestLevel = 0.0f;
        m_levelCount = 0;
        m_levelNext = 0;
        m_sinceAttack = kNoAttack;
        m_rising = false;
    }

    [[nodiscard]] bool isPlaying() const noexcept
    {
        return m_playing;
    }

    /// The note that plays, meaningful while isPlaying().
    [[nodiscard]] int getNote() const noexcept
    {
        return m_note;
    }

    /// Takes the next reading; returns the event it completes, if any.
    [[nodiscard]] std::optional<PitchNote> update(const PitchObservation& reading) noexcept
    {
        trackAttack(reading.level);
        const bool voiced =
            reading.hz > 0.0f && reading.confidence >= m_settings.minConfidence && reading.level >= m_minLevel;
        if (!voiced)
        {
            return unvoiced(reading);
        }
        m_unvoiced = 0;
        const float pitch = Convert::frequencyToNote(reading.hz);
        if (m_playing && std::abs(pitch - static_cast<float>(m_note)) <= m_settings.hysteresisSemitones)
        {
            m_candidateCount = 0;
            return playingNote(reading);
        }
        if (m_playing && m_settings.changeNeedsAttack && m_sinceAttack > m_settings.changeAttackHops)
        {
            m_candidateCount = 0;
            return std::nullopt;
        }
        return candidate(reading, pitch);
    }

  private:
    static constexpr size_t kMaxLookback{16};
    static constexpr size_t kNoAttack{~size_t{0} / 2};

    /// Remembers how many readings ago the level began to rise over the lowest of the readings before it.
    void trackAttack(const float level) noexcept
    {
        const size_t look = std::min({m_settings.attackLookbackHops, m_levelCount, kMaxLookback});
        float lowest = level;
        for (size_t i = 0; i < look; ++i)
        {
            lowest = std::min(lowest, m_levels[(m_levelNext + kMaxLookback - 1 - i) % kMaxLookback]);
        }
        const bool rising = look > 0 && level > 0.0f && level >= lowest * m_changeAttackRatio;
        m_sinceAttack = rising && !m_rising ? 0 : std::min(m_sinceAttack + 1, kNoAttack);
        m_rising = rising;
        m_levels[m_levelNext] = level;
        m_levelNext = (m_levelNext + 1) % kMaxLookback;
        m_levelCount = std::min(m_levelCount + 1, kMaxLookback);
    }

    [[nodiscard]] std::optional<PitchNote> unvoiced(const PitchObservation& reading) noexcept
    {
        m_candidateCount = 0;
        if (!m_playing || ++m_unvoiced < m_settings.releaseHops)
        {
            return std::nullopt;
        }
        m_playing = false;
        PitchNote event = noteOf(static_cast<float>(m_note), reading);
        event.released = true;
        return event;
    }

    [[nodiscard]] std::optional<PitchNote> playingNote(const PitchObservation& reading) noexcept
    {
        if (reading.level >= m_lowestLevel * m_attackRatio)
        {
            m_lowestLevel = reading.level;
            PitchNote event = noteOf(Convert::frequencyToNote(reading.hz), reading);
            event.note = m_note;
            return event;
        }
        m_lowestLevel = std::min(m_lowestLevel, reading.level);
        return std::nullopt;
    }

    [[nodiscard]] std::optional<PitchNote> candidate(const PitchObservation& reading, const float pitch) noexcept
    {
        const float mean = m_candidateCount > 0 ? m_candidateSum / static_cast<float>(m_candidateCount) : pitch;
        if (m_candidateCount > 0 && std::abs(pitch - mean) * 100.0f > m_settings.toleranceCents)
        {
            m_candidateCount = 0;
        }
        m_candidateSum = m_candidateCount > 0 ? m_candidateSum + pitch : pitch;
        m_candidateCount = m_candidateCount > 0 ? m_candidateCount + 1 : 1;
        if (m_candidateCount < m_settings.stableHops)
        {
            return std::nullopt;
        }
        const float accepted = m_candidateSum / static_cast<float>(m_candidateCount);
        m_candidateCount = 0;
        m_playing = true;
        m_note = static_cast<int>(std::lround(accepted));
        m_lowestLevel = reading.level;
        return noteOf(accepted, reading);
    }

    [[nodiscard]] static PitchNote noteOf(const float pitch, const PitchObservation& reading) noexcept
    {
        PitchNote event;
        event.pitch = pitch;
        event.note = static_cast<int>(std::lround(pitch));
        event.hz = Convert::noteToFrequency(pitch);
        event.confidence = reading.confidence;
        event.level = reading.level;
        event.stamp = reading.stamp;
        return event;
    }

    PitchNoteTrackerSettings m_settings;
    float m_minLevel;
    float m_attackRatio;
    float m_changeAttackRatio;
    bool m_playing{false};
    int m_note{0};
    size_t m_candidateCount{0};
    float m_candidateSum{0.0f};
    size_t m_unvoiced{0};
    float m_lowestLevel{0.0f};
    std::array<float, kMaxLookback> m_levels{};
    size_t m_levelCount{0};
    size_t m_levelNext{0};
    size_t m_sinceAttack{kNoAttack};
    bool m_rising{false};
};

}
