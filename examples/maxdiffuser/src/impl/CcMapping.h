#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int {
  dry,
  wet,
  preDelay,
  elements,
  feedback,
  bulge,
  bottomSize,
  topSize,
  modulationDepth,
  modulationSpeed,
  lowPass,
  mix,
  pitch
};

struct CcMapping {
  int controller;
  float valueLow;
  float valueHigh;
};

struct CcFullRange {
  float lo;
  float hi;
};

inline constexpr std::array<CcMapping, 13> kDefaultCcMappings{{
    {20, -100.0f, 12.0f},
    {21, -100.0f, 12.0f},
    {22, 0.0f, 1000.0f},
    {23, 1.0f, 50.0f},
    {24, -100.0f, 100.0f},
    {25, -1.0f, 1.0f},
    {26, 51.0f, 22000.0f},
    {27, 51.0f, 22000.0f},
    {28, 0.0f, 1.0f},
    {29, 0.01f, 5.0f},
    {30, 20.0f, 20000.0f},
    {31, 0.0f, 100.0f},
    {32, -24.0f, 24.0f},
}};

inline constexpr std::array<std::string_view, 13> kCcTargetParamIds{
    "dry",     "wet",        "preDelay", "elements",        "feedback",
    "bulge",   "bottomSize", "topSize",  "modulationDepth", "modulationSpeed",
    "lowPass", "mix",        "pitch",
};

// The dial's own full range (blueprint "range"), independent of the CC
// sub-range, used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, 13> kCcTargetFullRange{{
    {-100.0f, 12.0f},
    {-100.0f, 12.0f},
    {0.0f, 1000.0f},
    {1.0f, 50.0f},
    {-100.0f, 100.0f},
    {-1.0f, 1.0f},
    {51.0f, 22000.0f},
    {51.0f, 22000.0f},
    {0.0f, 1.0f},
    {0.01f, 5.0f},
    {20.0f, 20000.0f},
    {0.0f, 100.0f},
    {-24.0f, 24.0f},
}};
