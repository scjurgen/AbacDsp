#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace NotePattern
{

// What a lesson trains; the scoring weights follow from it.
enum class Focus : uint8_t
{
    Pitch,
    Rhythm,
    Dynamics,
    Combined,
};

// Indexed by Focus; the Lua constant of each entry is its index.
inline constexpr auto kFocusNames =
    std::to_array<std::string_view>({"FocusPitch", "FocusRhythm", "FocusDynamics", "FocusCombined"});
static_assert(kFocusNames.size() == static_cast<size_t>(Focus::Combined) + 1);

[[nodiscard]] constexpr std::optional<Focus> focusFromIndex(const long long index) noexcept
{
    if (index < 0 || index >= static_cast<long long>(kFocusNames.size()))
    {
        return std::nullopt;
    }
    return static_cast<Focus>(index);
}

inline constexpr size_t kMaxNotes{64};
inline constexpr size_t kMaxBeatsPerBar{16};
inline constexpr size_t kMaxPatternBars{8};
inline constexpr size_t kMaxNameBytes{64};
inline constexpr float kMaxPickupBeats{8.f};
inline constexpr float kMaxTimingToleranceMs{1000.f};
inline constexpr float kMaxDynamicsToleranceDb{30.f};
inline constexpr float kMinBpm{30.f};
inline constexpr float kMaxBpm{240.f};
inline constexpr uint8_t kDefaultVelocity{100};
// Positions and lengths closer than this (in beats) count as equal.
inline constexpr float kEpsilonBeats{1e-4f};

// One expected note. The phrase is monophonic: a note ends before the next one starts.
struct Note
{
    float positionBeats{0.f};
    float durationBeats{0.f};
    uint8_t pitch{60};
    uint8_t velocity{kDefaultVelocity};
};

// How strictly a take is scored: full credit within the good band, none from the zero value on.
struct Scoring
{
    float timingGoodMs{60.f};
    float timingZeroMs{200.f};
    float dynamicsGoodDb{3.f};
    float dynamicsZeroDb{9.f};
    float passScore{70.f};
    bool octaveFree{true};
};

// The phrase to repeat, notes sorted by position. Positions count in beats from the downbeat of
// bar 1; an anacrusis lies at negative positions, down to -pickupBeats. Inactive means the
// script never defined a phrase.
struct Pattern
{
    std::array<Note, kMaxNotes> notes{};
    size_t noteCount{0};
    size_t beatsPerBar{4};
    size_t patternBars{1};
    float pickupBeats{0.f};
    float bpm{100.f};
    Scoring scoring{};
    uint8_t tonic{0};
    Focus focus{Focus::Combined};
    bool clickDefault{true};
    bool active{false};

    [[nodiscard]] float patternBeats() const noexcept
    {
        return static_cast<float>(beatsPerBar * patternBars);
    }
};

}
