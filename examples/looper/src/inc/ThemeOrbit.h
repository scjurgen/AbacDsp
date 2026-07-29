#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>
/* usage
#include "Theme.h"

ui::Theme theme {};
theme.setMode(ui::ThemeMode::Dark);
theme.setRelativeHueDegrees(24.0);

const std::string primary = theme.hex(ui::ThemeRole::Primary);
const std::string background = theme.hex(ui::ThemeRole::Background0);
const std::string text = theme.hex(ui::ThemeRole::Text);
const std::string onPrimary = theme.onPrimaryHex();
const std::string spectrogram = theme.spectrogramCssGradient();
*/
namespace ui
{

struct Rgb
{
    double r{};
    double g{};
    double b{};
};

struct Oklab
{
    double L{};
    double a{};
    double b{};
};

struct Oklch
{
    double L{};
    double C{};
    double hDeg{};
};

enum class ThemeMode : std::uint32_t
{
    Light = 0,
    Dark
};

// Monochromatic is reserved for a future single-hue (tints/shades only) family and not yet
// implemented - makeDefaultColors()/makeDefaultSpectrogram() fall back to Bichromatic for it.
enum class ThemeFamily : std::uint32_t
{
    Monochromatic = 0,
    Bichromatic,
    Trichromatic
};

enum class ThemeRole : std::uint32_t
{
    Primary = 0,
    Hover,
    Background0,
    Background1,
    Background2,
    Text,
    TextMuted,
    AccentInfo,
    AccentSuccess,
    AccentWarning,
    AccentDanger,
    Count
};

enum class ThemeGroup : std::uint32_t
{
    Interactive = 0,
    Surface,
    Content,
    Accent
};

struct ThemeColor
{
    ThemeRole role{};
    ThemeGroup group{};
    Rgb baseRgbSrgb{};
    Oklch baseOklch{};
    double hueWeight{1.0};
    double chromaWeight{1.0};
    double lightnessWeight{0.0};
    double hueBiasDeg{0.0};
};

class Theme
{
  public:
    Theme()
        : m_colors{makeDefaultColors(m_family)}
        , m_currentRgbSrgb(static_cast<std::size_t>(ThemeRole::Count))
        , m_spectrogramBaseOklch(makeDefaultSpectrogram(m_family))
        , m_spectrogramCurrentRgbSrgb(m_spectrogramBaseOklch.size())
    {
        rebuild();
    }

    void setMode(const ThemeMode mode) noexcept
    {
        m_mode = mode;
        rebuild();
    }

    [[nodiscard]] ThemeMode mode() const noexcept
    {
        return m_mode;
    }

    void setFamily(const ThemeFamily family) noexcept
    {
        m_family = family;
        m_colors = makeDefaultColors(m_family);
        m_spectrogramBaseOklch = makeDefaultSpectrogram(m_family);
        m_spectrogramCurrentRgbSrgb.resize(m_spectrogramBaseOklch.size());
        rebuild();
    }

    [[nodiscard]] ThemeFamily family() const noexcept
    {
        return m_family;
    }

    void setRelativeHueDegrees(const double hueDeg) noexcept
    {
        m_relativeHueDeg = hueDeg;
        rebuild();
    }

    [[nodiscard]] double relativeHueDegrees() const noexcept
    {
        return m_relativeHueDeg;
    }

    [[nodiscard]] Rgb rgb(const ThemeRole role) const noexcept
    {
        return m_currentRgbSrgb.at(toIndex(role));
    }

    [[nodiscard]] std::string hex(const ThemeRole role) const
    {
        return toHex(rgb(role));
    }

    [[nodiscard]] Oklch oklch(const ThemeRole role) const noexcept
    {
        return srgbToOklch(rgb(role));
    }

    [[nodiscard]] Rgb onPrimaryRgb() const noexcept
    {
        return readableForeground(rgb(ThemeRole::Primary));
    }

    [[nodiscard]] std::string onPrimaryHex() const
    {
        return toHex(onPrimaryRgb());
    }

    [[nodiscard]] Rgb onHoverRgb() const noexcept
    {
        return readableForeground(rgb(ThemeRole::Hover));
    }

    [[nodiscard]] std::string onHoverHex() const
    {
        return toHex(onHoverRgb());
    }

