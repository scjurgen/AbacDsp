#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int
{
    record,
    play,
    overdub,
    undo,
    mixDown,
    clear,
    threshRec,
    freeRecord,
    bpm,
    clickVolume,
    clickRecordVolume,
    loopVolume,
    recThreshold,
    freeze,
    seqPlay,
    clearSeq
};

struct CcMapping
{
    int controller;
    float valueLow;
    float valueHigh;
};

struct CcFullRange
{
    float lo;
    float hi;
};

inline constexpr std::array<CcMapping, 16> kDefaultCcMappings{{
    {64, 0.0f, 1.0f},
    {66, 0.0f, 1.0f},
    {65, 0.0f, 1.0f},
    {67, 0.0f, 1.0f},
    {68, 0.0f, 1.0f},
    {69, 0.0f, 1.0f},
    {70, 0.0f, 1.0f},
    {72, 0.0f, 1.0f},
    {20, 60.0f, 180.0f},
    {22, -60.0f, 0.0f},
    {25, -60.0f, 0.0f},
    {23, -60.0f, 12.0f},
    {24, -60.0f, 0.0f},
    {71, 0.0f, 1.0f},
    {75, 0.0f, 1.0f},
    {76, 0.0f, 1.0f},
}};

inline constexpr std::array<std::string_view, 16> kCcTargetParamIds{
    "record", "play",        "overdub",           "undo",       "mixDown",      "clear",  "threshRec", "freeRecord",
    "bpm",    "clickVolume", "clickRecordVolume", "loopVolume", "recThreshold", "freeze", "seqPlay",   "clearSeq",
};

// The dial's own full range (blueprint "range"), independent of the CC sub-range,
// used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, 16> kCcTargetFullRange{{
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {50.0f, 250.0f},
    {-60.0f, 0.0f},
    {-60.0f, 0.0f},
    {-60.0f, 12.0f},
    {-60.0f, 0.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
}};
