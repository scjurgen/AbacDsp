#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kInferno{
    .name = "Inferno",

    .background = 0xff340a44,
    .backgroundComponent = 0xff1a0524,
    .backgroundDark = 0xffa52e5a,
    .backgroundMid = 0xff4e0f64,
    .backgroundLight = 0xfffbd266,
    .gradientDark = 0xff340a44,
    .knobGradientStart = 0xff000004,
    .knobGradientCenter = 0xff4e0f64,
    .knobGradientEnd = 0xfffaa528,
    .statusOutline = 0xfffaa528,
    .labelColour = 0xfffaa528,

    .spectrogramStops =
        {{{0.0f, 0xff000004}, {0.33f, 0xff56106e}, {0.55f, 0xffbc3754}, {0.75f, 0xfff98e09}, {1.0f, 0xfffcffa4}}},
    .spectrogramStopCount = 5,

    .cpuZones = {.safe = 0xff8aa03c, .warn = 0xfff98e09, .danger = 0xffe0303c},
    .levelZones = {.safe = 0xff8aa03c, .warn = 0xfff98e09, .danger = 0xffe0303c},
};
}
