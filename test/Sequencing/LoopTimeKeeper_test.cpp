#include <cmath>
#include <gtest/gtest.h>

#include "Sequencing/LoopTimeKeeper.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kMaxTimeSignatureChanges = 8;
using Keeper = LoopTimeKeeper<kMaxTimeSignatureChanges>;
}

TEST(LoopTimeKeeperTest, startsAtZero)
{
    Keeper keeper(48000.f);
    EXPECT_DOUBLE_EQ(keeper.positionBeats(), 0.0);
}

TEST(LoopTimeKeeperTest, advanceMovesByBeatsAtGivenBpm)
{
    static constexpr float kSampleRate = 48000.f;
    static constexpr float kBpm = 120.f;
    Keeper keeper(kSampleRate);
    keeper.setBpm(kBpm);
    keeper.setBars(4u);

    // One quarter note at 120 BPM lasts 0.5 s = 24000 frames.
    keeper.advance(24000, 1.f);
    EXPECT_NEAR(keeper.positionBeats(), 1.0, 1e-9);
}

TEST(LoopTimeKeeperTest, advanceWrapsAtLoopLength)
{
    static constexpr float kSampleRate = 48000.f;
    Keeper keeper(kSampleRate);
    keeper.setBpm(120.f);
    keeper.setBars(1u); // default 4/4 loop is 4 beats

    // 5 beats worth of frames should wrap to 1 beat.
    keeper.advance(24000 * 5, 1.f);
    EXPECT_NEAR(keeper.positionBeats(), 1.0, 1e-9);
}

TEST(LoopTimeKeeperTest, advanceScalesWithSpeedRatio)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(4u);

    keeper.advance(24000, 2.f);
    EXPECT_NEAR(keeper.positionBeats(), 2.0, 1e-9);
}

TEST(LoopTimeKeeperTest, resetIsExplicitOnly)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(4u);
    keeper.advance(24000, 1.f);
    ASSERT_NEAR(keeper.positionBeats(), 1.0, 1e-9);

    keeper.setBpm(140.f);
    keeper.setBars(2u);
    EXPECT_NEAR(keeper.positionBeats(), 1.0, 1e-9);

    keeper.reset();
    EXPECT_DOUBLE_EQ(keeper.positionBeats(), 0.0);
}

TEST(LoopTimeKeeperTest, setBpmNeverChangesPosition)
{
    Keeper keeper(48000.f);
    keeper.setBpm(90.f);
    keeper.setBars(8u);
    keeper.advance(24000, 1.f);
    const auto before = keeper.positionBeats();

    keeper.setBpm(200.f);
    EXPECT_DOUBLE_EQ(keeper.positionBeats(), before);

    keeper.setBpm(40.f);
    EXPECT_DOUBLE_EQ(keeper.positionBeats(), before);
}

TEST(LoopTimeKeeperTest, growingBarsLeavesPositionUntouched)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(2u);
    keeper.advance(24000 * 3, 1.f); // 3 beats in, within the 8-beat loop
    ASSERT_NEAR(keeper.positionBeats(), 3.0, 1e-9);

    keeper.setBars(8u);
    EXPECT_NEAR(keeper.positionBeats(), 3.0, 1e-9);
}

TEST(LoopTimeKeeperTest, shrinkingBarsWrapsPositionIntoRange)
{
    // 3.1.323 in a 4-bar loop, wrapped to a 2-bar loop, becomes 1.1.323 - see
    // positionBbtRoundTripsThroughBarWrap below for the exact BBT check; this
    // verifies the underlying beats-domain wrap that BBT is sliced from.
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(4u); // default 4/4 loop is 16 beats
    // Bar 3 (0-indexed bar 2), beat 1 (0-indexed) => 2*4 + 0 = 8 beats in.
    keeper.advance(24000 * 8, 1.f);
    ASSERT_NEAR(keeper.positionBeats(), 8.0, 1e-9);

    keeper.setBars(2u); // loop shrinks to 8 beats
    // 8 beats wraps to exactly 0 within an 8-beat loop.
    EXPECT_NEAR(keeper.positionBeats(), 0.0, 1e-9);
}

TEST(LoopTimeKeeperTest, positionBbtRoundTripsThroughBarWrap)
{
    static constexpr size_t kTicksPerQuarterNote = 960;
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(4u); // default 4/4 loop is 16 beats

    // Land at bar 3, beat 1, tick 323 (1-based bar/beat): 8 beats plus a
    // fractional-beat offset of 323/960.
    const double fractionalBeat = 323.0 / static_cast<double>(kTicksPerQuarterNote);
    const auto framesForBeats = [](const double beats) noexcept
    { return static_cast<size_t>(std::lround(beats * 60.0 * 48000.0 / 120.0)); };
    keeper.advance(framesForBeats(8.0 + fractionalBeat), 1.f);

    const auto before = keeper.positionBBT(kTicksPerQuarterNote);
    EXPECT_EQ(before.bar, 3u);
    EXPECT_EQ(before.beat, 1u);
    EXPECT_EQ(before.tick, 323u);

    keeper.setBars(2u);
    const auto after = keeper.positionBBT(kTicksPerQuarterNote);
    EXPECT_EQ(after.bar, 1u);
    EXPECT_EQ(after.beat, 1u);
    EXPECT_EQ(after.tick, 323u);
}