    [[nodiscard]] Rgb onAccentRgb(const ThemeRole role) const noexcept
    {
        return readableForeground(rgb(role));
    }

    [[nodiscard]] std::string onAccentHex(const ThemeRole role) const
    {
        return toHex(onAccentRgb(role));
    }

    [[nodiscard]] std::vector<Rgb> spectrogramStopsRgb() const
    {
        return m_spectrogramCurrentRgbSrgb;
    }

    [[nodiscard]] std::vector<std::string> spectrogramStopsHex() const
    {
        std::vector<std::string> result;
        result.reserve(m_spectrogramCurrentRgbSrgb.size());

        for (const Rgb& color : m_spectrogramCurrentRgbSrgb)
        {
            result.push_back(toHex(color));
        }

        return result;
    }

    [[nodiscard]] std::string spectrogramCssGradient() const
    {
        const auto stops = spectrogramStopsHex();
        std::string result{"linear-gradient(to top"};

        for (std::size_t i = 0; i < stops.size(); ++i)
        {
            const double t =
                stops.size() > 1 ? (static_cast<double>(i) / static_cast<double>(stops.size() - 1)) * 100.0 : 0.0;

            result += std::format(", {} {:.3f}%", stops[i], t);
        }

        result += ')';
        return result;
    }

  private:
    struct HueCorrection
    {
        double deltaL{};
        double chromaScale{1.0};
    };

    struct ToneBand
    {
        double targetL{};
        double maxC{};
    };

    static constexpr double kPi = std::numbers::pi_v<double>;
    static constexpr double kDegToRad = kPi / 180.0;
    static constexpr double kRadToDeg = 180.0 / kPi;

    ThemeMode m_mode{ThemeMode::Light};
    ThemeFamily m_family{ThemeFamily::Bichromatic};
    double m_relativeHueDeg{};
    std::vector<ThemeColor> m_colors;
    std::vector<Rgb> m_currentRgbSrgb;
    std::vector<Oklch> m_spectrogramBaseOklch;
    std::vector<Rgb> m_spectrogramCurrentRgbSrgb;

    [[nodiscard]] static constexpr std::size_t toIndex(const ThemeRole role) noexcept
    {
        return static_cast<std::size_t>(role);
    }

    void rebuild() noexcept
    {
        for (const ThemeColor& color : m_colors)
        {
            const Oklch transformed = transform(color);
            m_currentRgbSrgb[toIndex(color.role)] = oklchToSrgbGamutMapped(transformed);
        }

        rebuildSpectrogram();
    }

    void rebuildSpectrogram() noexcept
    {
        std::vector<Oklch> transformedStops;
        transformedStops.reserve(m_spectrogramBaseOklch.size());

        for (const Oklch& stop : m_spectrogramBaseOklch)
        {
            Oklch transformed = stop;
            transformed.hDeg = wrapHue(stop.hDeg + m_relativeHueDeg);

            const HueCorrection correction = evaluateHueCorrection(transformed.hDeg);
            const double chromaPreserve = chromaPreservationFactor(stop.C, 0.10);

            transformed.C *= lerp(1.0, correction.chromaScale, 0.90 * chromaPreserve);
            transformed.L = clamp01(stop.L + correction.deltaL * 0.35 * chromaPreserve);
            transformed = applySpectrogramModeMapping(transformed);
            transformedStops.push_back(transformed);
        }

        enforceMonotonicLightness(transformedStops);

        for (std::size_t i = 0; i < transformedStops.size(); ++i)
        {
            m_spectrogramCurrentRgbSrgb[i] = oklchToSrgbGamutMapped(transformedStops[i]);
        }
    }

    [[nodiscard]] Oklch transform(const ThemeColor& color) const noexcept
    {
        Oklch value = color.baseOklch;
        value.hDeg = wrapHue(color.baseOklch.hDeg + m_relativeHueDeg * color.hueWeight + color.hueBiasDeg);

        const HueCorrection correction = evaluateHueCorrection(value.hDeg);
        const double chromaPreserve = chromaPreservationFactor(color.baseOklch.C, 0.08);

        value.C *= lerp(1.0, correction.chromaScale, color.chromaWeight * chromaPreserve);
        value.L = clamp01(color.baseOklch.L + correction.deltaL * color.lightnessWeight * chromaPreserve);

        value = applyModeMapping(color, value);
        applyRoleConstraints(color, value);

        return value;
    }

