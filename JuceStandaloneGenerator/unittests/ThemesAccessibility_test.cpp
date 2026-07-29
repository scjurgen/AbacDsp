#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>

#include "themes/Themes.h"

namespace Themes::Test
{

[[nodiscard]] double relativeLuminance(const std::uint32_t argb)
{
    const auto linearChannel = [&](const int shift) -> double
    {
        const double c = static_cast<double>((argb >> shift) & 0xFFu) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linearChannel(16) + 0.7152 * linearChannel(8) + 0.0722 * linearChannel(0);
}

[[nodiscard]] double contrastRatio(const std::uint32_t a, const std::uint32_t b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

class ThemesAccessibilityTest : public ::testing::TestWithParam<int>
{
  protected:
    [[nodiscard]] ThemeDefinition def() const
    {
        return definition(static_cast<Theme>(GetParam()));
    }
};

TEST_P(ThemesAccessibilityTest, LabelTextMeetsAaContrastAgainstPage)
{
    const auto d = def();
    EXPECT_GE(contrastRatio(d.labelColour, d.background), 4.5);
}

TEST_P(ThemesAccessibilityTest, StatusOutlineMeetsAaContrastAgainstPage)
{
    const auto d = def();
    EXPECT_GE(contrastRatio(d.statusOutline, d.background), 4.5);
}

TEST_P(ThemesAccessibilityTest, SwitchOffThumbIsDistinctFromItsTrack)
{
    const auto d = def();
    // 1.2, not a more typical AA-ish 1.4+: against a near-black backgroundDark, WCAG's
    // contrast formula compresses hard (its +0.05 term dominates near luminance 0) - even
    // using the page background itself as gradientDark only reaches ~1.3 here, so 1.4 is
    // not reachable without the thumb looking identical to the page.
    EXPECT_GE(contrastRatio(d.gradientDark, d.backgroundDark), 1.2);
}

TEST_P(ThemesAccessibilityTest, SurfaceTiersAreDistinctFromPage)
{
    const auto d = def();
    EXPECT_GE(contrastRatio(d.background, d.backgroundDark), 1.3);
    EXPECT_GE(contrastRatio(d.background, d.backgroundComponent), 1.15);
}

TEST_P(ThemesAccessibilityTest, MeterZonesAreLuminanceOrdered)
{
    const auto d = def();
    const double safeL = relativeLuminance(d.cpuZones.safe);
    const double warnL = relativeLuminance(d.cpuZones.warn);
    const double dangerL = relativeLuminance(d.cpuZones.danger);

    // Ordered by luminance, not hue, so severity still reads correctly for colour-blind users.
    EXPECT_LT(safeL, warnL);
    EXPECT_LT(warnL, dangerL);
}

// Covers all three ui::ThemeFamily slots (Monochromatic falls back to Bichromatic's tables
// until implemented, so it's redundant with the Bichromatic range but harmless to include).
INSTANTIATE_TEST_SUITE_P(AllHuesModesAndFamilies, ThemesAccessibilityTest, ::testing::Range(0, 3 * kSlotsPerFamily));

} // namespace Themes::Test
