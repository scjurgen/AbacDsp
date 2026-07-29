#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <regex>
#include <string>

#include "ThemeOrbit.h"

namespace ui::Test
{

struct ParsedRgb
{
    double r{};
    double g{};
    double b{};
};

[[nodiscard]] ParsedRgb parseHex(const std::string& hex)
{
    const auto byte = [&](const size_t offset)
    { return static_cast<double>(std::stoul(hex.substr(offset, 2), nullptr, 16)) / 255.0; };
    return {.r = byte(1), .g = byte(3), .b = byte(5)};
}

[[nodiscard]] double relativeLuminance(const ParsedRgb& rgb)
{
    const auto linear = [](const double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(rgb.r) + 0.7152 * linear(rgb.g) + 0.0722 * linear(rgb.b);
}

[[nodiscard]] bool isHexColor(const std::string& hex)
{
    static const std::regex pattern{"^#[0-9A-F]{6}$"};
    return std::regex_match(hex, pattern);
}

TEST(ThemeOrbitTest, AllRolesProduceValidHexColors)
{
    Theme theme;
    for (auto role : {ThemeRole::Primary, ThemeRole::Hover, ThemeRole::Background0, ThemeRole::Background1,
                      ThemeRole::Background2, ThemeRole::Text, ThemeRole::TextMuted, ThemeRole::AccentInfo,
                      ThemeRole::AccentSuccess, ThemeRole::AccentWarning, ThemeRole::AccentDanger})
    {
        EXPECT_TRUE(isHexColor(theme.hex(role))) << "role " << static_cast<std::uint32_t>(role);
    }
}

TEST(ThemeOrbitTest, LightModeBackgroundIsLighterThanDarkMode)
{
    Theme light;
    light.setMode(ThemeMode::Light);
    Theme dark;
    dark.setMode(ThemeMode::Dark);

    const auto lightLuminance = relativeLuminance(parseHex(light.hex(ThemeRole::Background0)));
    const auto darkLuminance = relativeLuminance(parseHex(dark.hex(ThemeRole::Background0)));

    EXPECT_GT(lightLuminance, darkLuminance);
}

TEST(ThemeOrbitTest, LightModeTextIsDarkAndDarkModeTextIsLight)
{
    Theme light;
    light.setMode(ThemeMode::Light);
    Theme dark;
    dark.setMode(ThemeMode::Dark);

    EXPECT_LT(relativeLuminance(parseHex(light.hex(ThemeRole::Text))), 0.3);
    EXPECT_GT(relativeLuminance(parseHex(dark.hex(ThemeRole::Text))), 0.6);
}

TEST(ThemeOrbitTest, HueRotationChangesPrimaryColor)
{
    Theme theme;
    const auto hue0 = theme.hex(ThemeRole::Primary);
    theme.setRelativeHueDegrees(180.0);
    const auto hue180 = theme.hex(ThemeRole::Primary);

    EXPECT_NE(hue0, hue180);
}

TEST(ThemeOrbitTest, FullHueWrapReturnsToStartingColor)
{
    Theme theme;
    theme.setRelativeHueDegrees(45.0);
    const auto hue45 = theme.hex(ThemeRole::Primary);
    theme.setRelativeHueDegrees(45.0 + 360.0);
    const auto hue45Wrapped = theme.hex(ThemeRole::Primary);

    EXPECT_EQ(hue45, hue45Wrapped);
}

TEST(ThemeOrbitTest, OnPrimaryColorIsReadableAgainstPrimaryBackground)
{
    Theme theme;
    for (int hueStep = 0; hueStep < 12; ++hueStep)
    {
        theme.setRelativeHueDegrees(hueStep * 30.0);
        const auto primaryLuminance = relativeLuminance(parseHex(theme.hex(ThemeRole::Primary)));
        const auto onPrimaryLuminance = relativeLuminance(parseHex(theme.onPrimaryHex()));
        const auto lighter = std::max(primaryLuminance, onPrimaryLuminance);
        const auto darker = std::min(primaryLuminance, onPrimaryLuminance);
        const auto contrast = (lighter + 0.05) / (darker + 0.05);

        EXPECT_GE(contrast, 4.0) << "hue step " << hueStep;
    }
}

TEST(ThemeOrbitTest, SpectrogramProducesOrderedGradientStops)
{
    Theme theme;
    const auto stops = theme.spectrogramStopsHex();

    ASSERT_GE(stops.size(), 2u);
    for (const auto& stop : stops)
    {
        EXPECT_TRUE(isHexColor(stop));
    }

    const auto firstLuminance = relativeLuminance(parseHex(stops.front()));
    const auto lastLuminance = relativeLuminance(parseHex(stops.back()));
    EXPECT_LT(firstLuminance, lastLuminance);
}

TEST(ThemeOrbitTest, SpectrogramCssGradientIsWellFormed)
{
    Theme theme;
    const auto css = theme.spectrogramCssGradient();

    EXPECT_TRUE(css.starts_with("linear-gradient(to top"));
    EXPECT_TRUE(css.ends_with(")"));
}

} // namespace ui::Test
