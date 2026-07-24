#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kClassic{
    .name = "Classic",

    .background = 0xff330000,
    .backgroundComponent = 0xff1a0000,
    .backgroundDark = 0xff800000,
    .backgroundMid = 0xff4d0000,
    .backgroundLight = 0xffe60000,
    .gradientDark = 0xff330000,
    .knobGradientStart = 0xff000000,
    .knobGradientCenter = 0xff4d0000,
    .knobGradientEnd = 0xffcc0000,
    .statusOutline = 0xffcc0000,
    .labelColour = 0xffcccccc,

    .spectrogramStops = {{{0.0f, 0xff000000}, {1.0f, 0xffff0000}}},
    .spectrogramStopCount = 2,

    .cpuZones = {.safe = 0xff4e9a3c, .warn = 0xffe08a1e, .danger = 0xffff2e1e},
    .levelZones = {.safe = 0xff4e9a3c, .warn = 0xffe08a1e, .danger = 0xffff2e1e},
};
}
