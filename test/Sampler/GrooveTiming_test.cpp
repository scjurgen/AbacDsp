#include <gtest/gtest.h>

#include "Sampler/GrooveTiming.h"

namespace AbacDsp::Test
{

TEST(GrooveTiming, GridStepTicksMatchesResolution)
{
    EXPECT_EQ(gridStepTicks(480, GridResolution::Eighth), 240u);
    EXPECT_EQ(gridStepTicks(480, GridResolution::Sixteenth), 120u);
    EXPECT_EQ(gridStepTicks(480, GridResolution::ThirtySecond), 60u);
}

TEST(GrooveTiming, InferredGridTickRoundsToNearestStep)
{
    EXPECT_EQ(inferredGridTick(115, 120), 120u);
    EXPECT_EQ(inferredGridTick(5, 120), 0u);
    EXPECT_EQ(inferredGridTick(65, 120), 120u);
}

TEST(GrooveTiming, BarLengthTicksDefaultsToFourFour)
{
    EXPECT_EQ(barLengthTicks({}, 480, 0), 1920u);
}

TEST(GrooveTiming, BarLengthTicksFollowsTimeSignatureChange)
{
    const std::vector<MidiTimeSignatureEvent> timeSignatures{{0, 4, 2}, {1920, 3, 2}};
    EXPECT_EQ(barLengthTicks(timeSignatures, 480, 0), 1920u);
    EXPECT_EQ(barLengthTicks(timeSignatures, 480, 1920), 1440u);
    EXPECT_EQ(barLengthTicks(timeSignatures, 480, 3000), 1440u);
}

TEST(GrooveTiming, BarLengthTicksHandlesEighthDenominator)
{
    // 6/8: six eighth-note beats per bar.
    const std::vector<MidiTimeSignatureEvent> timeSignatures{{0, 6, 3}};
    EXPECT_EQ(barLengthTicks(timeSignatures, 480, 0), 1440u);
}

TEST(GrooveTiming, BarIndexForTickCountsAcrossTimeSignatureChange)
{
    const std::vector<MidiTimeSignatureEvent> timeSignatures{{0, 4, 2}, {3840, 3, 2}}; // two 4/4 bars, then 3/4
    EXPECT_EQ(barIndexForTick(timeSignatures, 480, 0), 0u);
    EXPECT_EQ(barIndexForTick(timeSignatures, 480, 1920), 1u);
    EXPECT_EQ(barIndexForTick(timeSignatures, 480, 3840), 2u);
    EXPECT_EQ(barIndexForTick(timeSignatures, 480, 3840 + 1440), 3u);
}

TEST(GrooveTiming, BarStartTickResolvesBarBoundaries)
{
    EXPECT_EQ(barStartTick({}, 480, 0), 0u);
    EXPECT_EQ(barStartTick({}, 480, 1919), 0u);
    EXPECT_EQ(barStartTick({}, 480, 1920), 1920u);
}

TEST(GrooveTiming, PatternPositionInBarIsZeroAtDownbeatAndWrapsPerBar)
{
    EXPECT_EQ(patternPositionInBar({}, 480, 0, GridResolution::Sixteenth), 0u);
    EXPECT_EQ(patternPositionInBar({}, 480, 120, GridResolution::Sixteenth), 1u);
    EXPECT_EQ(patternPositionInBar({}, 480, 1920, GridResolution::Sixteenth), 0u); // next bar's downbeat
}

TEST(GrooveTiming, GridStrengthIsStrongOnlyOnBeatBoundaries)
{
    EXPECT_EQ(gridStrength(0, GridResolution::Sixteenth), GridStrength::Strong);
    EXPECT_EQ(gridStrength(4, GridResolution::Sixteenth), GridStrength::Strong);
    EXPECT_EQ(gridStrength(1, GridResolution::Sixteenth), GridStrength::Weak);
    EXPECT_EQ(gridStrength(2, GridResolution::Sixteenth), GridStrength::Weak);
}

TEST(GrooveTiming, IsBackbeatPositionExcludesDownbeat)
{
    EXPECT_FALSE(isBackbeatPosition(0, GridResolution::Sixteenth));
    EXPECT_TRUE(isBackbeatPosition(4, GridResolution::Sixteenth));  // beat 2
    EXPECT_TRUE(isBackbeatPosition(8, GridResolution::Sixteenth));  // beat 3
    EXPECT_FALSE(isBackbeatPosition(1, GridResolution::Sixteenth)); // weak position
}

}
