#pragma once

#include <array>
#include <string_view>

enum class CcTarget : int
{
    luaParam1,
    luaParam2,
    luaParam3,
    luaParam4,
    luaParam5,
    luaParam6,
    luaParam7,
    luaParam8
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

inline constexpr std::array<CcMapping, 8> kDefaultCcMappings{{
    {16, 0.0f, 1.0f},
    {17, 0.0f, 1.0f},
    {18, 0.0f, 1.0f},
    {19, 0.0f, 1.0f},
    {20, 0.0f, 1.0f},
    {21, 0.0f, 1.0f},
    {22, 0.0f, 1.0f},
    {23, 0.0f, 1.0f},
}};

inline constexpr std::array<std::string_view, 8> kCcTargetParamIds{
    "luaParam1", "luaParam2", "luaParam3", "luaParam4", "luaParam5", "luaParam6", "luaParam7", "luaParam8",
};

// The dial's own full range (blueprint "range"), independent of the CC sub-range,
// used to clamp user-editable CC value ranges.
inline constexpr std::array<CcFullRange, 8> kCcTargetFullRange{{
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 1.0f},
}};
