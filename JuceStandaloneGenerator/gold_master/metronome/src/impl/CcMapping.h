#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int
{
    bpm,
    metroVolume,
    inputVolume,
    subVolume,
    swingRatio
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

inline constexpr std::array<CcMapping, 5> kDefaultCcMappings{{
    {20, 60.0f, 180.0f},
    {21, -20.0f, -10.0f},
    {22, -60.0f, 12.0f},
    {23, -60.0f, 0.0f},
    {24, 1.0f, 2.0f},
}};

inline constexpr std::array<std::string_view, 5> kCcTargetParamIds{
    "bpm", "metroVolume", "inputVolume", "subVolume", "swingRatio",
};

// The dial's own full range (blueprint "range"), independent of the CC sub-range,
// used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, 5> kCcTargetFullRange{{
    {40.0f, 250.0f},
    {-60.0f, 0.0f},
    {-60.0f, 12.0f},
    {-60.0f, 0.0f},
    {1.0f, 2.0f},
}};
