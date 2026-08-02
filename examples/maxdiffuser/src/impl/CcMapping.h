#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int
{
    dry,
    wet,
    preDelay,
    elements,
    tapSpan,
    feedback,
    bulge,
    bottomSize,
    topSize,
    sizeSpread,
    modulationDepth,
    modulationSpeed,
    lowPass,
    mix,
    pitch,
    pitchDelay,
    pitch2,
    pitch2Delay,
    fdnMix,
    fdnSize,
    fdnDecay
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

inline constexpr std::array<CcMapping, 21> kDefaultCcMappings{{
    {20, -100.0f, 12.0f},  {21, -100.0f, 12.0f}, {22, 0.0f, 1000.0f},   {23, 0.0f, 50.0f},    {40, 0.0f, 100.0f},
    {24, -100.0f, 100.0f}, {25, -1.0f, 1.0f},    {26, 0.5f, 100.0f},    {27, 0.5f, 100.0f},   {36, 0.0f, 10.0f},
    {28, 0.0f, 1.0f},      {29, 0.01f, 5.0f},    {30, 20.0f, 20000.0f}, {31, 0.0f, 100.0f},   {32, -24.0f, 24.0f},
    {38, 0.0f, 1000.0f},   {37, -24.0f, 24.0f},  {39, 0.0f, 1000.0f},   {33, -100.0f, 12.0f}, {34, 1.0f, 330.0f},
    {35, 1.0f, 100000.0f},
}};

inline constexpr std::array<std::string_view, 21> kCcTargetParamIds{
    "dry",        "wet",        "preDelay",   "elements",        "tapSpan",         "feedback", "bulge",
    "bottomSize", "topSize",    "sizeSpread", "modulationDepth", "modulationSpeed", "lowPass",  "mix",
    "pitch",      "pitchDelay", "pitch2",     "pitch2Delay",     "fdnMix",          "fdnSize",  "fdnDecay",
};

// The dial's own full range (blueprint "range"), independent of the CC sub-range,
// used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, 21> kCcTargetFullRange{{
    {-100.0f, 12.0f},  {-100.0f, 12.0f}, {0.0f, 1000.0f},   {0.0f, 50.0f},   {0.0f, 100.0f},  {-100.0f, 100.0f},
    {-1.0f, 1.0f},     {0.5f, 100.0f},   {0.5f, 100.0f},    {0.0f, 10.0f},   {0.0f, 1.0f},    {0.01f, 5.0f},
    {20.0f, 20000.0f}, {0.0f, 100.0f},   {-24.0f, 24.0f},   {0.0f, 1000.0f}, {-24.0f, 24.0f}, {0.0f, 1000.0f},
    {-100.0f, 12.0f},  {1.0f, 330.0f},   {1.0f, 100000.0f},
}};