    [[nodiscard]] Oklch applyModeMapping(const ThemeColor& color, Oklch value) const noexcept
    {
        const ToneBand band = toneBand(m_mode, color.group);

        double lBlend = 0.0;
        double cBlend = 0.0;

        switch (color.group)
        {
            case ThemeGroup::Surface:
                lBlend = 0.88;
                cBlend = 0.92;
                break;
            case ThemeGroup::Content:
                lBlend = 0.86;
                cBlend = 0.78;
                break;
            case ThemeGroup::Interactive:
                lBlend = 0.58;
                cBlend = 0.42;
                break;
            case ThemeGroup::Accent:
                lBlend = 0.46;
                cBlend = 0.36;
                break;
        }

        if (color.role == ThemeRole::Text)
        {
            value.L = lerp(value.L, textTargetLightness(), 0.92);
            value.C *= 0.25;
            return value;
        }

        if (color.role == ThemeRole::TextMuted)
        {
            value.L = lerp(value.L, mutedTextTargetLightness(), 0.90);
            value.C *= 0.35;
            return value;
        }

        // Surface roles all share one tone-band target; blending that strongly (needed so
        // Dark mode reliably lands near-black) otherwise crushes Background0/1/2 into
        // near-identical tones. Offsetting each role's own target keeps that polarity while
        // restoring a visible page/card/recess step in both modes.
        const double targetL = band.targetL - surfaceTierOffset(color.role);

        value.L = lerp(value.L, targetL, lBlend);
        value.C = std::min(lerp(value.C, std::min(value.C, band.maxC), cBlend), band.maxC);

        return value;
    }

    [[nodiscard]] static double surfaceTierOffset(const ThemeRole role) noexcept
    {
        if (role == ThemeRole::Background1)
        {
            return 0.06;
        }

        if (role == ThemeRole::Background2)
        {
            return 0.14;
        }

        return 0.0;
    }

    [[nodiscard]] Oklch applySpectrogramModeMapping(Oklch value) const noexcept
    {
        if (m_mode == ThemeMode::Light)
        {
            value.L = clamp01(0.10 + value.L * 0.82);
            value.C *= 0.94;
        }
        else
        {
            value.L = clamp01(0.04 + value.L * 0.92);
            value.C *= 0.90;
        }

        return value;
    }

    static void applyRoleConstraints(const ThemeColor& color, Oklch& value) noexcept
    {
        switch (color.group)
        {
            case ThemeGroup::Surface:
                value.C = std::min(value.C, 0.028);
                value.L = clamp(value.L, 0.03, 0.985);
                break;
            case ThemeGroup::Content:
                value.C = std::min(value.C, 0.040);
                value.L = clamp(value.L, 0.02, 0.985);
                break;
            case ThemeGroup::Interactive:
                value.C = std::min(value.C, 0.19);
                value.L = clamp(value.L, 0.10, 0.92);
                break;
            case ThemeGroup::Accent:
                value.C = std::min(value.C, 0.18);
                value.L = clamp(value.L, 0.12, 0.95);
                break;
        }

        if (color.baseOklch.C < 0.03)
        {
            value.C *= 0.25;
        }
    }

    [[nodiscard]] static ToneBand toneBand(const ThemeMode mode, const ThemeGroup group) noexcept
    {
        if (mode == ThemeMode::Light)
        {
            switch (group)
            {
                case ThemeGroup::Surface:
                    return {0.955, 0.020};
                case ThemeGroup::Content:
                    return {0.180, 0.025};
                case ThemeGroup::Interactive:
                    return {0.420, 0.180};
                case ThemeGroup::Accent:
                    return {0.520, 0.160};
            }
        }
        else
        {
            switch (group)
            {
                case ThemeGroup::Surface:
                    return {0.175, 0.018};
                case ThemeGroup::Content:
                    return {0.920, 0.025};
                case ThemeGroup::Interactive:
                    return {0.660, 0.140};
                case ThemeGroup::Accent:
                    return {0.710, 0.130};
            }
        }

        return {0.5, 0.1};
    }

