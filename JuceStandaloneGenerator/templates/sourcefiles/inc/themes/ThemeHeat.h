#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kHeat{
    .name = "Heat",

    .background = 0xffffddbb,
    .backgroundComponent = 0xffffeedd,
    .backgroundDark = 0xffdd6633,
    .backgroundMid = 0xffffcc99,
    .backgroundLight = 0xff4d0000,
    .gradientDark = 0xffffddbb,
    .knobGradientStart = 0xffffffff,
    .knobGradientCenter = 0xffffcc99,
    .knobGradientEnd = 0xff7a0a00,
    .statusOutline = 0xff7a0a00,

    .spectrogramStops =
        {{{0.0f, 0xffffffff}, {0.30f, 0xffffcc99}, {0.60f, 0xffcc3300}, {0.85f, 0xff660000}, {1.0f, 0xff1a0000}}},
    .spectrogramStopCount = 5,

    .cpuZones = {.safe = 0xff6f9e46, .warn = 0xffe0862e, .danger = 0xffb31212},
    .levelZones = {.safe = 0xff6f9e46, .warn = 0xffe0862e, .danger = 0xffb31212},
};
}
