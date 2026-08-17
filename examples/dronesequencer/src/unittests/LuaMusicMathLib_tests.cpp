#include <algorithm>
#include <array>
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

TEST(LuaMusicMathLib, HarmonicSeriesIsIntegerMultiplesOfTheFundamental)
{
    std::array<float, 4> out{};
    const size_t written = LuaMusicMath::harmonicSeries(110.f, 4, out);
    ASSERT_EQ(written, 4u);
    EXPECT_FLOAT_EQ(out[0], 110.f);
    EXPECT_FLOAT_EQ(out[1], 220.f);
    EXPECT_FLOAT_EQ(out[2], 330.f);
    EXPECT_FLOAT_EQ(out[3], 440.f);
}

TEST(LuaMusicMathLib, HarmonicSeriesIsClampedToTheOutputSpan)
{
    std::array<float, 2> out{};
    EXPECT_EQ(LuaMusicMath::harmonicSeries(100.f, 10, out), 2u);
}

TEST(LuaMusicMathLib, HarmonizeToScaleSnapsToNearestScaleDegree)
{
    // C major (root = 60): note 61 (C#) is 1 semitone from both 60 (C) and 62 (D) - the
    // scan picks the first minimum, so it resolves to 60.
    EXPECT_FLOAT_EQ(LuaMusicMath::harmonizeToScale(61.f, 60.f, LuaMusicMath::kScaleMajor), 60.f);
    // note 63 (D#) is 1 semitone from 62 (D) and 2 from 64 (E) - snaps to 62.
    EXPECT_FLOAT_EQ(LuaMusicMath::harmonizeToScale(63.f, 60.f, LuaMusicMath::kScaleMajor), 62.f);
}

TEST(LuaMusicMathLib, HarmonizeToScalePreservesOctave)
{
    EXPECT_FLOAT_EQ(LuaMusicMath::harmonizeToScale(73.f, 60.f, LuaMusicMath::kScaleMajor), 72.f);
}

TEST(LuaMusicMathLib, HarmonizeToScaleWithEmptyIntervalsReturnsNoteUnchanged)
{
    EXPECT_FLOAT_EQ(LuaMusicMath::harmonizeToScale(65.f, 60.f, {}), 65.f);
}
