#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/BeatTempoTracker.h"
#include "Analysis/OnsetDetector.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};

[[nodiscard]] double samplesPerBeat(const double bpm)
{
    return 60.0 * kSampleRate / bpm;
}

[[nodiscard]] uint64_t beatPosition(const double bpm, const double beat)
{
    return static_cast<uint64_t>(std::llround(1000.0 + beat * samplesPerBeat(bpm)));
}

[[nodiscard]] BeatTempoTracker trackerFed(const std::vector<uint64_t>& onsets)
{
    BeatTempoTracker tracker(kSampleRate);
    for (const auto onset : onsets)
    {
        tracker.addOnset(onset);
    }
    return tracker;
}

[[nodiscard]] std::vector<uint64_t> steadyPulse(const double bpm, const size_t beats)
{
    std::vector<uint64_t> onsets;
    for (size_t i = 0; i < beats; ++i)
    {
        onsets.push_back(beatPosition(bpm, static_cast<double>(i)));
    }
    return onsets;
}
}

TEST(BeatTempoTrackerTest, NeedsFourBeatsForAReading)
{
    auto tracker = trackerFed(steadyPulse(120.0, 3));
    EXPECT_FALSE(tracker.reading().has_value());
    tracker.addOnset(beatPosition(120.0, 3.0));
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, 120.f, 0.01f);
}

TEST(BeatTempoTrackerTest, SteadyPulseReadsItsTempo)
{
    for (const double bpm : {40.0, 72.0, 120.0, 147.0, 240.0})
    {
        const auto tracker = trackerFed(steadyPulse(bpm, 24));
        ASSERT_TRUE(tracker.reading().has_value()) << bpm;
        EXPECT_NEAR(tracker.reading()->bpm, bpm, 0.01) << bpm;
        EXPECT_LT(tracker.reading()->jitterMs, 0.01f) << bpm;
        EXPECT_EQ(tracker.reading()->beats, BeatTempoTracker::kMaxOnsets);
    }
}

TEST(BeatTempoTrackerTest, SkippedHitDoesNotChangeTempo)
{
    auto onsets = steadyPulse(100.0, 16);
    onsets.erase(onsets.begin() + 7);
    const auto tracker = trackerFed(onsets);
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, 100.f, 0.01f);
}

TEST(BeatTempoTrackerTest, ExtraHitIsIgnored)
{
    auto onsets = steadyPulse(100.0, 16);
    onsets.insert(onsets.begin() + 8, beatPosition(100.0, 7.4));
    const auto tracker = trackerFed(onsets);
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, 100.f, 0.01f);
    EXPECT_EQ(tracker.reading()->beats, 16u);
}

TEST(BeatTempoTrackerTest, ExtraHitAsOldestOnsetIsIgnored)
{
    auto onsets = steadyPulse(90.0, 12);
    onsets.insert(onsets.begin(), beatPosition(90.0, -0.6));
    const auto tracker = trackerFed(onsets);
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, 90.f, 0.01f);
    EXPECT_EQ(tracker.reading()->beats, 12u);
}

TEST(BeatTempoTrackerTest, HumanisedTimingReportsJitter)
{
    std::mt19937 rng(1234);
    std::normal_distribution<double> deviationMs(0.0, 5.0);
    std::vector<uint64_t> onsets;
    for (size_t i = 0; i < 17; ++i)
    {
        const double offset = deviationMs(rng) * 0.001 * kSampleRate;
        onsets.push_back(
            static_cast<uint64_t>(std::llround(10000.0 + static_cast<double>(i) * samplesPerBeat(120.0) + offset)));
    }
    const auto tracker = trackerFed(onsets);
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, 120.f, 0.5f);
    EXPECT_GT(tracker.reading()->jitterMs, 2.5f);
    EXPECT_LT(tracker.reading()->jitterMs, 7.5f);
}

TEST(BeatTempoTrackerTest, FollowsATempoChangeOnceTheWindowHasMoved)
{
    BeatTempoTracker tracker(kSampleRate);
    for (const auto onset : steadyPulse(100.0, 17))
    {
        tracker.addOnset(onset);
    }
    const uint64_t start = beatPosition(100.0, 17.0);
    for (size_t i = 0; i < BeatTempoTracker::kMaxOnsets; ++i)
    {
        tracker.addOnset(start + static_cast<uint64_t>(std::llround(static_cast<double>(i) * samplesPerBeat(130.0))));
    }
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, 130.f, 0.01f);
}

TEST(BeatTempoTrackerTest, SilenceClearsTheHistory)
{
    auto tracker = trackerFed(steadyPulse(120.0, 8));
    const uint64_t last = beatPosition(120.0, 7.0);
    tracker.advanceTo(last + static_cast<uint64_t>(2.9 * kSampleRate));
    EXPECT_TRUE(tracker.reading().has_value());
    tracker.advanceTo(last + static_cast<uint64_t>(3.1 * kSampleRate));
    EXPECT_FALSE(tracker.reading().has_value());
}

TEST(BeatTempoTrackerTest, SilenceTimeoutScalesWithSlowTempo)
{
    auto tracker = trackerFed(steadyPulse(40.0, 8));
    const uint64_t last = beatPosition(40.0, 7.0);
    tracker.advanceTo(last + static_cast<uint64_t>(4.4 * kSampleRate));
    EXPECT_TRUE(tracker.reading().has_value());
    tracker.advanceTo(last + static_cast<uint64_t>(4.6 * kSampleRate));
    EXPECT_FALSE(tracker.reading().has_value());
}

TEST(BeatTempoTrackerTest, ClickTrainThroughOnsetDetector)
{
    constexpr double kBpm{132.0};
    const auto beatSamples = static_cast<size_t>(std::llround(samplesPerBeat(kBpm)));
    std::vector<float> signal(static_cast<size_t>(10.f * kSampleRate), 0.f);
    for (size_t start = 2000; start < signal.size(); start += beatSamples)
    {
        for (size_t n = 0; n < 480 && start + n < signal.size(); ++n)
        {
            const float decay = std::exp(-static_cast<float>(n) / 80.f);
            signal[start + n] = 0.5f * decay * std::sin(static_cast<float>(n) * 0.3f);
        }
    }
    OnsetDetector detector(kSampleRate);
    BeatTempoTracker tracker(kSampleRate);
    for (size_t n = 0; n < signal.size(); ++n)
    {
        if (detector.step(signal[n]))
        {
            tracker.addOnset(n);
        }
        tracker.advanceTo(n);
    }
    ASSERT_TRUE(tracker.reading().has_value());
    EXPECT_NEAR(tracker.reading()->bpm, kBpm, 0.05);
}

}