    [[nodiscard]] double textTargetLightness() const noexcept
    {
        return m_mode == ThemeMode::Light ? 0.16 : 0.94;
    }

    [[nodiscard]] double mutedTextTargetLightness() const noexcept
    {
        return m_mode == ThemeMode::Light ? 0.42 : 0.72;
    }

    [[nodiscard]] static double chromaPreservationFactor(const double chroma, const double fullStrengthAt) noexcept
    {
        return clamp01(chroma / fullStrengthAt);
    }

    [[nodiscard]] static HueCorrection evaluateHueCorrection(const double hueDeg) noexcept
    {
        const double yellow = gaussianHue(hueDeg, 95.0, 28.0);
        const double orange = gaussianHue(hueDeg, 55.0, 24.0);
        const double blue = gaussianHue(hueDeg, 255.0, 30.0);
        const double violet = gaussianHue(hueDeg, 315.0, 26.0);
        const double green = gaussianHue(hueDeg, 145.0, 22.0);

        HueCorrection result{};
        result.deltaL = (-0.030 * yellow) + (-0.012 * orange) + (+0.018 * blue) + (+0.010 * violet) + (-0.004 * green);

        result.chromaScale =
            1.0 + (-0.12 * yellow) + (-0.06 * orange) + (+0.08 * blue) + (+0.05 * violet) + (-0.02 * green);

        result.chromaScale = std::max(0.55, result.chromaScale);
        return result;
    }

    [[nodiscard]] static double gaussianHue(const double hueDeg, const double centerDeg, const double sigmaDeg) noexcept
    {
        const double distance = angularDistanceDegrees(hueDeg, centerDeg);
        const double x = distance / sigmaDeg;
        return std::exp(-0.5 * x * x);
    }

    [[nodiscard]] static double angularDistanceDegrees(const double aDeg, const double bDeg) noexcept
    {
        const double d = std::fabs(wrapHue(aDeg - bDeg));
        return std::min(d, 360.0 - d);
    }

    [[nodiscard]] static double wrapHue(const double hueDeg) noexcept
    {
        double h = std::fmod(hueDeg, 360.0);

        if (h < 0.0)
        {
            h += 360.0;
        }

        return h;
    }

    [[nodiscard]] static double clamp01(const double v) noexcept
    {
        return std::clamp(v, 0.0, 1.0);
    }

    [[nodiscard]] static double clamp(const double v, const double lo, const double hi) noexcept
    {
        return std::clamp(v, lo, hi);
    }

    [[nodiscard]] static double lerp(const double a, const double b, const double t) noexcept
    {
        return a + (b - a) * t;
    }

    [[nodiscard]] static double srgbToLinear(const double c) noexcept
    {
        if (c <= 0.04045)
        {
            return c / 12.92;
        }

        return std::pow((c + 0.055) / 1.055, 2.4);
    }

    [[nodiscard]] static double linearToSrgb(const double c) noexcept
    {
        if (c <= 0.0031308)
        {
            return 12.92 * c;
        }

        return 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    }

    [[nodiscard]] static Rgb srgbToLinearRgb(const Rgb& srgb) noexcept
    {
        return {.r = srgbToLinear(srgb.r), .g = srgbToLinear(srgb.g), .b = srgbToLinear(srgb.b)};
    }

    [[nodiscard]] static Rgb linearRgbToSrgb(const Rgb& linear) noexcept
    {
        return {.r = linearToSrgb(linear.r), .g = linearToSrgb(linear.g), .b = linearToSrgb(linear.b)};
    }

    [[nodiscard]] static Oklab linearSrgbToOklab(const Rgb& linear) noexcept
    {
        const double l = 0.4122214708 * linear.r + 0.5363325363 * linear.g + 0.0514459929 * linear.b;
        const double m = 0.2119034982 * linear.r + 0.6806995451 * linear.g + 0.1073969566 * linear.b;
        const double s = 0.0883024619 * linear.r + 0.2817188376 * linear.g + 0.6299787005 * linear.b;

        const double lCbrt = std::cbrt(l);
        const double mCbrt = std::cbrt(m);
        const double sCbrt = std::cbrt(s);

        return {.L = 0.2104542553 * lCbrt + 0.7936177850 * mCbrt - 0.0040720468 * sCbrt,
                .a = 1.9779984951 * lCbrt - 2.4285922050 * mCbrt + 0.4505937099 * sCbrt,
                .b = 0.0259040371 * lCbrt + 0.7827717662 * mCbrt - 0.8086757660 * sCbrt};
    }

