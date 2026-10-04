#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "impl/MetronomePattern.h"

using namespace MetronomePattern;

namespace
{

struct Fired
{
    size_t beat{0};
    size_t sample{0};
    Instrument instrument{Instrument::Kick};
};

HitPattern makePattern(const std::vector<Hit>& hits)
{
    HitPattern pattern{};
    pattern.active = true;
    for (const auto& hit : hits)
    {
        pattern.hits[pattern.hitCount++] = hit;
    }
    return pattern;
}

// Drives the scheduler like a sequencer would: every sample of every beat of numBars bars.
std::vector<Fired> runBars(PatternScheduler& scheduler, const size_t beatsPerBar, const size_t samplesPerBeat,
                           const size_t numBars)
{
    std::vector<Fired> fired;
    for (size_t bar = 0; bar < numBars; ++bar)
    {
        for (size_t beat = 0; beat < beatsPerBar; ++beat)
        {
            for (size_t pos = 0; pos < samplesPerBeat; ++pos)
            {
                scheduler.step(beat, pos, samplesPerBeat,
                               [&](const Hit& hit) { fired.push_back({beat, pos, hit.instrument}); });
            }
        }
    }
    return fired;
}

constexpr size_t kBeatSamples{1000};
}

TEST(InstrumentTableTest, IndexMatchesEnumAndSampleCode)
{
    EXPECT_EQ(sampleCode(Instrument::Kick), "bd");
    EXPECT_EQ(sampleCode(Instrument::HihatOpen), "hhopen");
    EXPECT_EQ(sampleCode(Instrument::ClickHigh), "clickhigh");
    EXPECT_EQ(kInstruments[static_cast<size_t>(Instrument::Snare)].luaName, "Snare");
}

TEST(InstrumentTableTest, InstrumentFromIndexRejectsOutOfRange)
{
    EXPECT_EQ(instrumentFromIndex(0), Instrument::Kick);
    EXPECT_FALSE(instrumentFromIndex(-1).has_value());
    EXPECT_FALSE(instrumentFromIndex(static_cast<long long>(kInstruments.size())).has_value());
}

TEST(TripleBufferTest, NothingAcquiredBeforeFirstPublish)
{
    TripleBuffer<int> buffer;
    EXPECT_EQ(buffer.acquireNewest(), nullptr);
}

TEST(TripleBufferTest, ConsumerSeesNewestValueOnly)
{
    TripleBuffer<int> buffer;
    buffer.backSlot() = 1;
    buffer.publish();
    buffer.backSlot() = 2;
    buffer.publish();
    const int* newest = buffer.acquireNewest();
    ASSERT_NE(newest, nullptr);
    EXPECT_EQ(*newest, 2);
    EXPECT_EQ(buffer.acquireNewest(), nullptr);
}

TEST(TripleBufferTest, ProducerNeverOverwritesTheValueTheConsumerHolds)
{
    TripleBuffer<int> buffer;
    buffer.backSlot() = 10;
    buffer.publish();
    const int* held = buffer.acquireNewest();
    ASSERT_NE(held, nullptr);
    for (int i = 11; i < 20; ++i)
    {
        buffer.backSlot() = i;
        buffer.publish();
        EXPECT_EQ(*held, 10);
    }
}

TEST(PatternSchedulerTest, InactiveWithoutPatternFiresNothing)
{
    PatternScheduler scheduler;
    EXPECT_FALSE(scheduler.active());
    EXPECT_TRUE(runBars(scheduler, 4, kBeatSamples, 1).empty());
}

TEST(PatternSchedulerTest, EmptyActivePatternIsActiveButSilent)
{
    const HitPattern pattern = makePattern({});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    EXPECT_TRUE(scheduler.active());
    EXPECT_TRUE(runBars(scheduler, 4, kBeatSamples, 1).empty());
}

