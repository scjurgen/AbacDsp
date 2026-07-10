#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kViridis{
    .name = "Viridis",

    .background = 0xff383f77,
    .backgroundComponent = 0xff3e2066,
    .backgroundDark = 0xff339183,
    .backgroundMid = 0xff335f89,
    .backgroundLight = 0xffc2d93e,
    .gradientDark = 0xff383f77,
    .knobGradientStart = 0xff440154,
    .knobGradientCenter = 0xff335f89,
    .knobGradientEnd = 0xff87cb56,
    .statusOutline = 0xff87cb56,
    .labelColour = 0xff87cb56,

    .spectrogramStops = {{{0.0f, 0xff440154}, {0.33f, 0xff31688e}, {0.66f, 0xff35b779}, {1.0f, 0xfffde725}}},
    .spectrogramStopCount = 4,

    .cpuZones = {.safe = 0xff35b779, .warn = 0xffe3c62a, .danger = 0xffd94545},
    .levelZones = {.safe = 0xff35b779, .warn = 0xffe3c62a, .danger = 0xffd94545},
};
}
