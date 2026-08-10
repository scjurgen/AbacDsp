#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <string_view>

#include "Numbers/Convert.h"

// Fixed, always-copied shared component (CPP_SOURCE_FILES_FIXED) - must stay JUCE-free so
// the audio-thread engine can use it too. Pure, allocation-free helpers for the Lua Music/
// Vel/Rr/Rhythm bindings in LuaScriptEngineBase.h; wraps Numbers/Convert.h rather than
// reimplementing Hz/note/interval math, so the engine and the core DSP library agree.

namespace LuaMusicMath
{

[[nodiscard]] inline float noteToHz(const float note, const float tuning = 440.f) noexcept
{
    return Convert::noteToFrequency<float>(note, tuning);
}

[[nodiscard]] inline float hzToNote(const float hz, const float tuning = 440.f) noexcept
{
    return Convert::frequencyToNote<float>(hz, tuning);
}

[[nodiscard]] inline float intervalToRatio(const float semitones) noexcept
{
    return Convert::noteIntervalToRatio<float>(semitones);
}

[[nodiscard]] inline float ratioToInterval(const float ratio) noexcept
{
    return Convert::ratioToNoteInterval<float>(ratio);
}

/// Exponential velocity/CC (0..1) to gain (0..1) curve; curve == 0 is linear. Positive curve
/// bends the low end down (slow start, fast rise); negative bends it up.
[[nodiscard]] inline float velocityToGainExponential(const float velocity, const float curve = 4.f) noexcept
{
    const float v = std::clamp(velocity, 0.f, 1.f);
    if (std::abs(curve) < 1e-4f)
    {
        return v;
    }
    return (std::exp(curve * v) - 1.f) / (std::exp(curve) - 1.f);
}

[[nodiscard]] inline float velocityToGainCubic(const float velocity) noexcept
{
    const float v = std::clamp(velocity, 0.f, 1.f);
    return v * v * v;
}

[[nodiscard]] inline size_t toroidIncrement(const size_t current, const size_t count) noexcept
{
    return count == 0 ? 0 : (current + 1) % count;
}

[[nodiscard]] inline size_t toroidAdvance(const size_t current, const size_t step, const size_t count) noexcept
{
    return count == 0 ? 0 : (current + step) % count;
}

struct ScaleEntry
{
    std::string_view name;
    std::span<const int> intervals;
};

struct ChordEntry
{
    std::string_view name;
    std::span<const int> intervals;
};

struct NoteValueEntry
{
    std::string_view name;
    float beats{1.f};
};

// clang-format off
inline constexpr auto kScaleMajor           = std::to_array<int>({0, 2, 4, 5, 7, 9, 11});
inline constexpr auto kScaleNaturalMinor    = std::to_array<int>({0, 2, 3, 5, 7, 8, 10});
inline constexpr auto kScaleHarmonicMinor   = std::to_array<int>({0, 2, 3, 5, 7, 8, 11});
inline constexpr auto kScaleMelodicMinor    = std::to_array<int>({0, 2, 3, 5, 7, 9, 11});
inline constexpr auto kScaleDorian          = std::to_array<int>({0, 2, 3, 5, 7, 9, 10});
inline constexpr auto kScalePhrygian        = std::to_array<int>({0, 1, 3, 5, 7, 8, 10});
inline constexpr auto kScaleLydian          = std::to_array<int>({0, 2, 4, 6, 7, 9, 11});
inline constexpr auto kScaleMixolydian      = std::to_array<int>({0, 2, 4, 5, 7, 9, 10});
inline constexpr auto kScaleLocrian         = std::to_array<int>({0, 1, 3, 5, 6, 8, 10});
inline constexpr auto kScaleMajorPentatonic = std::to_array<int>({0, 2, 4, 7, 9});
inline constexpr auto kScaleMinorPentatonic = std::to_array<int>({0, 3, 5, 7, 10});
inline constexpr auto kScaleBlues           = std::to_array<int>({0, 3, 5, 6, 7, 10});
inline constexpr auto kScaleWholeTone       = std::to_array<int>({0, 2, 4, 6, 8, 10});
inline constexpr auto kScaleChromatic       = std::to_array<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});

