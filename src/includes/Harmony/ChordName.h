#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>

#include "Harmony/PitchClassSet.h"

namespace AbacDsp
{

/// @brief Sharps-only pitch class name (0 = C .. 11 = B), matching the twelve names a Home/Root
/// selector dial offers - folds any int into 0..11 first, so an out-of-range pitch class is safe.
[[nodiscard]] inline std::string_view noteName(const int pitchClass) noexcept
{
    static constexpr std::array<std::string_view, 12> kNames{"C",  "C#", "D",  "D#", "E",  "F",
                                                             "F#", "G",  "G#", "A",  "A#", "B"};
    return kNames[PitchClassSet::pitchClassOf(pitchClass)];
}

namespace ChordNameDetail
{

[[nodiscard]] constexpr std::array<bool, 12> intervalMask(const std::initializer_list<int> semitones) noexcept
{
    std::array<bool, 12> mask{};
    for (const auto semitone : semitones)
    {
        mask[PitchClassSet::pitchClassOf(semitone)] = true;
    }
    return mask;
}

struct ChordTemplate
{
    std::string_view suffix;
    std::array<bool, 12> intervals;
};

/// @brief Common chord qualities, most-familiar first so a tied score (e.g. sus2 vs. sus4 a
/// fifth apart, the same pitch classes either way) resolves toward the more ordinary label.
inline constexpr std::array<ChordTemplate, 18> kTemplates{{
    {"", intervalMask({0, 4, 7})},
    {"m", intervalMask({0, 3, 7})},
    {"7", intervalMask({0, 4, 7, 10})},
    {"maj7", intervalMask({0, 4, 7, 11})},
    {"m7", intervalMask({0, 3, 7, 10})},
    {"6", intervalMask({0, 4, 7, 9})},
    {"m6", intervalMask({0, 3, 7, 9})},
    {"9", intervalMask({0, 2, 4, 7, 10})},
    {"maj9", intervalMask({0, 2, 4, 7, 11})},
    {"m9", intervalMask({0, 2, 3, 7, 10})},
    {"m11", intervalMask({0, 2, 3, 5, 7, 10})},
    {"add9", intervalMask({0, 2, 4, 7})},
    {"m(add9)", intervalMask({0, 2, 3, 7})},
    {"6/9", intervalMask({0, 2, 4, 7, 9})},
    {"sus2", intervalMask({0, 2, 7})},
    {"sus4", intervalMask({0, 5, 7})},
    {"dim", intervalMask({0, 3, 6})},
    {"m7b5", intervalMask({0, 3, 6, 10})},
}};

}

/// @brief Best-effort chord label for a Voicing already in absolute semitones (e.g. from
/// HarmonicOrganism::currentState() after setHome()), as ordinary notation with correct bass
/// ("Cmaj7/E") - a diagnostic label, not a rigorous chord-theory engine.
[[nodiscard]] inline std::string nameChord(const Voicing& voicing) noexcept
{
    const auto notes = voicing.notes();
    if (notes.empty())
    {
        return {};
    }

    std::array<bool, 12> present{};
    for (const auto semitone : notes)
    {
        present[PitchClassSet::pitchClassOf(semitone)] = true;
    }

    int bestRoot = 0;
    int bestScore = std::numeric_limits<int>::min();
    std::string_view bestSuffix;
    for (int root = 0; root < 12; ++root)
    {
        if (!present[static_cast<size_t>(root)])
        {
            continue;
        }
        for (const auto& tmpl : ChordNameDetail::kTemplates)
        {
            int matched = 0;
            int extras = 0;
            int missing = 0;
            for (int interval = 0; interval < 12; ++interval)
            {
                const auto has = present[static_cast<size_t>((root + interval) % 12)];
                const auto wants = tmpl.intervals[static_cast<size_t>(interval)];
                if (has && wants)
                {
                    ++matched;
                }
                else if (has)
                {
                    ++extras;
                }
                else if (wants)
                {
                    ++missing;
                }
            }
            const auto score = matched * 3 - extras * 4 - missing;
            if (score > bestScore)
            {
                bestScore = score;
                bestRoot = root;
                bestSuffix = tmpl.suffix;
            }
        }
    }

    const auto lowestSemitone = *std::min_element(notes.begin(), notes.end());
    const auto bassPitchClass = PitchClassSet::pitchClassOf(lowestSemitone);

    std::string result{noteName(bestRoot)};
    result += bestSuffix;
    if (bassPitchClass != static_cast<unsigned>(bestRoot))
    {
        result += "/";
        result += noteName(static_cast<int>(bassPitchClass));
    }
    return result;
}

}