    [[nodiscard]] static Rgb oklabToLinearSrgb(const Oklab& lab) noexcept
    {
        const double l_ = lab.L + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
        const double m_ = lab.L - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
        const double s_ = lab.L - 0.0894841775 * lab.a - 1.2914855480 * lab.b;

        const double l = l_ * l_ * l_;
        const double m = m_ * m_ * m_;
        const double s = s_ * s_ * s_;

        return {.r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
                .g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
                .b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s};
    }

    [[nodiscard]] static Oklch oklabToOklch(const Oklab& lab) noexcept
    {
        const double C = std::sqrt(lab.a * lab.a + lab.b * lab.b);
        double hDeg = std::atan2(lab.b, lab.a) * kRadToDeg;

        if (hDeg < 0.0)
        {
            hDeg += 360.0;
        }

        return {.L = lab.L, .C = C, .hDeg = hDeg};
    }

    [[nodiscard]] static Oklab oklchToOklab(const Oklch& lch) noexcept
    {
        const double hRad = lch.hDeg * kDegToRad;

        return {.L = lch.L, .a = lch.C * std::cos(hRad), .b = lch.C * std::sin(hRad)};
    }

    [[nodiscard]] static Oklch srgbToOklch(const Rgb& srgb) noexcept
    {
        return oklabToOklch(linearSrgbToOklab(srgbToLinearRgb(srgb)));
    }

    [[nodiscard]] static Rgb oklchToSrgbUnchecked(const Oklch& lch) noexcept
    {
        return linearRgbToSrgb(oklabToLinearSrgb(oklchToOklab(lch)));
    }

    [[nodiscard]] static bool isFinite(const Rgb& color) noexcept
    {
        return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b);
    }

    [[nodiscard]] static bool isInSrgbGamut(const Rgb& color) noexcept
    {
        return isFinite(color) && color.r >= 0.0 && color.r <= 1.0 && color.g >= 0.0 && color.g <= 1.0 &&
               color.b >= 0.0 && color.b <= 1.0;
    }

    [[nodiscard]] static Rgb clampSrgb(const Rgb& color) noexcept
    {
        return {.r = clamp01(color.r), .g = clamp01(color.g), .b = clamp01(color.b)};
    }

    [[nodiscard]] static Rgb oklchToSrgbGamutMapped(const Oklch& input) noexcept
    {
        Rgb srgb = oklchToSrgbUnchecked(input);

        if (isInSrgbGamut(srgb))
        {
            return srgb;
        }

        double low = 0.0;
        double high = input.C;
        Oklch candidate = input;
        Rgb best = oklchToSrgbUnchecked({.L = input.L, .C = 0.0, .hDeg = input.hDeg});

        for (int i = 0; i < 28; ++i)
        {
            const double mid = 0.5 * (low + high);
            candidate.C = mid;
            srgb = oklchToSrgbUnchecked(candidate);

            if (isInSrgbGamut(srgb))
            {
                best = srgb;
                low = mid;
            }
            else
            {
                high = mid;
            }
        }

        return clampSrgb(best);
    }

    [[nodiscard]] static double relativeLuminance(const Rgb& srgb) noexcept
    {
        const Rgb linear = srgbToLinearRgb(srgb);
        return 0.2126 * linear.r + 0.7152 * linear.g + 0.0722 * linear.b;
    }

    [[nodiscard]] static double contrastRatio(const Rgb& a, const Rgb& b) noexcept
    {
        const double la = relativeLuminance(a);
        const double lb = relativeLuminance(b);
        const double l1 = std::max(la, lb);
        const double l2 = std::min(la, lb);
        return (l1 + 0.05) / (l2 + 0.05);
    }

    [[nodiscard]] static Rgb readableForeground(const Rgb& background) noexcept
    {
        const Rgb light{.r = 0.98, .g = 0.98, .b = 0.99};
        const Rgb dark{.r = 0.08, .g = 0.08, .b = 0.09};

        const double lightContrast = contrastRatio(light, background);
        const double darkContrast = contrastRatio(dark, background);

        return lightContrast >= darkContrast ? light : dark;
    }

