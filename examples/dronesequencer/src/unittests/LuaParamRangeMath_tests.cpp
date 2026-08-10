#include <gtest/gtest.h>

#include "inc/LuaParamRangeMath.h"

TEST(LuaParamRangeMath, LinearRoundTrip)
{
    const float display = luaParamNormalizedToDisplay(-12.f, 12.f, 0.f, 1.f, 0.75f);
    EXPECT_FLOAT_EQ(display, 6.f);
    EXPECT_FLOAT_EQ(luaParamDisplayToNormalized(-12.f, 12.f, 1.f, display), 0.75f);
}

TEST(LuaParamRangeMath, SkewedMatchesJuceNormalisableRangeConvention)
{
    // Mirrors juce::NormalisableRange::convertFrom0to1's non-symmetric-skew formula:
    // display = start + span * normalized^(1/skew), not normalized^skew.
    const float display = luaParamNormalizedToDisplay(0.f, 100.f, 0.f, 2.f, 0.25f);
    EXPECT_NEAR(display, 50.f, 1e-4f);
}

TEST(LuaParamRangeMath, SkewedRoundTrip)
{
    const float normalized = luaParamDisplayToNormalized(0.f, 100.f, 2.f, 50.f);
    EXPECT_NEAR(normalized, 0.25f, 1e-4f);
}

TEST(LuaParamRangeMath, StepSnapsToNearestMultiple)
{
    // raw display 5.3 (10 * 0.53) snaps to the nearest multiple of the 2-unit step, 6.
    const float display = luaParamNormalizedToDisplay(0.f, 10.f, 2.f, 1.f, 0.53f);
    EXPECT_FLOAT_EQ(display, 6.f);
}

TEST(LuaParamRangeMath, ClampsOutOfRangeInputs)
{
    EXPECT_FLOAT_EQ(luaParamNormalizedToDisplay(0.f, 10.f, 0.f, 1.f, -1.f), 0.f);
    EXPECT_FLOAT_EQ(luaParamNormalizedToDisplay(0.f, 10.f, 0.f, 1.f, 2.f), 10.f);
    EXPECT_FLOAT_EQ(luaParamDisplayToNormalized(0.f, 10.f, 1.f, -5.f), 0.f);
    EXPECT_FLOAT_EQ(luaParamDisplayToNormalized(0.f, 10.f, 1.f, 15.f), 1.f);
}
