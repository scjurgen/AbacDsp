#include <algorithm>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "impl/BeatAnalysisReport.h"

using namespace MetronomeAnalysis;

namespace
{
constexpr float kSampleRate{48000.f};
}

TEST(OnsetDetectorTest, SilenceNeverFires)
{
    OnsetDetector detector(kSampleRate);
    for (int i = 0; i < kSampleRate; ++i)
    {
        EXPECT_FALSE(detector.step(0.f));
    }
}

TEST(OnsetDetectorTest, LoudTransientFiresOnce)
{
    OnsetDetector detector(kSampleRate);
    int fireCount = 0;
    for (int i = 0; i < 4800; ++i)
    {
        if (detector.step(0.9f))
        {
            ++fireCount;
        }
    }
    EXPECT_EQ(fireCount, 1);
}

TEST(OnsetDetectorTest, MinGapSuppressesASecondImmediateHit)
{
    OnsetDetector detector(kSampleRate);
    bool firstFired = false;
    for (int i = 0; i < 10 && !firstFired; ++i)
    {
        firstFired = detector.step(0.9f);
    }
    ASSERT_TRUE(firstFired);
    // The very next sample is still inside the debounce window and must not fire again.
    EXPECT_FALSE(detector.step(0.9f));
}

TEST(OnsetDetectorTest, StaysDisarmedUntilEnvelopeDropsBelowLowerThreshold)
{
    OnsetDetector detector(kSampleRate);
    bool firstFired = false;
    for (int i = 0; i < 10 && !firstFired; ++i)
    {
        firstFired = detector.step(0.9f);
    }
    ASSERT_TRUE(firstFired);
    // A sustain that stays inside the hysteresis band (below the upper threshold, above the
    // lower one) never re-arms, so it must not fire again even after the debounce window ends.
    int fireCount = 0;
    for (int i = 0; i < static_cast<int>(kSampleRate); ++i)
    {
        const float ripple = 0.06f + 0.005f * std::sin(static_cast<float>(i) * 0.01f);
        if (detector.step(ripple))
        {
            ++fireCount;
        }
    }
    EXPECT_EQ(fireCount, 0);
}

TEST(OnsetDetectorTest, RearmsOnceEnvelopeDropsBelowLowerThreshold)
{
    OnsetDetector detector(kSampleRate);
    bool firstFired = false;
    for (int i = 0; i < 10 && !firstFired; ++i)
    {
        firstFired = detector.step(0.9f);
    }
    ASSERT_TRUE(firstFired);
    for (int i = 0; i < static_cast<int>(kSampleRate); ++i)
    {
        static_cast<void>(detector.step(0.f));
    }
    bool secondFired = false;
    for (int i = 0; i < 10 && !secondFired; ++i)
    {
        secondFired = detector.step(0.9f);
    }
    EXPECT_TRUE(secondFired);
}

TEST(OnsetDetectorTest, ResetClearsArmedStateAndEnvelope)
{
    OnsetDetector detector(kSampleRate);
    for (int i = 0; i < 10; ++i)
    {
        static_cast<void>(detector.step(0.9f));
    }
    detector.reset();
    bool fired = false;
    for (int i = 0; i < 10 && !fired; ++i)
    {
        fired = detector.step(0.9f);
    }
    EXPECT_TRUE(fired);
}

TEST(DeviationCollectorTest, CollectsPushedHitsInOrder)
{
    DeviationCollector collector;
    collector.push(1.5f, 0);
    collector.push(-2.5f, 2);
    ASSERT_EQ(collector.count(), 2u);
    const auto hits = collector.hits();
    EXPECT_FLOAT_EQ(hits[0].deviationMs, 1.5f);
    EXPECT_EQ(hits[0].role, 0u);
    EXPECT_FLOAT_EQ(hits[1].deviationMs, -2.5f);
    EXPECT_EQ(hits[1].role, 2u);
}

TEST(DeviationCollectorTest, ResetClearsCollectedValues)
{
    DeviationCollector collector;
    collector.push(1.f, 0);
    collector.reset();
    EXPECT_EQ(collector.count(), 0u);
}

TEST(DeviationCollectorTest, StopsRecordingOnceAtCapacity)
{
    DeviationCollector collector;
    for (size_t i = 0; i < DeviationCollector::kCapacity + 10; ++i)
    {
        collector.push(1.f, 0);
    }
    EXPECT_EQ(collector.count(), DeviationCollector::kCapacity);
}