    static void enforceMonotonicLightness(std::vector<Oklch>& stops) noexcept
    {
        if (stops.empty())
        {
            return;
        }

        for (std::size_t i = 1; i < stops.size(); ++i)
        {
            if (stops[i].L < stops[i - 1].L)
            {
                stops[i].L = stops[i - 1].L;
            }
        }
    }

    [[nodiscard]] static std::string toHex(const Rgb& srgb)
    {
        const Rgb clamped = clampSrgb(srgb);

        const auto toByte = [](const double v) noexcept -> std::uint32_t
        { return static_cast<std::uint32_t>(std::lround(clamp01(v) * 255.0)); };

        return std::format("#{:02X}{:02X}{:02X}", toByte(clamped.r), toByte(clamped.g), toByte(clamped.b));
    }

    [[nodiscard]] static constexpr Rgb rgb8(const std::uint32_t r, const std::uint32_t g,
                                            const std::uint32_t b) noexcept
    {
        return {.r = static_cast<double>(r) / 255.0,
                .g = static_cast<double>(g) / 255.0,
                .b = static_cast<double>(b) / 255.0};
    }

    [[nodiscard]] static ThemeColor makeThemeColor(const ThemeRole role, const ThemeGroup group, const Rgb srgb,
                                                   const double hueWeight, const double chromaWeight,
                                                   const double lightnessWeight, const double hueBiasDeg = 0.0) noexcept
    {
        return {.role = role,
                .group = group,
                .baseRgbSrgb = srgb,
                .baseOklch = srgbToOklch(srgb),
                .hueWeight = hueWeight,
                .chromaWeight = chromaWeight,
                .lightnessWeight = lightnessWeight,
                .hueBiasDeg = hueBiasDeg};
    }

    // Bichromatic's own hue (rgb(45,20,45), OKLCH ~327 degrees) versus Trichromatic's seed
    // (rgb(45,20,20), OKLCH ~21 degrees): the palette-wide shift that carries Primary from
    // one to the other exactly.
    static constexpr double kTrichromaticHueShiftDeg = 53.6;

    [[nodiscard]] static std::vector<ThemeColor> makeBichromaticColors() noexcept
    {
        std::vector<ThemeColor> colors;
        colors.reserve(static_cast<std::size_t>(ThemeRole::Count));

        colors.push_back(
            makeThemeColor(ThemeRole::Primary, ThemeGroup::Interactive, rgb8(0x2D, 0x14, 0x2D), 1.00, 1.00, 0.65));
        colors.push_back(
            makeThemeColor(ThemeRole::Hover, ThemeGroup::Interactive, rgb8(0x3E, 0x1F, 0x3F), 1.00, 0.95, 0.60));

        colors.push_back(
            makeThemeColor(ThemeRole::Background0, ThemeGroup::Surface, rgb8(0xFA, 0xF7, 0xFB), 1.00, 0.20, 0.15));
        colors.push_back(
            makeThemeColor(ThemeRole::Background1, ThemeGroup::Surface, rgb8(0xF3, 0xEE, 0xF6), 1.00, 0.25, 0.18));
        colors.push_back(
            makeThemeColor(ThemeRole::Background2, ThemeGroup::Surface, rgb8(0xE8, 0xE0, 0xED), 1.00, 0.30, 0.20));

        colors.push_back(
            makeThemeColor(ThemeRole::Text, ThemeGroup::Content, rgb8(0x1D, 0x12, 0x1F), 0.55, 0.18, 0.10));
        colors.push_back(
            makeThemeColor(ThemeRole::TextMuted, ThemeGroup::Content, rgb8(0x5B, 0x48, 0x5F), 0.70, 0.30, 0.15));

        colors.push_back(
            makeThemeColor(ThemeRole::AccentInfo, ThemeGroup::Accent, rgb8(0x20, 0x7E, 0x91), 1.00, 1.00, 0.55));
        colors.push_back(
            makeThemeColor(ThemeRole::AccentSuccess, ThemeGroup::Accent, rgb8(0x31, 0x70, 0x4C), 1.00, 1.00, 0.45));
        colors.push_back(
            makeThemeColor(ThemeRole::AccentWarning, ThemeGroup::Accent, rgb8(0x9B, 0x68, 0x18), 1.00, 1.00, 0.55));
        colors.push_back(
            makeThemeColor(ThemeRole::AccentDanger, ThemeGroup::Accent, rgb8(0x97, 0x34, 0x5C), 1.00, 1.00, 0.55));

        return colors;
    }

