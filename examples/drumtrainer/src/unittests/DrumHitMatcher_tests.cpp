#include <gtest/gtest.h>
#include <initializer_list>
#include <utility>
#include <vector>

#include "impl/DrumHitMatcher.h"

using namespace DrumAnalysis;
using DrumInput::PlayedVoice;
using DrumPattern::Instrument;

namespace
{
// 120 BPM: one beat is 500 ms, so the 70 ms window is 0.14 beats.
TakeSetup makeSetup(const std::initializer_list<std::pair<float, Instrument>> hits, const size_t capturedBars,
                    const size_t patternBars = 1)
{
    TakeSetup setup{};
    setup.pattern.active = true;
    setup.pattern.patternBars = patternBars;
    for (const auto& [position, instrument] : hits)
    {
        setup.pattern.hits[setup.pattern.hitCount++] = {position, instrument, 100};
    }
    setup.beatsPerBar = 4;
    setup.capturedBars = capturedBars;
    setup.bpm = 120.f;
    return setup;
}

CapturedHit played(const float takeBeats, const PlayedVoice voice)
{
    return {takeBeats, voice, 100, 127};
}

const TakeSetup kBackbeat = makeSetup(
    {{0.f, Instrument::Kick}, {1.f, Instrument::Snare}, {2.f, Instrument::Kick}, {3.f, Instrument::Snare}}, 1);
}

TEST(DrumHitMatcherTest, ExactTakeMatchesEveryHit)
{
    const std::vector<CapturedHit> hits{played(0.f, PlayedVoice::Kick), played(1.f, PlayedVoice::Snare),
                                        played(2.f, PlayedVoice::Kick), played(3.f, PlayedVoice::Snare)};
    const auto result = matchTake(kBackbeat, hits);
    EXPECT_EQ(result.expectedCount, 4u);
    ASSERT_EQ(result.matched.size(), 4u);
    EXPECT_TRUE(result.missed.empty());
    EXPECT_TRUE(result.extras.empty());
    for (const auto& hit : result.matched)
    {
        EXPECT_FLOAT_EQ(hit.deviationMs, 0.f);
        EXPECT_FALSE(hit.wrongArticulation);
    }
}

TEST(DrumHitMatcherTest, LateIsNegativeAndEarlyPositiveInMilliseconds)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}, {1.f, Instrument::Snare}}, 1);
    const std::vector<CapturedHit> hits{played(0.02f, PlayedVoice::Kick), played(0.96f, PlayedVoice::Snare)};
    const auto result = matchTake(setup, hits);
    ASSERT_EQ(result.matched.size(), 2u);
    for (const auto& hit : result.matched)
    {
        const auto instrument = setup.pattern.hits[hit.patternHit].instrument;
        EXPECT_NEAR(hit.deviationMs, instrument == Instrument::Kick ? -10.f : 20.f, 0.01f);
    }
}

TEST(DrumHitMatcherTest, ThePatternRepeatsOverTheCapturedBars)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}, {6.f, Instrument::Crash}}, 4, 2);
    const std::vector<CapturedHit> hits{played(0.f, PlayedVoice::Kick), played(6.f, PlayedVoice::Crash),
                                        played(8.f, PlayedVoice::Kick), played(14.f, PlayedVoice::Crash)};
    const auto result = matchTake(setup, hits);
    EXPECT_EQ(result.expectedCount, 4u);
    ASSERT_EQ(result.matched.size(), 4u);
    EXPECT_EQ(result.matched[0].cycle + result.matched[1].cycle, 1u);
}

TEST(DrumHitMatcherTest, AHitOutsideTheWindowIsAMissPlusAnExtra)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}}, 1);
    const std::vector<CapturedHit> hits{played(0.2f, PlayedVoice::Kick)};
    const auto result = matchTake(setup, hits);
    EXPECT_TRUE(result.matched.empty());
    ASSERT_EQ(result.missed.size(), 1u);
    ASSERT_EQ(result.extras.size(), 1u);
    EXPECT_EQ(result.extras[0].instrument, Instrument::Kick);
}

TEST(DrumHitMatcherTest, DenseHitsShrinkTheWindowToHalfTheirDistance)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::HihatClosed}, {0.25f, Instrument::HihatClosed}}, 1);
    const std::vector<CapturedHit> hits{played(0.13f, PlayedVoice::HihatClosed)};
    const auto result = matchTake(setup, hits);
    ASSERT_EQ(result.matched.size(), 1u);
    EXPECT_EQ(result.matched[0].patternHit, 1u);
    EXPECT_NEAR(result.matched[0].deviationMs, 60.f, 0.01f);
    EXPECT_EQ(result.missed.size(), 1u);
}