TEST(LoopTimeKeeperTest, positionFramesReflectsCurrentBpm)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(4u);
    keeper.advance(24000, 1.f); // 1 beat in

    EXPECT_NEAR(keeper.positionFrames(), 24000.0, 1e-6);

    // Same beat position, halved BPM => twice the frame-domain distance.
    keeper.setBpm(60.f);
    EXPECT_NEAR(keeper.positionFrames(), 48000.0, 1e-6);
}

TEST(LoopTimeKeeperTest, absolutePositionBeatsNeverWraps)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(1u); // loop is 4 beats

    keeper.advance(24000 * 10, 1.f); // 10 beats, well past two loop wraps
    EXPECT_NEAR(keeper.absolutePositionBeats(), 10.0, 1e-9);
    EXPECT_NEAR(keeper.positionBeats(), 2.0, 1e-9); // 10 mod 4
}

TEST(LoopTimeKeeperTest, absolutePositionBeatsResetOnlyOnExplicitReset)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(2u);
    keeper.advance(24000 * 3, 1.f);
    ASSERT_NEAR(keeper.absolutePositionBeats(), 3.0, 1e-9);

    keeper.setBpm(90.f);
    keeper.setBars(8u);
    EXPECT_NEAR(keeper.absolutePositionBeats(), 3.0, 1e-9);

    keeper.reset();
    EXPECT_DOUBLE_EQ(keeper.absolutePositionBeats(), 0.0);
}

TEST(LoopTimeKeeperTest, absolutePositionBeatsSurvivesThisClockOwnWrapForADifferentSizedLoop)
{
    // The motivating case: a consumer whose own loop (7 beats) doesn't evenly
    // divide this clock's own loop (4 beats) must not glitch when this clock
    // wraps - its own modulo against the unwrapped absolute value stays smooth.
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(1u); // this clock's own loop is 4 beats

    keeper.advance(24000 * 5, 1.f); // 5 beats - this clock has already wrapped once
    const auto consumerLoopBeats = 7.0;
    const auto consumerPosition = std::fmod(keeper.absolutePositionBeats(), consumerLoopBeats);
    EXPECT_NEAR(consumerPosition, 5.0, 1e-9); // continuous, not reset to 1.0 by this clock's own wrap
}

TEST(LoopTimeKeeperTest, defaultTimeSignatureMatchesFourFour)
{
    // No setTimeSignature() call at all - every bar must behave like the old,
    // fixed-4/4 Phase 1 clock.
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(3u);
    EXPECT_NEAR(keeper.loopBeats(), 12.0, 1e-9);

    keeper.advance(24000 * 5, 1.f); // 5 beats in => bar 2, beat 2
    const auto bbt = keeper.positionBBT(960);
    EXPECT_EQ(bbt.bar, 2u);
    EXPECT_EQ(bbt.beat, 2u);
    EXPECT_EQ(bbt.tick, 0u);
}

TEST(LoopTimeKeeperTest, mixedTimeSignatureLoopLength)
{
    // Bars 1-4 stay 4/4 (16 beats), bars 5-6 switch to 7/8 (3.5 beats each = 7).
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    ASSERT_TRUE(keeper.setTimeSignature(5u, 7, 8));
    keeper.setBars(6u);

    EXPECT_NEAR(keeper.loopBeats(), 16.0 + 7.0, 1e-9);
}

TEST(LoopTimeKeeperTest, positionBbtInsideOddBarReportsBeatUpToNumerator)
{
    static constexpr size_t kTicksPerQuarterNote = 960;
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    ASSERT_TRUE(keeper.setTimeSignature(1u, 7, 8));
    keeper.setBars(2u);

    // Beat 6 (0-indexed 5) of the 7/8 bar: 5 beats * 0.5 quarter-note each = 2.5
    // quarter-note beats in, landing exactly on a beat boundary (tick 0).
    keeper.advance(static_cast<size_t>(2.5 * 60.0 / 120.0 * 48000.0), 1.f);
    const auto bbt = keeper.positionBBT(kTicksPerQuarterNote);
    EXPECT_EQ(bbt.bar, 1u);
    EXPECT_EQ(bbt.beat, 6u);
    EXPECT_EQ(bbt.tick, 0u);
}

TEST(LoopTimeKeeperTest, setTimeSignatureOverwritesExistingStartBar)
{
    Keeper keeper(48000.f);
    ASSERT_TRUE(keeper.setTimeSignature(1u, 3, 4));
    ASSERT_TRUE(keeper.setTimeSignature(1u, 5, 4));
    keeper.setBars(1u);

    // Only one entry should exist at startBar 1 - the later call replaces it.
    EXPECT_NEAR(keeper.loopBeats(), 5.0, 1e-9);
}

TEST(LoopTimeKeeperTest, setTimeSignatureFailsWhenTableIsFull)
{
    LoopTimeKeeper<1> keeper(48000.f); // only room for the default entry
    EXPECT_FALSE(keeper.setTimeSignature(2u, 3, 4));
}

TEST(LoopTimeKeeperTest, timeSignatureChangeNeverResetsPosition)
{
    Keeper keeper(48000.f);
    keeper.setBpm(120.f);
    keeper.setBars(4u);
    keeper.advance(24000, 1.f);
    ASSERT_NEAR(keeper.positionBeats(), 1.0, 1e-9);

    ASSERT_TRUE(keeper.setTimeSignature(3u, 3, 4));
    EXPECT_NEAR(keeper.positionBeats(), 1.0, 1e-9);
}

}
