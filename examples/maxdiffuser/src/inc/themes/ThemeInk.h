#pragma once

#include "ThemeTypes.h"

namespace Themes
{
inline constexpr ThemeDefinition kInk{
    .name = "Ink",

    .background = 0xffe0d6f2,
    .backgroundComponent = 0xffefebf8,
    .backgroundDark = 0xff8a6cc8,
    .backgroundMid = 0xffd0c2eb,
    .backgroundLight = 0xff1f0a48,
    .gradientDark = 0xffe0d6f2,
    .knobGradientStart = 0xffffffff,
    .knobGradientCenter = 0xffd0c2eb,
    .knobGradientEnd = 0xff30136e,
    .statusOutline = 0xff30136e,

    .spectrogramStops = {{{0.0f, 0xffffffff}, {0.35f, 0xffc8b8e8}, {0.65f, 0xff4b1fa8}, {1.0f, 0xff0d0221}}},
    .spectrogramStopCount = 4,

    .cpuZones = {.safe = 0xff4e9a78, .warn = 0xffc98f3c, .danger = 0xffc03060},
    .levelZones = {.safe = 0xff4e9a78, .warn = 0xffc98f3c, .danger = 0xffc03060},
};
}
