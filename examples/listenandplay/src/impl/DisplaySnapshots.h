#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "NotePattern.h"

// What the editor widgets draw, copied out of the engine once per UI frame. No JUCE and no engine types.
namespace ListenPlayDisplay
{

inline constexpr size_t kMaxPlayed{128};

enum class Phase : uint8_t
{
    Idle,
    Listening,
    CountIn,
    Capturing,
    Analysing,
    Review,
    Replaying,
};

struct RollNote
{
    float startBeats{0.f};
    float lengthBeats{0.f};
    uint8_t pitch{60};
    uint8_t velocity{90};
    bool missed{false};
};

// credit runs from 0 (wrong) to 1 (right and on time); an extra note has none.
struct RollPlayed
{
    float startBeats{0.f};
    float lengthBeats{0.f};
    float midi{60.f};
    float credit{0.f};
    bool pitchRight{false};
    bool extra{false};
};

struct PianoRollSnapshot
{
    std::array<RollNote, NotePattern::kMaxNotes> target{};
    size_t targetCount{0};
    std::array<RollPlayed, kMaxPlayed> played{};
    size_t playedCount{0};
    float pickupBeats{0.f};
    float patternBeats{4.f};
    size_t beatsPerBar{4};
    float progress{0.f};
    size_t countInBeats{4};
    int countInBeat{0};
    bool hasPhrase{false};
    bool hasReference{false};
    bool reference{false};
    bool revealed{false};
    Phase phase{Phase::Idle};
};

struct ScoreSnapshot
{
    float notes{0.f};
    float rhythm{0.f};
    float dynamics{0.f};
    float overall{0.f};
    float passScore{70.f};
    float meanTimingMs{0.f};
    float timingSpreadMs{0.f};
    float meanIntonationCents{0.f};
    float tempoRatio{1.f};
    float suggestedLatencyMs{0.f};
    size_t expected{0};
    size_t matched{0};
    size_t missed{0};
    size_t extras{0};
    int octaveShift{0};
    bool latencyKnown{false};
    bool clickOn{true};
    bool passed{false};
    bool hasResult{false};
};

struct LessonRow
{
    std::string stem;
    std::string title;
    std::string focus;
    char group{'a'};
    float best{0.f};
    uint32_t attempts{0};
    bool passed{false};
    bool unlocked{true};
    bool recommended{false};
    bool current{false};
};

}
