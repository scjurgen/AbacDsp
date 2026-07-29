#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "../ThemeOrbit.h"
#include "ThemeClassic.h"
#include "ThemeGrayscale.h"
#include "ThemeHeat.h"
#include "ThemeInferno.h"
#include "ThemeInk.h"
#include "ThemeTeal.h"
#include "ThemeViridis.h"

namespace Themes
{
enum class LegacyTheme
{
    Classic,
    Viridis,
    Inferno,
    Grayscale,
    Heat,
    Ink,
    Teal,
};

inline constexpr auto kLegacyThemes = std::to_array<ThemeDefinition>({
    kClassic,
    kViridis,
    kInferno,
    kGrayscale,
    kHeat,
    kInk,
    kTeal,
});

[[nodiscard]] constexpr const ThemeDefinition& legacyDefinition(const LegacyTheme theme)
{
    return kLegacyThemes[static_cast<size_t>(theme)];
}

// A selection packs a hue step (0..kHueCount-1, 30 degrees apart), a light/dark mode, and
// a base colour family (ui::ThemeFamily) into one index, so it still persists as a single
// int (AppSettings): hueIdx + mode*kHueCount + family*kSlotsPerFamily.
inline constexpr int kHueCount = 12;
inline constexpr int kHueStepDeg = 360 / kHueCount;
inline constexpr int kSlotsPerFamily = kHueCount * 2;

// The 12 hue steps land exactly on the standard 12-part hue wheel (Bichromatic's Primary
// has no hue correction applied to its hue itself, only to lightness/chroma), so these
// names are precise, not approximate.
inline constexpr std::array<const char*, kHueCount> kHueNames{
    "Magenta", "Rose", "Red", "Orange", "Yellow", "Chartreuse", "Green", "Mint", "Cyan", "Azure", "Blue", "Violet",
};

enum class Theme : int
{
};

[[nodiscard]] constexpr int hueIndex(const Theme theme)
{
    return static_cast<int>(theme) % kHueCount;
}

[[nodiscard]] constexpr bool isDark(const Theme theme)
{
    return (static_cast<int>(theme) % kSlotsPerFamily) >= kHueCount;
}

[[nodiscard]] constexpr ui::ThemeFamily family(const Theme theme)
{
    return static_cast<ui::ThemeFamily>(static_cast<int>(theme) / kSlotsPerFamily);
}

[[nodiscard]] constexpr double hueDegrees(const Theme theme)
{
    return static_cast<double>(hueIndex(theme) * kHueStepDeg);
}

[[nodiscard]] constexpr Theme makeTheme(const int hueIdx, const bool dark,
                                        const ui::ThemeFamily fam = ui::ThemeFamily::Bichromatic)
{
    return static_cast<Theme>(hueIdx + (dark ? kHueCount : 0) + static_cast<int>(fam) * kSlotsPerFamily);
}

[[nodiscard]] constexpr Theme withHue(const Theme theme, const int hueIdx)
{
    return makeTheme(hueIdx, isDark(theme), family(theme));
}

[[nodiscard]] constexpr Theme withMode(const Theme theme, const bool dark)
{
    return makeTheme(hueIndex(theme), dark, family(theme));
}

[[nodiscard]] constexpr Theme withFamily(const Theme theme, const ui::ThemeFamily fam)
{
    return makeTheme(hueIndex(theme), isDark(theme), fam);
}

[[nodiscard]] inline uint32_t toArgb(const ui::Rgb& color)
{
    const auto toByte = [](const double v) -> uint32_t
    { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    return 0xFF000000u | (toByte(color.r) << 16) | (toByte(color.g) << 8) | toByte(color.b);
}

// Scales an sRGB colour's channels toward black (factor < 1) or away from it
// (factor > 1, clamped), preserving alpha. Used to derive shading/depth tones
// (knob highlight/shadow, recessed track) that Orbit itself has no role for.
[[nodiscard]] inline uint32_t scaleLightness(const uint32_t argb, const double factor)
{
    const auto channel = [&](const int shift) -> uint32_t
    {
        const auto value = static_cast<double>((argb >> shift) & 0xFFu);
        return static_cast<uint32_t>(std::lround(std::clamp(value * factor, 0.0, 255.0)));
    };
    return (argb & 0xFF000000u) | (channel(16) << 16) | (channel(8) << 8) | channel(0);
}

[[nodiscard]] inline uint32_t mixColors(const uint32_t a, const uint32_t b, const double t)
{
    const auto mixChannel = [&](const int shift) -> uint32_t
    {
        const auto va = static_cast<double>((a >> shift) & 0xFFu);
        const auto vb = static_cast<double>((b >> shift) & 0xFFu);
        return static_cast<uint32_t>(std::lround(std::clamp(va + (vb - va) * t, 0.0, 255.0)));
    };
    return 0xFF000000u | (mixChannel(16) << 16) | (mixChannel(8) << 8) | mixChannel(0);
}

struct Hsl
{
    double h{};
    double s{};
    double l{};
};

[[nodiscard]] inline Hsl toHsl(const uint32_t argb)
{
    const double r = static_cast<double>((argb >> 16) & 0xFFu) / 255.0;
    const double g = static_cast<double>((argb >> 8) & 0xFFu) / 255.0;
    const double b = static_cast<double>(argb & 0xFFu) / 255.0;

    const std::array<double, 3> channels{r, g, b};
    const auto maxIt = std::max_element(channels.begin(), channels.end());
    const auto minIt = std::min_element(channels.begin(), channels.end());
    const double maxC = *maxIt;
    const double minC = *minIt;
    const double l = (maxC + minC) / 2.0;
    const double d = maxC - minC;

    if (d <= std::numeric_limits<double>::epsilon())
    {
        return {.h = 0.0, .s = 0.0, .l = l};
    }

    const double s = l > 0.5 ? d / (2.0 - maxC - minC) : d / (maxC + minC);
    const auto maxIdx = std::distance(channels.begin(), maxIt);

    double h{};
    if (maxIdx == 0)
    {
        h = (g - b) / d + (g < b ? 6.0 : 0.0);
    }
    else if (maxIdx == 1)
    {
        h = (b - r) / d + 2.0;
    }
    else
    {
        h = (r - g) / d + 4.0;
    }

    return {.h = h / 6.0, .s = s, .l = l};
}

[[nodiscard]] inline double hueToRgbChannel(const double p, const double q, double t)
{
    if (t < 0.0)
    {
        t += 1.0;
    }
    if (t > 1.0)
    {
        t -= 1.0;
    }
    if (t < 1.0 / 6.0)
    {
        return p + (q - p) * 6.0 * t;
    }
    if (t < 0.5)
    {
        return q;
    }
    if (t < 2.0 / 3.0)
    {
        return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    }
    return p;
}

[[nodiscard]] inline uint32_t fromHsl(const Hsl& hsl)
{
    const auto toByte = [](const double v) -> uint32_t
    { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };

    if (hsl.s <= std::numeric_limits<double>::epsilon())
    {
        const uint32_t v = toByte(hsl.l);
        return 0xFF000000u | (v << 16) | (v << 8) | v;
    }

    const double q = hsl.l < 0.5 ? hsl.l * (1.0 + hsl.s) : hsl.l + hsl.s - hsl.l * hsl.s;
    const double p = 2.0 * hsl.l - q;

    const uint32_t r = toByte(hueToRgbChannel(p, q, hsl.h + 1.0 / 3.0));
    const uint32_t g = toByte(hueToRgbChannel(p, q, hsl.h));
    const uint32_t b = toByte(hueToRgbChannel(p, q, hsl.h - 1.0 / 3.0));

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// Sets lightness to a fixed target (not just a floor - a source hue already lighter than
// the target would otherwise be untouched) and scales saturation, in HSL space - unlike a
// plain RGB tint toward white/grey (which lightens and desaturates together), this keeps
// the colour's hue identity so it reads as a pastel, not a wash of grey.
[[nodiscard]] inline uint32_t pastelize(const uint32_t argb, const double targetLightness,
                                        const double saturationFactor)
{
    Hsl hsl = toHsl(argb);
    hsl.l = targetLightness;
    hsl.s = std::clamp(hsl.s * saturationFactor, 0.0, 1.0);
    return fromHsl(hsl);
}

[[nodiscard]] inline double relativeLuminance(const uint32_t argb)
{
    const auto linearChannel = [&](const int shift) -> double
    {
        const double c = static_cast<double>((argb >> shift) & 0xFFu) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linearChannel(16) + 0.7152 * linearChannel(8) + 0.0722 * linearChannel(0);
}

[[nodiscard]] inline double contrastRatio(const uint32_t a, const uint32_t b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// Walks the theme's own spectrogram gradient from its bright top down toward its dark
// bottom, returning the first stop that clears WCAG AA (4.5:1) against `background` -
// stays as close to "the gradient's highlight colour" as contrast allows.
[[nodiscard]] inline uint32_t pickAccessibleGradientStop(const ui::Theme& orbit, const uint32_t background)
{
    constexpr double kMinAaContrast = 4.5;
    const auto stops = orbit.spectrogramStopsRgb();

    for (auto it = stops.rbegin(); it != stops.rend(); ++it)
    {
        const uint32_t candidate = toArgb(*it);
        if (contrastRatio(candidate, background) >= kMinAaContrast)
        {
            return candidate;
        }
    }

    return toArgb(stops.front());
}

// Position `t` (0=bottom/coolest, 1=top/brightest) along the theme's own spectrogram
// gradient - reused for the meter zones since that same low-to-high ramp already
// matches "low intensity -> high intensity", i.e. safe -> warn -> danger.
[[nodiscard]] inline uint32_t gradientStopAt(const ui::Theme& orbit, const double t)
{
    const auto stops = orbit.spectrogramStopsRgb();
    const auto idx = static_cast<size_t>(std::lround(std::clamp(t, 0.0, 1.0) * static_cast<double>(stops.size() - 1)));
    return toArgb(stops[idx]);
}

[[nodiscard]] inline MeterZoneColors zonesFromOrbit(const ui::Theme& orbit)
{
    return {
        .safe = gradientStopAt(orbit, 0.35),
        .warn = gradientStopAt(orbit, 0.65),
        .danger = gradientStopAt(orbit, 0.95),
    };
}

[[nodiscard]] inline std::array<GradientStop, 14> spectrogramStopsFromOrbit(const ui::Theme& orbit, size_t& outCount)
{
    const auto orbitStops = orbit.spectrogramStopsRgb();
    std::array<GradientStop, 14> stops{};
    outCount = std::min(stops.size(), orbitStops.size());

    for (size_t i = 0; i < outCount; ++i)
    {
        const double t = outCount > 1 ? static_cast<double>(i) / static_cast<double>(outCount - 1) : 0.0;
        stops[i] = {.position = static_cast<float>(t), .argb = toArgb(orbitStops[i])};
    }

    return stops;
}

[[nodiscard]] inline ThemeDefinition definition(const Theme theme)
{
    ui::Theme orbit;
    orbit.setFamily(family(theme));
    orbit.setMode(isDark(theme) ? ui::ThemeMode::Dark : ui::ThemeMode::Light);
    orbit.setRelativeHueDegrees(hueDegrees(theme));

    ThemeDefinition def{};
    def.name = "Orbit";
    def.background = toArgb(orbit.rgb(ui::ThemeRole::Background0));
    def.backgroundComponent = toArgb(orbit.rgb(ui::ThemeRole::Background1));
    def.backgroundDark = toArgb(orbit.rgb(ui::ThemeRole::Background2));
    def.backgroundMid = toArgb(orbit.rgb(ui::ThemeRole::Background1));
    def.backgroundLight = toArgb(orbit.rgb(ui::ThemeRole::Background0));
    // Near WCAG's dark end the contrast-ratio formula's +0.05 term compresses hard: even
    // pure black only reaches ~1.08 against an already near-black backgroundDark, so Dark
    // mode has to move lighter (toward the page) to gain real contrast. Light mode instead
    // pastelizes Hover (raised lightness, preserved saturation) rather than plain-graying
    // it - targeted relative to backgroundDark's own lightness (which drifts a little per
    // hue) rather than a fixed constant, so contrast against it stays consistent everywhere.
    const double backgroundDarkLightness = toHsl(def.backgroundDark).l;
    def.gradientDark = isDark(theme) ? mixColors(def.backgroundDark, def.background, 0.85)
                                     : pastelize(toArgb(orbit.rgb(ui::ThemeRole::Hover)),
                                                 std::clamp(backgroundDarkLightness - 0.38, 0.0, 1.0), 0.85);
    def.labelColour = toArgb(orbit.rgb(ui::ThemeRole::Text));
    def.statusOutline = pickAccessibleGradientStop(orbit, def.background);
    def.knobGradientStart = scaleLightness(toArgb(orbit.rgb(ui::ThemeRole::Primary)), 1.35);
    // Sole source of the knob disk fill (drawRotarySlider); pastelized (not just muted
    // toward grey) in light mode so statusOutline's value ring still reads as more
    // important, without the disk itself looking like a flat grey blob.
    def.knobGradientCenter = isDark(theme) ? toArgb(orbit.rgb(ui::ThemeRole::Hover))
                                           : pastelize(toArgb(orbit.rgb(ui::ThemeRole::Hover)), 0.80, 0.85);
    def.knobGradientEnd = scaleLightness(toArgb(orbit.rgb(ui::ThemeRole::Primary)), 0.65);
    def.cpuZones = zonesFromOrbit(orbit);
    def.levelZones = def.cpuZones;
    def.spectrogramStops = spectrogramStopsFromOrbit(orbit, def.spectrogramStopCount);

    return def;
}
}
