#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kGrayscale{
    .name = "Grayscale",

    .background = 0xff333333,
    .backgroundComponent = 0xff1a1a1a,
    .backgroundDark = 0xff808080,
    .backgroundMid = 0xff4d4d4d,
    .backgroundLight = 0xffe6e6e6,
    .gradientDark = 0xff333333,
    .knobGradientStart = 0xff000000,
    .knobGradientCenter = 0xff4d4d4d,
    .knobGradientEnd = 0xffcccccc,
    .statusOutline = 0xffcccccc,
    .labelColour = 0xffcccccc,

    .spectrogramStops = {{{0.0f, 0xff000000}, {1.0f, 0xffffffff}}},
    .spectrogramStopCount = 2,

    .cpuZones = {.safe = 0xff808080, .warn = 0xffb3b3b3, .danger = 0xffe6e6e6},
    .levelZones = {.safe = 0xff808080, .warn = 0xffb3b3b3, .danger = 0xffe6e6e6},
};
}
