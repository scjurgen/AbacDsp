#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Themes
{
struct GradientStop
{
    float position{};
    uint32_t argb{};
};

struct MeterZoneColors
{
    uint32_t safe{};
    uint32_t warn{};
    uint32_t danger{};
};

struct ThemeDefinition
{
    const char* name{};

    uint32_t background{};
    uint32_t backgroundComponent{};
    uint32_t backgroundDark{};
    uint32_t backgroundMid{};
    uint32_t backgroundLight{};
    uint32_t gradientDark{};
    uint32_t knobGradientStart{};
    uint32_t knobGradientCenter{};
    uint32_t knobGradientEnd{};
    uint32_t statusOutline{};
    uint32_t labelColour{};

    std::array<GradientStop, 14> spectrogramStops{};
    size_t spectrogramStopCount{};

    MeterZoneColors cpuZones{};
    MeterZoneColors levelZones{};
};
}
