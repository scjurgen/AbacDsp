#include <algorithm>
#include <gtest/gtest.h>

#include "inc/LuaMusicMathLib.h"

TEST(LuaMusicMathLib, NoteToHzAndBackRoundTrips)
{
    const float hz = LuaMusicMath::noteToHz(69.f);
    EXPECT_FLOAT_EQ(hz, 440.f);
    EXPECT_FLOAT_EQ(LuaMusicMath::hzToNote(hz), 69.f);
}

TEST(LuaMusicMathLib, IntervalToRatioMatchesTwelveToneEqualTemperament)
{
    EXPECT_FLOAT_EQ(LuaMusicMath::intervalToRatio(-12.f), 0.5f);
    EXPECT_FLOAT_EQ(LuaMusicMath::intervalToRatio(0.f), 1.f);
    EXPECT_FLOAT_EQ(LuaMusicMath::intervalToRatio(12.f), 2.f);
}

TEST(LuaMusicMathLib, RatioToIntervalIsTheInverse)
{
    EXPECT_NEAR(LuaMusicMath::ratioToInterval(2.f), 12.f, 1e-4f);
    EXPECT_NEAR(LuaMusicMath::ratioToInterval(1.f), 0.f, 1e-4f);
}

TEST(LuaMusicMathLib, VelocityCurvesAreAnchoredAtZeroAndOne)
{
    EXPECT_NEAR(LuaMusicMath::velocityToGainExponential(0.f), 0.f, 1e-5f);
    EXPECT_NEAR(LuaMusicMath::velocityToGainExponential(1.f), 1.f, 1e-5f);
    EXPECT_NEAR(LuaMusicMath::velocityToGainCubic(0.f), 0.f, 1e-5f);
    EXPECT_NEAR(LuaMusicMath::velocityToGainCubic(1.f), 1.f, 1e-5f);
}

TEST(LuaMusicMathLib, ExponentialCurveIsMonotonic)
{
    float previous = LuaMusicMath::velocityToGainExponential(0.f);
    for (int i = 1; i <= 10; ++i)
    {
        const float v = LuaMusicMath::velocityToGainExponential(static_cast<float>(i) / 10.f);
        EXPECT_GT(v, previous);
        previous = v;
    }
}

TEST(LuaMusicMathLib, ToroidIncrementWrapsAtBoundary)
{
    EXPECT_EQ(LuaMusicMath::toroidIncrement(0, 4), 1u);
    EXPECT_EQ(LuaMusicMath::toroidIncrement(3, 4), 0u);
    EXPECT_EQ(LuaMusicMath::toroidIncrement(0, 0), 0u);
}

TEST(LuaMusicMathLib, ToroidAdvanceWrapsByStep)
{
    EXPECT_EQ(LuaMusicMath::toroidAdvance(1, 5, 4), 2u);
}

TEST(LuaMusicMathLib, ScaleTableLookupHasExpectedIntervals)
{
    const auto it = std::ranges::find(LuaMusicMath::kScales, "Major", &LuaMusicMath::ScaleEntry::name);
    ASSERT_NE(it, LuaMusicMath::kScales.end());
    ASSERT_EQ(it->intervals.size(), 7u);
    EXPECT_EQ(it->intervals[2], 4);
}

TEST(LuaMusicMathLib, ChordTableLookupHasExpectedIntervals)
{
    const auto it = std::ranges::find(LuaMusicMath::kChords, "Major7", &LuaMusicMath::ChordEntry::name);
    ASSERT_NE(it, LuaMusicMath::kChords.end());
    ASSERT_EQ(it->intervals.size(), 4u);
    EXPECT_EQ(it->intervals[3], 11);
}

TEST(LuaMusicMathLib, BeatsToMsMatchesKnownBpm)
{
    EXPECT_FLOAT_EQ(LuaMusicMath::beatsToMs(1.f, 120.f), 500.f);
    EXPECT_FLOAT_EQ(LuaMusicMath::beatsToMs(0.5f, 120.f), 250.f);
}