TEST(DeviationsForRoleTest, FiltersHitsByRole)
{
    DeviationCollector collector;
    collector.push(1.f, 0);
    collector.push(2.f, 1);
    collector.push(3.f, 0);
    const auto beat0 = deviationsForRole(collector.hits(), 0);
    ASSERT_EQ(beat0.size(), 2u);
    EXPECT_FLOAT_EQ(beat0[0], 1.f);
    EXPECT_FLOAT_EQ(beat0[1], 3.f);
    EXPECT_TRUE(deviationsForRole(collector.hits(), 5).empty());
}

TEST(RoleLabelsTest, LabelsBeatsThenASingleOffBeatSlot)
{
    const auto labels = roleLabels(4, 1);
    ASSERT_EQ(labels.size(), 5u);
    EXPECT_EQ(labels[0], "Beat 1");
    EXPECT_EQ(labels[3], "Beat 4");
    EXPECT_EQ(labels[4], "Off-beat");
}

TEST(RoleLabelsTest, LabelsMultipleSubdivisionsNumerically)
{
    const auto labels = roleLabels(4, 3);
    ASSERT_EQ(labels.size(), 7u);
    EXPECT_EQ(labels[4], "Sub 1");
    EXPECT_EQ(labels[5], "Sub 2");
    EXPECT_EQ(labels[6], "Sub 3");
}

TEST(RoleLabelsTest, NoSubdivisionsYieldsOnlyBeatLabels)
{
    const auto labels = roleLabels(3, 0);
    ASSERT_EQ(labels.size(), 3u);
    EXPECT_EQ(labels[2], "Beat 3");
}

TEST(ComputeStatsTest, EmptyInputYieldsZeroedStats)
{
    const auto stats = computeStats({});
    EXPECT_EQ(stats.count, 0u);
    EXPECT_FLOAT_EQ(stats.meanMs, 0.f);
    EXPECT_FLOAT_EQ(stats.stdDevMs, 0.f);
}

TEST(ComputeStatsTest, ComputesMeanAndStdDeviation)
{
    const std::vector<float> deviations{-10.f, 0.f, 10.f};
    const auto stats = computeStats(deviations);
    EXPECT_EQ(stats.count, 3u);
    EXPECT_FLOAT_EQ(stats.meanMs, 0.f);
    EXPECT_NEAR(stats.stdDevMs, 8.16497f, 1e-3f);
}

TEST(ComputeHistogramTest, BucketsValuesIntoTheirBin)
{
    const std::vector<float> deviations{0.2f, 0.4f, 1.2f, 1.8f};
    const auto histogram = computeHistogram(deviations, 1.f, 2.f);
    ASSERT_EQ(histogram.bins.size(), 4u);
    EXPECT_EQ(histogram.bins[2], 2u);
    EXPECT_EQ(histogram.bins[3], 2u);
}

TEST(ComputeHistogramTest, RangeIsFixedRegardlessOfData)
{
    const auto histogram = computeHistogram({}, 10.f, 100.f);
    EXPECT_EQ(histogram.minMs, -100.f);
    EXPECT_EQ(histogram.bins.size(), 20u);
}

TEST(ComputeHistogramTest, ValuesBeyondRangeClampIntoTheOutermostBin)
{
    const std::vector<float> deviations{-500.f, 500.f};
    const auto histogram = computeHistogram(deviations, 10.f, 100.f);
    EXPECT_EQ(histogram.bins.front(), 1u);
    EXPECT_EQ(histogram.bins.back(), 1u);
}

TEST(ComputeHistogramTest, NonPositiveRangeYieldsNoBins)
{
    const std::vector<float> deviations{1.f};
    EXPECT_TRUE(computeHistogram(deviations, 1.f, 0.f).bins.empty());
}

TEST(BuildReportHtmlTest, EmbedsBootstrapCdnAndSvgCharts)
{
    DeviationCollector collector;
    collector.push(-5.f, roleForBeat(0));
    collector.push(0.f, roleForBeat(1));
    collector.push(5.f, roleForSubdivision(4, 0));
    const auto html = buildReportHtml(collector.hits(), 4, 1, 120.f, "4/4 8th");
    EXPECT_NE(html.find("cdn.jsdelivr.net/npm/bootstrap"), std::string::npos);
    EXPECT_EQ(std::count(html.begin(), html.end(), '<') > 0, true);
    EXPECT_NE(html.find("<svg"), std::string::npos);
    EXPECT_NE(html.find("4/4 8th"), std::string::npos);
    EXPECT_NE(html.find("Beat 1"), std::string::npos);
    EXPECT_NE(html.find("Off-beat"), std::string::npos);
}

TEST(BuildReportHtmlTest, OmitsRoleBreakdownWhenNoLabels)
{
    DeviationCollector collector;
    const auto html = buildReportHtml(collector.hits(), 0, 0, 120.f, "none");
    EXPECT_EQ(html.find("By beat / subdivision"), std::string::npos);
}