TEST(DrumHitMatcherTest, AnExpectedHitTakesOnlyTheClosestPlayedHit)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}}, 1);
    const std::vector<CapturedHit> hits{played(0.05f, PlayedVoice::Kick), played(0.01f, PlayedVoice::Kick)};
    const auto result = matchTake(setup, hits);
    ASSERT_EQ(result.matched.size(), 1u);
    EXPECT_NEAR(result.matched[0].deviationMs, -5.f, 0.01f);
    EXPECT_EQ(result.extras.size(), 1u);
}

TEST(DrumHitMatcherTest, InstrumentsAreMatchedIndependently)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}, {0.f, Instrument::Crash}}, 1);
    const std::vector<CapturedHit> hits{played(0.f, PlayedVoice::Crash), played(0.f, PlayedVoice::Snare)};
    const auto result = matchTake(setup, hits);
    ASSERT_EQ(result.matched.size(), 1u);
    EXPECT_EQ(setup.pattern.hits[result.matched[0].patternHit].instrument, Instrument::Crash);
    EXPECT_EQ(result.missed.size(), 1u);
    ASSERT_EQ(result.extras.size(), 1u);
    EXPECT_EQ(result.extras[0].instrument, Instrument::Snare);
}

TEST(DrumHitMatcherTest, WrongHihatArticulationStillMatchesForTiming)
{
    const TakeSetup setup =
        makeSetup({{0.f, Instrument::HihatClosed}, {0.5f, Instrument::HihatOpen}, {1.f, Instrument::HihatClosed}}, 1);
    const std::vector<CapturedHit> hits{played(0.f, PlayedVoice::HihatHalfOpen), played(0.5f, PlayedVoice::HihatClosed),
                                        played(1.f, PlayedVoice::HihatOpen)};
    const auto result = matchTake(setup, hits);
    ASSERT_EQ(result.matched.size(), 3u);
    EXPECT_TRUE(result.missed.empty());
    EXPECT_TRUE(result.extras.empty());
    for (const auto& hit : result.matched)
    {
        EXPECT_EQ(hit.wrongArticulation, hit.patternHit != 0) << hit.patternHit;
    }
}

TEST(DrumHitMatcherTest, PedalChicksAreCountedButNotScored)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}}, 1);
    const std::vector<CapturedHit> hits{played(1.f, PlayedVoice::HihatPedal), played(3.f, PlayedVoice::HihatPedal),
                                        played(-0.5f, PlayedVoice::HihatPedal)};
    const auto result = matchTake(setup, hits);
    EXPECT_EQ(result.pedalHits, 2u);
    EXPECT_TRUE(result.extras.empty());
}

TEST(DrumHitMatcherTest, AnEarlyFirstHitFromTheCountInStillMatches)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}}, 1);
    const std::vector<CapturedHit> hits{played(-0.05f, PlayedVoice::Kick), played(-2.f, PlayedVoice::Snare)};
    const auto result = matchTake(setup, hits);
    ASSERT_EQ(result.matched.size(), 1u);
    EXPECT_NEAR(result.matched[0].deviationMs, 25.f, 0.01f);
    EXPECT_TRUE(result.extras.empty());
}

TEST(DrumHitMatcherTest, HitsAfterTheLastCapturedBarAreDropped)
{
    const TakeSetup setup = makeSetup({{0.f, Instrument::Kick}}, 1);
    const std::vector<CapturedHit> hits{played(0.f, PlayedVoice::Kick), played(4.f, PlayedVoice::Kick),
                                        played(5.5f, PlayedVoice::Snare)};
    const auto result = matchTake(setup, hits);
    EXPECT_EQ(result.expectedCount, 1u);
    EXPECT_EQ(result.matched.size(), 1u);
    EXPECT_TRUE(result.extras.empty());
}

TEST(DrumHitMatcherTest, AnInactivePatternMatchesNothing)
{
    TakeSetup setup = kBackbeat;
    setup.pattern.active = false;
    const std::vector<CapturedHit> hits{played(0.f, PlayedVoice::Kick)};
    const auto result = matchTake(setup, hits);
    EXPECT_EQ(result.expectedCount, 0u);
    EXPECT_TRUE(result.matched.empty());
    EXPECT_TRUE(result.extras.empty());
}

TEST(DrumHitMatcherTest, CollectorStopsAtItsCapacity)
{
    CapturedHitCollector collector;
    for (size_t i = 0; i < CapturedHitCollector::kCapacity + 10; ++i)
    {
        collector.push(played(static_cast<float>(i), PlayedVoice::Kick));
    }
    EXPECT_EQ(collector.hits().size(), CapturedHitCollector::kCapacity);
    collector.reset();
    EXPECT_TRUE(collector.hits().empty());
}