    // Trichromatic reuses Bichromatic's whole palette (so every existing role relationship -
    // contrast, accent identities - carries over untouched), uniformly rotated so Primary's
    // hue lands exactly on rgb(45,20,20)'s own hue.
    [[nodiscard]] static std::vector<ThemeColor> makeTrichromaticColors() noexcept
    {
        std::vector<ThemeColor> colors = makeBichromaticColors();

        for (ThemeColor& color : colors)
        {
            color.baseOklch.hDeg = wrapHue(color.baseOklch.hDeg + kTrichromaticHueShiftDeg);
            color.baseRgbSrgb = oklchToSrgbGamutMapped(color.baseOklch);
        }

        return colors;
    }

    [[nodiscard]] static std::vector<ThemeColor> makeDefaultColors(const ThemeFamily family) noexcept
    {
        if (family == ThemeFamily::Trichromatic)
        {
            return makeTrichromaticColors();
        }

        return makeBichromaticColors();
    }

    [[nodiscard]] static std::vector<Oklch> makeBichromaticSpectrogram() noexcept
    {
        constexpr std::array<Rgb, 14> stops{rgb8(0x14, 0x0C, 0x16), rgb8(0x1B, 0x10, 0x20), rgb8(0x25, 0x15, 0x2B),
                                            rgb8(0x2D, 0x14, 0x2D), rgb8(0x3A, 0x1A, 0x39), rgb8(0x4A, 0x23, 0x45),
                                            rgb8(0x5D, 0x2F, 0x50), rgb8(0x72, 0x41, 0x5B), rgb8(0x88, 0x58, 0x65),
                                            rgb8(0xA0, 0x72, 0x6A), rgb8(0xBA, 0x8F, 0x64), rgb8(0xD2, 0xB0, 0x5E),
                                            rgb8(0xE8, 0xD4, 0x5F), rgb8(0xF5, 0xF2, 0xA6)};

        std::vector<Oklch> result;
        result.reserve(stops.size());

        for (const Rgb& stop : stops)
        {
            result.push_back(srgbToOklch(stop));
        }

        return result;
    }

    // Hand-authored (not derived by rotating Bichromatic's gradient): same lightness/chroma
    // progression, hue swept from near Trichromatic's own dark identity up to a light
    // blue-green top, independent of the palette-wide hue shift above.
    [[nodiscard]] static std::vector<Oklch> makeTrichromaticSpectrogram() noexcept
    {
        constexpr std::array<Rgb, 14> stops{rgb8(0x17, 0x0B, 0x12), rgb8(0x22, 0x0E, 0x14), rgb8(0x2F, 0x12, 0x15),
                                            rgb8(0x36, 0x13, 0x0D), rgb8(0x43, 0x1B, 0x05), rgb8(0x4E, 0x29, 0x00),
                                            rgb8(0x59, 0x3B, 0x00), rgb8(0x62, 0x52, 0x1A), rgb8(0x6B, 0x6B, 0x3D),
                                            rgb8(0x78, 0x86, 0x5E), rgb8(0x80, 0xA5, 0x76), rgb8(0x7A, 0xC9, 0x95),
                                            rgb8(0x61, 0xEF, 0xC4), rgb8(0xB1, 0xFF, 0xF4)};

        std::vector<Oklch> result;
        result.reserve(stops.size());

        for (const Rgb& stop : stops)
        {
            result.push_back(srgbToOklch(stop));
        }

        return result;
    }

    [[nodiscard]] static std::vector<Oklch> makeDefaultSpectrogram(const ThemeFamily family) noexcept
    {
        if (family == ThemeFamily::Trichromatic)
        {
            return makeTrichromaticSpectrogram();
        }

        return makeBichromaticSpectrogram();
    }
};

} // namespace ui
