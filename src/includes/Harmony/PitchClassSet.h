#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <span>

namespace AbacDsp
{

/**
 * @ingroup harmony
 * @brief A set of the 12 pitch classes (0 = the harmonic home, 11 = a semitone below it),
 * independent of register - answers register-agnostic questions like "does this candidate
 * keep the home audible" or "how many tones do these two fields share".
 */
class PitchClassSet
{
  public:
    constexpr PitchClassSet() noexcept = default;

    /// @brief Builds a set from absolute semitones (any range), folding each into 0..11.
    [[nodiscard]] static constexpr PitchClassSet fromSemitones(const std::span<const int> semitones) noexcept
    {
        PitchClassSet set{};
        for (const auto semitone : semitones)
        {
            set.add(semitone);
        }
        return set;
    }

    constexpr void add(const int semitone) noexcept
    {
        m_bits |= (1u << pitchClassOf(semitone));
    }

    [[nodiscard]] constexpr bool contains(const int semitone) const noexcept
    {
        return (m_bits & (1u << pitchClassOf(semitone))) != 0u;
    }

    [[nodiscard]] constexpr PitchClassSet unionWith(const PitchClassSet& other) const noexcept
    {
        PitchClassSet result{};
        result.m_bits = m_bits | other.m_bits;
        return result;
    }

    [[nodiscard]] constexpr size_t commonToneCount(const PitchClassSet& other) const noexcept
    {
        return static_cast<size_t>(std::popcount(m_bits & other.m_bits));
    }

    [[nodiscard]] constexpr size_t size() const noexcept
    {
        return static_cast<size_t>(std::popcount(m_bits));
    }

    /// @brief Folds any semitone offset into a 0..11 pitch class.
    [[nodiscard]] static constexpr unsigned pitchClassOf(const int semitone) noexcept
    {
        const int mod = semitone % 12;
        return static_cast<unsigned>(mod < 0 ? mod + 12 : mod);
    }

  private:
    unsigned m_bits{0u};
};

/**
 * @ingroup harmony
 * @brief One concrete upper-structure voicing: absolute semitone offsets from the harmonic
 * home, register included (may be negative, may repeat a pitch class an octave apart), up to
 * kMaxNotes at a time.
 */
class Voicing
{
  public:
    static constexpr size_t kMaxNotes{12};

    constexpr Voicing() noexcept = default;

    [[nodiscard]] static constexpr Voicing fromSemitones(const std::span<const int> semitones) noexcept
    {
        Voicing voicing{};
        for (const auto semitone : semitones)
        {
            voicing.add(semitone);
        }
        return voicing;
    }

    /// @brief Appends a note; silently drops it once kMaxNotes is reached.
    constexpr void add(const int semitone) noexcept
    {
        if (m_count < kMaxNotes)
        {
            m_notes[m_count++] = semitone;
        }
    }

    [[nodiscard]] constexpr std::span<const int> notes() const noexcept
    {
        return {m_notes.data(), m_count};
    }

    [[nodiscard]] constexpr size_t size() const noexcept
    {
        return m_count;
    }

    [[nodiscard]] constexpr PitchClassSet pitchClasses() const noexcept
    {
        return PitchClassSet::fromSemitones(notes());
    }

  private:
    std::array<int, kMaxNotes> m_notes{};
    size_t m_count{0};
};

/// @brief Count of notes present in both voicings at the same absolute semitone (each note in
/// `a` matches at most one equal note in `b`, so a repeated pitch counts once per repetition).
[[nodiscard]] constexpr size_t commonTones(const Voicing& a, const Voicing& b) noexcept
{
    std::array<bool, Voicing::kMaxNotes> bUsed{};
    size_t count = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        for (size_t j = 0; j < b.size(); ++j)
        {
            if (!bUsed[j] && a.notes()[i] == b.notes()[j])
            {
                bUsed[j] = true;
                ++count;
                break;
            }
        }
    }
    return count;
}

/// @brief The voice-leading cost of moving from one voicing to another, under a greedy
/// nearest-unclaimed-partner pairing (not necessarily globally optimal, but deterministic and
/// cheap for the small voicings this operates on). Notes left unpaired - `from` shrinking or
/// `to` growing - are drops/adds, not motion, and so don't contribute to either total.
struct VoicingMotion
{
    size_t matchedCount{0};
    int maxMotionSemitones{0};
    int totalMotionSemitones{0};
};

[[nodiscard]] constexpr VoicingMotion voiceLeadingMotion(const Voicing& from, const Voicing& to) noexcept
{
    std::array<bool, Voicing::kMaxNotes> fromUsed{};
    std::array<bool, Voicing::kMaxNotes> toUsed{};
    VoicingMotion motion{};
    const auto pairsToMatch = std::min(from.size(), to.size());
    for (size_t pair = 0; pair < pairsToMatch; ++pair)
    {
        int bestDistance = -1;
        size_t bestFrom = 0;
        size_t bestTo = 0;
        for (size_t i = 0; i < from.size(); ++i)
        {
            if (fromUsed[i])
            {
                continue;
            }
            for (size_t j = 0; j < to.size(); ++j)
            {
                if (toUsed[j])
                {
                    continue;
                }
                const int diff = from.notes()[i] - to.notes()[j];
                const int distance = diff < 0 ? -diff : diff;
                if (bestDistance < 0 || distance < bestDistance)
                {
                    bestDistance = distance;
                    bestFrom = i;
                    bestTo = j;
                }
            }
        }
        fromUsed[bestFrom] = true;
        toUsed[bestTo] = true;
        motion.maxMotionSemitones = std::max(motion.maxMotionSemitones, bestDistance);
        motion.totalMotionSemitones += bestDistance;
        ++motion.matchedCount;
    }
    return motion;
}

}
