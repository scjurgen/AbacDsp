#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int {

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

inline constexpr std::array<CcMapping, 0> kDefaultCcMappings{{

}};

inline constexpr std::array<std::string_view, 0> kCcTargetParamIds{

};

// The dial's own full range (blueprint "range"), independent of the CC
// sub-range, used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, 0> kCcTargetFullRange{{

}};