inline constexpr auto kScales = std::to_array<ScaleEntry>({
    {"Major", kScaleMajor},
    {"NaturalMinor", kScaleNaturalMinor},
    {"HarmonicMinor", kScaleHarmonicMinor},
    {"MelodicMinor", kScaleMelodicMinor},
    {"Dorian", kScaleDorian},
    {"Phrygian", kScalePhrygian},
    {"Lydian", kScaleLydian},
    {"Mixolydian", kScaleMixolydian},
    {"Locrian", kScaleLocrian},
    {"MajorPentatonic", kScaleMajorPentatonic},
    {"MinorPentatonic", kScaleMinorPentatonic},
    {"Blues", kScaleBlues},
    {"WholeTone", kScaleWholeTone},
    {"Chromatic", kScaleChromatic},
});

inline constexpr auto kChordMajor           = std::to_array<int>({0, 4, 7});
inline constexpr auto kChordMinor           = std::to_array<int>({0, 3, 7});
inline constexpr auto kChordDiminished      = std::to_array<int>({0, 3, 6});
inline constexpr auto kChordAugmented       = std::to_array<int>({0, 4, 8});
inline constexpr auto kChordMajor6          = std::to_array<int>({0, 4, 7, 9});
inline constexpr auto kChordMinor6          = std::to_array<int>({0, 3, 7, 9});
inline constexpr auto kChordMajor7          = std::to_array<int>({0, 4, 7, 11});
inline constexpr auto kChordMinor7          = std::to_array<int>({0, 3, 7, 10});
inline constexpr auto kChordDominant7       = std::to_array<int>({0, 4, 7, 10});
inline constexpr auto kChordMinorMajor7     = std::to_array<int>({0, 3, 7, 11});
inline constexpr auto kChordDiminished7     = std::to_array<int>({0, 3, 6, 9});
inline constexpr auto kChordHalfDiminished7 = std::to_array<int>({0, 3, 6, 10});
inline constexpr auto kChordSus2            = std::to_array<int>({0, 2, 7});
inline constexpr auto kChordSus4            = std::to_array<int>({0, 5, 7});
inline constexpr auto kChordAdd9            = std::to_array<int>({0, 4, 7, 14});
inline constexpr auto kChordMajor9          = std::to_array<int>({0, 4, 7, 11, 14});
inline constexpr auto kChordMinor9          = std::to_array<int>({0, 3, 7, 10, 14});
inline constexpr auto kChordDominant9       = std::to_array<int>({0, 4, 7, 10, 14});

inline constexpr auto kChords = std::to_array<ChordEntry>({
    {"Major", kChordMajor},
    {"Minor", kChordMinor},
    {"Diminished", kChordDiminished},
    {"Augmented", kChordAugmented},
    {"Major6", kChordMajor6},
    {"Minor6", kChordMinor6},
    {"Major7", kChordMajor7},
    {"Minor7", kChordMinor7},
    {"Dominant7", kChordDominant7},
    {"MinorMajor7", kChordMinorMajor7},
    {"Diminished7", kChordDiminished7},
    {"HalfDiminished7", kChordHalfDiminished7},
    {"Sus2", kChordSus2},
    {"Sus4", kChordSus4},
    {"Add9", kChordAdd9},
    {"Major9", kChordMajor9},
    {"Minor9", kChordMinor9},
    {"Dominant9", kChordDominant9},
});

// Beat-multipliers relative to one quarter note (a "beat" in BeatsToMs's sense).
inline constexpr auto kNoteValues = std::to_array<NoteValueEntry>({
    {"Whole", 4.f},
    {"Half", 2.f},
    {"Quarter", 1.f},
    {"Eighth", 0.5f},
    {"Sixteenth", 0.25f},
    {"ThirtySecond", 0.125f},
    {"DottedHalf", 3.f},
    {"DottedQuarter", 1.5f},
    {"DottedEighth", 0.75f},
    {"DottedSixteenth", 0.375f},
    {"TripletHalf", 4.f / 3.f},
    {"TripletQuarter", 2.f / 3.f},
    {"TripletEighth", 1.f / 3.f},
    {"TripletSixteenth", 1.f / 6.f},
});
// clang-format on

[[nodiscard]] inline float beatsToMs(const float beats, const float bpm) noexcept
{
    return beats * (60000.f / bpm);
}

}
