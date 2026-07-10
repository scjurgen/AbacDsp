#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kTeal{
    .name = "Teal",

    .background = 0xffd3edea,
    .backgroundComponent = 0xffe9f6f5,
    .backgroundDark = 0xff59a49c,
    .backgroundMid = 0xffbde4e0,
    .backgroundLight = 0xff00312d,
    .gradientDark = 0xffd3edea,
    .knobGradientStart = 0xffffffff,
    .knobGradientCenter = 0xffbde4e0,
    .knobGradientEnd = 0xff004740,
    .statusOutline = 0xff004740,
    .labelColour = 0xff004740,

    .spectrogramStops = {{{0.0f, 0xffffffff}, {0.35f, 0xffb2dfdb}, {0.65f, 0xff00695c}, {1.0f, 0xff001a1a}}},
    .spectrogramStopCount = 4,

    .cpuZones = {.safe = 0xff2e9e6e, .warn = 0xffc9a23c, .danger = 0xffc04038},
    .levelZones = {.safe = 0xff2e9e6e, .warn = 0xffc9a23c, .danger = 0xffc04038},
};
}
