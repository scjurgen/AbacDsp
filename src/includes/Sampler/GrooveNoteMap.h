#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace AbacDsp
{

// Instrument tags a groove MIDI note can carry, most specific first, transcribed from
// MidiDrums/README.md's note table. Kit-independent: "what a note means", not which sample
// plays it - see GrooveKit.h for how a kit's own pieces resolve against this vocabulary.
enum class GrooveTag : uint8_t
{
    None, // sentinel: padding only, never a real match
    Kick,
    Snare,
    SnareRoll,
    SnareAlt,
    Rimshot,
    Sidestick,
    Tom,
    TomLow,
    Tom1,
    Tom2,
    Tom3,
    TomLeft,
    Hihat,
    HihatClosed,
    HihatOpen,
    HihatOpenTip,
    HihatClosedPedal,
    HihatClosedEdge,
    HihatOpenPedal,
    HihatOpen1,
    HihatOpen2,
    HihatOpen3,
    HihatStep,
    HihatHalfOpen,
    HihatStopped,
    HihatSoftStep,
    HihatGhost,
    Cymbal,
    Crash,
    CrashStopped,
    CrashLong,
    Ride,
    RideBell,
    China,
    Timbale,
    Timbale1,
    Timbale2,
    Timbale3,
    Timbale4,
    TimbaleDamped,
    Woodblock,
    ClickLow, // metronome piece, not a real kit instrument - see MidiDrums/Metronome
    ClickHigh,
    Count, // sentinel: total tag count, for sizing a per-tag lookup table
};

struct GrooveTagName
{
    GrooveTag tag;
    std::string_view name;
};

// clang-format off
inline constexpr auto kGrooveTagNames = std::to_array<GrooveTagName>({
    {GrooveTag::Kick, "kick"},
    {GrooveTag::Snare, "snare"},
    {GrooveTag::SnareRoll, "snare_roll"},
    {GrooveTag::SnareAlt, "snare_alt"},
    {GrooveTag::Rimshot, "rimshot"},
    {GrooveTag::Sidestick, "sidestick"},
    {GrooveTag::Tom, "tom"},
    {GrooveTag::TomLow, "tom_low"},
    {GrooveTag::Tom1, "tom1"},
    {GrooveTag::Tom2, "tom2"},
    {GrooveTag::Tom3, "tom3"},
    {GrooveTag::TomLeft, "tom_left"},
    {GrooveTag::Hihat, "hihat"},
    {GrooveTag::HihatClosed, "hihat_closed"},
    {GrooveTag::HihatOpen, "hihat_open"},
    {GrooveTag::HihatOpenTip, "hihat_open_tip"},
    {GrooveTag::HihatClosedPedal, "hihat_closed_pedal"},
    {GrooveTag::HihatClosedEdge, "hihat_closed_edge"},
    {GrooveTag::HihatOpenPedal, "hihat_open_pedal"},
    {GrooveTag::HihatOpen1, "hihat_open1"},
    {GrooveTag::HihatOpen2, "hihat_open2"},
    {GrooveTag::HihatOpen3, "hihat_open3"},
    {GrooveTag::HihatStep, "hihat_step"},
    {GrooveTag::HihatHalfOpen, "hihat_half_open"},
    {GrooveTag::HihatStopped, "hihat_stopped"},
    {GrooveTag::HihatSoftStep, "hihat_soft_step"},
    {GrooveTag::HihatGhost, "hihat_ghost"},
    {GrooveTag::Cymbal, "cymbal"},
    {GrooveTag::Crash, "crash"},
    {GrooveTag::CrashStopped, "crash_stopped"},
    {GrooveTag::CrashLong, "crash_long"},
    {GrooveTag::Ride, "ride"},
    {GrooveTag::RideBell, "ride_bell"},
    {GrooveTag::China, "china"},
    {GrooveTag::Timbale, "timbale"},
    {GrooveTag::Timbale1, "timbale1"},
    {GrooveTag::Timbale2, "timbale2"},
    {GrooveTag::Timbale3, "timbale3"},
    {GrooveTag::Timbale4, "timbale4"},
    {GrooveTag::TimbaleDamped, "timbale_damped"},
    {GrooveTag::Woodblock, "woodblock"},
    {GrooveTag::ClickLow, "click_low"},
    {GrooveTag::ClickHigh, "click_high"},
});
// clang-format on

// Resolves a lowercase instrument name (see kGrooveTagNames) to its tag;
// GrooveTag::None if unrecognized.
[[nodiscard]] constexpr GrooveTag tagFromName(const std::string_view name) noexcept
{
    for (const auto& entry : kGrooveTagNames)
    {
        if (entry.name == name)
        {
            return entry.tag;
        }
    }
    return GrooveTag::None;
}

struct GrooveNoteTags
{
    uint8_t note;
    std::array<GrooveTag, 4> tags; // most specific first; GrooveTag::None marks the end
};

// clang-format off
inline constexpr auto kGrooveNoteMap = std::to_array<GrooveNoteTags>({
    {12, {GrooveTag::HihatOpenTip, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {13, {GrooveTag::HihatOpenTip, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {14, {GrooveTag::HihatOpenTip, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {15, {GrooveTag::HihatOpenTip, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {16, {GrooveTag::HihatOpenTip, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {21, {GrooveTag::HihatClosedPedal, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {22, {GrooveTag::HihatClosedEdge, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {23, {GrooveTag::HihatOpenPedal, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {24, {GrooveTag::HihatOpen1, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {25, {GrooveTag::HihatOpen2, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {26, {GrooveTag::HihatOpen3, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {27, {GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {28, {GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {29, {GrooveTag::Ride, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {30, {GrooveTag::Ride, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {31, {GrooveTag::China, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {32, {GrooveTag::China, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {36, {GrooveTag::Kick, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {37, {GrooveTag::Rimshot, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {38, {GrooveTag::Snare, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {39, {GrooveTag::SnareRoll, GrooveTag::Snare, GrooveTag::None, GrooveTag::None}},
    {40, {GrooveTag::SnareAlt, GrooveTag::Snare, GrooveTag::None, GrooveTag::None}},
    {41, {GrooveTag::TomLow, GrooveTag::Tom, GrooveTag::None, GrooveTag::None}},
    {42, {GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal, GrooveTag::None}},
    {43, {GrooveTag::Tom1, GrooveTag::Tom, GrooveTag::None, GrooveTag::None}},
    {44, {GrooveTag::HihatStep, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {45, {GrooveTag::Tom2, GrooveTag::Tom, GrooveTag::None, GrooveTag::None}},
    {46, {GrooveTag::HihatHalfOpen, GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {47, {GrooveTag::Tom3, GrooveTag::Tom, GrooveTag::None, GrooveTag::None}},
    {48, {GrooveTag::TomLeft, GrooveTag::Tom, GrooveTag::None, GrooveTag::None}},
    {49, {GrooveTag::HihatOpen, GrooveTag::Hihat, GrooveTag::Cymbal, GrooveTag::None}},
    {50, {GrooveTag::CrashStopped, GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None}},
    {51, {GrooveTag::Ride, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {52, {GrooveTag::CrashLong, GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None}},
    {53, {GrooveTag::RideBell, GrooveTag::Ride, GrooveTag::Cymbal, GrooveTag::None}},
    {54, {GrooveTag::CrashStopped, GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None}}, // "stopped, alt"
    {55, {GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {56, {GrooveTag::Woodblock, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {57, {GrooveTag::China, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}},
    {58, {GrooveTag::China, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}}, // "short"
    {59, {GrooveTag::CrashLong, GrooveTag::Crash, GrooveTag::Cymbal, GrooveTag::None}}, // "long, alt"
    {60, {GrooveTag::Ride, GrooveTag::Cymbal, GrooveTag::None, GrooveTag::None}}, // "alt"
    {61, {GrooveTag::HihatStopped, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {62, {GrooveTag::HihatStep, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}}, // "step, alt"
    {63, {GrooveTag::HihatSoftStep, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {64, {GrooveTag::HihatStep, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}}, // "step, alt 2"
    {65, {GrooveTag::HihatGhost, GrooveTag::HihatClosed, GrooveTag::Hihat, GrooveTag::Cymbal}},
    {66, {GrooveTag::Timbale1, GrooveTag::Timbale, GrooveTag::None, GrooveTag::None}},
    {67, {GrooveTag::Timbale2, GrooveTag::Timbale, GrooveTag::None, GrooveTag::None}},
    {68, {GrooveTag::Timbale3, GrooveTag::Timbale, GrooveTag::None, GrooveTag::None}},
    {69, {GrooveTag::TimbaleDamped, GrooveTag::Timbale, GrooveTag::None, GrooveTag::None}},
    {70, {GrooveTag::Timbale4, GrooveTag::Timbale, GrooveTag::None, GrooveTag::None}},
    {71, {GrooveTag::Sidestick, GrooveTag::Rimshot, GrooveTag::None, GrooveTag::None}},
    {72, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {73, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {74, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {75, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {76, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {77, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {78, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {79, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {80, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {81, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {82, {GrooveTag::Tom, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {100, {GrooveTag::ClickLow, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
    {101, {GrooveTag::ClickHigh, GrooveTag::None, GrooveTag::None, GrooveTag::None}},
});
// clang-format on

// Linear scan: called once per groove note during offline load, never per audio sample.
[[nodiscard]] inline std::array<GrooveTag, 4> tagsForGrooveNote(const uint8_t note) noexcept
{
    for (const auto& entry : kGrooveNoteMap)
    {
        if (entry.note == note)
        {
            return entry.tags;
        }
    }
    return {GrooveTag::None, GrooveTag::None, GrooveTag::None, GrooveTag::None};
}

}
