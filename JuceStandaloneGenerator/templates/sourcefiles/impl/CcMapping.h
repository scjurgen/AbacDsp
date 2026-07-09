#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int
{
    /*CC_TARGET_ENUM_LIST*/
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

inline constexpr std::array<CcMapping, /*NUM_CC_TARGETS*/> kDefaultCcMappings{{
    /*CC_DEFAULT_MAPPINGS*/
}};

inline constexpr std::array<std::string_view, /*NUM_CC_TARGETS*/> kCcTargetParamIds{
    /*CC_TARGET_PARAMID_LIST*/
};

// The dial's own full range (blueprint "range"), independent of the CC sub-range,
// used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, /*NUM_CC_TARGETS*/> kCcTargetFullRange{{
    /*CC_TARGET_FULL_RANGE*/
}};