TEST(PatternSchedulerTest, HitsLandOnTheirExactSample)
{
    const HitPattern pattern =
        makePattern({{0.f, Instrument::Kick, 1.f}, {0.5f, Instrument::Hihat, 1.f}, {2.25f, Instrument::Snare, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    const auto fired = runBars(scheduler, 4, kBeatSamples, 1);
    ASSERT_EQ(fired.size(), 3u);
    EXPECT_EQ(fired[0].beat, 0u);
    EXPECT_EQ(fired[0].sample, 0u);
    EXPECT_EQ(fired[1].beat, 0u);
    EXPECT_EQ(fired[1].sample, 500u);
    EXPECT_EQ(fired[2].beat, 2u);
    EXPECT_EQ(fired[2].sample, 250u);
    EXPECT_EQ(fired[2].instrument, Instrument::Snare);
}

TEST(PatternSchedulerTest, SeveralHitsAtOnePositionAllFireOnTheSameSample)
{
    const HitPattern pattern =
        makePattern({{1.f, Instrument::Kick, 1.f}, {1.f, Instrument::Hihat, 1.f}, {1.f, Instrument::Crash, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    const auto fired = runBars(scheduler, 4, kBeatSamples, 1);
    ASSERT_EQ(fired.size(), 3u);
    for (const auto& hit : fired)
    {
        EXPECT_EQ(hit.beat, 1u);
        EXPECT_EQ(hit.sample, 0u);
    }
}

TEST(PatternSchedulerTest, PatternRepeatsEveryBar)
{
    const HitPattern pattern = makePattern({{3.f, Instrument::Kick, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    EXPECT_EQ(runBars(scheduler, 4, kBeatSamples, 5).size(), 5u);
}

TEST(PatternSchedulerTest, HitsBeyondTheBarLengthNeverFire)
{
    const HitPattern pattern = makePattern({{0.f, Instrument::Kick, 1.f}, {4.f, Instrument::Snare, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    EXPECT_EQ(runBars(scheduler, 3, kBeatSamples, 1).size(), 1u);
}

TEST(PatternSchedulerTest, JumpingForwardSkipsHitsAlreadyPassed)
{
    const HitPattern pattern =
        makePattern({{0.f, Instrument::Kick, 1.f}, {0.25f, Instrument::Hihat, 1.f}, {0.75f, Instrument::Snare, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    std::vector<Instrument> fired;
    const auto record = [&](const Hit& hit) { fired.push_back(hit.instrument); };
    scheduler.step(0, 0, kBeatSamples, record);
    scheduler.step(0, 600, kBeatSamples, record);
    for (size_t pos = 601; pos < kBeatSamples; ++pos)
    {
        scheduler.step(0, pos, kBeatSamples, record);
    }
    EXPECT_THAT(fired, ::testing::ElementsAre(Instrument::Kick, Instrument::Snare));
}

TEST(PatternSchedulerTest, ReplacingThePatternRestartsFromTheCurrentPosition)
{
    const HitPattern first = makePattern({{0.f, Instrument::Kick, 1.f}});
    const HitPattern second = makePattern({{1.f, Instrument::Snare, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&first);
    static_cast<void>(runBars(scheduler, 4, kBeatSamples, 1));
    scheduler.setPattern(&second);
    const auto fired = runBars(scheduler, 4, kBeatSamples, 1);
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0].instrument, Instrument::Snare);
}

TEST(PatternSchedulerTest, TempoChangeMidBeatMovesAPendingHitToItsNewOffset)
{
    const HitPattern pattern = makePattern({{0.5f, Instrument::Hihat, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    std::vector<size_t> firedAt;
    for (size_t pos = 0; pos < 100; ++pos)
    {
        scheduler.step(0, pos, 1000, [&](const Hit&) { firedAt.push_back(pos); });
    }
    for (size_t pos = 100; pos < 500; ++pos)
    {
        scheduler.step(0, pos, 500, [&](const Hit&) { firedAt.push_back(pos); });
    }
    EXPECT_THAT(firedAt, ::testing::ElementsAre(250u));
}

TEST(PatternSchedulerTest, TempoChangeMidBeatDoesNotReplayAHitAlreadyFired)
{
    const HitPattern pattern = makePattern({{0.25f, Instrument::Hihat, 1.f}});
    PatternScheduler scheduler;
    scheduler.setPattern(&pattern);
    size_t count = 0;
    for (size_t pos = 0; pos < 300; ++pos)
    {
        scheduler.step(0, pos, 1000, [&](const Hit&) { ++count; });
    }
    for (size_t pos = 300; pos < 400; ++pos)
    {
        scheduler.step(0, pos, 400, [&](const Hit&) { ++count; });
    }
    EXPECT_EQ(count, 1u);
}
