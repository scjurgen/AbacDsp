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

TEST(RawOnsetCollectorTest, CollectsPushedHitsInOrder)
{
    RawOnsetCollector collector;
    collector.push({1, 100, 24000});
    collector.push({3, 200, 24000});
    ASSERT_EQ(collector.count(), 2u);
    const auto hits = collector.hits();
    EXPECT_EQ(hits[0].beatIndexInBar, 1u);
    EXPECT_EQ(hits[0].beatSamplePos, 100u);
    EXPECT_EQ(hits[1].beatIndexInBar, 3u);
}

TEST(RawOnsetCollectorTest, ResetClearsCollectedHits)
{
    RawOnsetCollector collector;
    collector.push({0, 0, 24000});
    collector.reset();
    EXPECT_EQ(collector.count(), 0u);
}

TEST(RawOnsetCollectorTest, StopsRecordingOnceAtCapacity)
{
    RawOnsetCollector collector;
    for (size_t i = 0; i < RawOnsetCollector::kCapacity + 10; ++i)
    {
        collector.push({0, 0, 24000});
    }
    EXPECT_EQ(collector.count(), RawOnsetCollector::kCapacity);
}

TEST(SlotLabelsTest, LabelsBeatsThenASingleOffBeatSlot)
{
    const auto labels = slotLabels(4, 1);
    ASSERT_EQ(labels.size(), 8u);
    EXPECT_EQ(labels[0], "Beat 1");
    EXPECT_EQ(labels[1], "Beat 1 - Off-beat");
    EXPECT_EQ(labels[6], "Beat 4");
}

TEST(SlotLabelsTest, LabelsMultipleSubdivisionsNumerically)
{
    const auto labels = slotLabels(2, 3);
    ASSERT_EQ(labels.size(), 8u);
    EXPECT_EQ(labels[1], "Beat 1 - Sub 1");
    EXPECT_EQ(labels[3], "Beat 1 - Sub 3");
    EXPECT_EQ(labels[4], "Beat 2");
}

TEST(SlotLabelsTest, NoSubdivisionsYieldsOnlyBeatLabels)
{
    const auto labels = slotLabels(3, 0);
    ASSERT_EQ(labels.size(), 3u);
    EXPECT_EQ(labels[2], "Beat 3");
}

TEST(PositionLabelTest, WholeBeatAndFractionalPosition)
{
    EXPECT_EQ(positionLabel(0.f), "Beat 1");
    EXPECT_EQ(positionLabel(2.f), "Beat 3");
    EXPECT_EQ(positionLabel(1.5f), "Beat 2 + 0.5");
    EXPECT_EQ(positionLabel(0.25f), "Beat 1 + 0.25");
}

TEST(BuiltInGridTest, EighthGridHasABeatAndAnOffBeatPerBeat)
{
    const auto grid = builtInGrid(AbacDsp::SubdivType::Eighth, "8th", 3, 1.5f);
    ASSERT_EQ(grid.positions.size(), 6u);
    EXPECT_FLOAT_EQ(grid.positions[0], 0.f);
    EXPECT_NEAR(grid.positions[1], 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(grid.positions[2], 1.f);
    EXPECT_EQ(grid.labels.size(), grid.positions.size());
    EXPECT_EQ(grid.name, "8th");
}

TEST(BuiltInGridTest, QuarterGridHasOnlyBeats)
{
    const auto grid = builtInGrid(AbacDsp::SubdivType::None, "Quarter", 4, 1.5f);
    ASSERT_EQ(grid.positions.size(), 4u);
    EXPECT_FLOAT_EQ(grid.positions[3], 3.f);
}

TEST(BuiltInGridTest, ShuffleGridPlacesTheOffBeatBySwingRatio)
{
    const auto grid = builtInGrid(AbacDsp::SubdivType::Shuffle, "Shuffle", 1, 2.f);
    ASSERT_EQ(grid.positions.size(), 2u);
    EXPECT_NEAR(grid.positions[1], 2.f / 3.f, 1e-4f);
}

TEST(ScriptGridTest, LabelsAndNameFollowThePositions)
{
    const std::vector<float> positions{0.f, 1.5f, 3.f};
    const auto grid = scriptGrid(positions);
    EXPECT_EQ(grid.name, "Script (3 positions)");
    ASSERT_EQ(grid.labels.size(), 3u);
    EXPECT_EQ(grid.labels[1], "Beat 2 + 0.5");
}

TEST(EvaluateHitsTest, HitOnAPositionHasZeroDeviationAndThatSlot)
{
    const std::vector<float> positions{0.f, 1.f, 2.f, 3.f};
    const std::vector<RawOnsetHit> raw{{2, 0, 24000}};
    const auto hits = evaluateHits(raw, positions, 4, 48000.f).matched;
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].slot, 2u);
    EXPECT_NEAR(hits[0].deviationMs, 0.f, 1e-3f);
}

TEST(EvaluateHitsTest, LateHitHasNegativeDeviationInMilliseconds)
{
    const std::vector<float> positions{0.f, 1.f};
    const std::vector<RawOnsetHit> raw{{0, 480, 24000}};
    const auto hits = evaluateHits(raw, positions, 2, 48000.f).matched;
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].slot, 0u);
    EXPECT_NEAR(hits[0].deviationMs, -10.f, 1e-3f);
}

TEST(EvaluateHitsTest, EarlyHitHasPositiveDeviation)
{
    const std::vector<float> positions{0.f, 1.f};
    const std::vector<RawOnsetHit> raw{{0, 23520, 24000}};
    const auto hits = evaluateHits(raw, positions, 2, 48000.f).matched;
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].slot, 1u);
    EXPECT_NEAR(hits[0].deviationMs, 10.f, 1e-3f);
}

TEST(EvaluateHitsTest, TheEndOfTheBarIsCloseToItsStart)
{
    const std::vector<float> positions{0.f, 1.f, 2.f, 3.f};
    const std::vector<RawOnsetHit> raw{{3, 23520, 24000}};
    const auto hits = evaluateHits(raw, positions, 4, 48000.f).matched;
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].slot, 0u);
    EXPECT_NEAR(hits[0].deviationMs, 10.f, 1e-3f);
}

TEST(EvaluateHitsTest, PositionsInsideABeatAreMatchedAcrossBeats)
{
    const std::vector<float> positions{0.f, 1.25f, 2.5f};
    const std::vector<RawOnsetHit> raw{{1, 6000, 24000}};
    const auto hits = evaluateHits(raw, positions, 4, 48000.f).matched;
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].slot, 1u);
    EXPECT_NEAR(hits[0].barPosition, 1.25f, 1e-5f);
}

TEST(EvaluateHitsTest, EmptyGridYieldsNoHits)
{
    const std::vector<RawOnsetHit> raw{{0, 0, 24000}};
    EXPECT_TRUE(evaluateHits(raw, std::span<const float>{}, 4, 48000.f).matched.empty());
}

TEST(EvaluateHitsTest, BuiltInEighthGridMatchesTheSequencerBehaviour)
{
    const auto grid = builtInGrid(AbacDsp::SubdivType::Eighth, "8th", 4, 1.5f);
    const std::vector<RawOnsetHit> raw{{1, 12000, 24000}, {1, 12480, 24000}};
    const auto hits = evaluateHits(raw, grid.positions, 4, 48000.f).matched;
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0].slot, 3u);
    EXPECT_NEAR(hits[0].deviationMs, 0.f, 0.1f);
    EXPECT_EQ(hits[1].slot, 3u);
    EXPECT_NEAR(hits[1].deviationMs, -10.f, 0.1f);
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

TEST(EvaluateHitsTest, HitFartherThanTheMatchWindowIsIgnored)
{
    const std::vector<float> positions{0.f, 1.f};
    const std::vector<RawOnsetHit> raw{{0, 6000, 24000}};
    EXPECT_TRUE(evaluateHits(raw, positions, 2, 48000.f).matched.empty());
}

TEST(EvaluateHitsTest, MatchWindowIsSeventyMillisecondsEachSide)
{
    const std::vector<float> positions{0.f, 1.f};
    const std::vector<RawOnsetHit> raw{{0, 3355, 24000}, {0, 3365, 24000}, {1, 24000 - 3355, 24000}};
    const auto hits = evaluateHits(raw, positions, 2, 48000.f).matched;
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_NEAR(hits[0].deviationMs, -69.9f, 0.05f);
    EXPECT_NEAR(hits[1].deviationMs, 69.9f, 0.05f);
}

TEST(EvaluateHitsTest, ExplicitMatchWindowOverridesTheDefault)
{
    const std::vector<float> positions{0.f, 1.f};
    const std::vector<RawOnsetHit> raw{{0, 6000, 24000}};
    EXPECT_EQ(evaluateHits(raw, positions, 2, 48000.f, 200.f).matched.size(), 1u);
    EXPECT_TRUE(evaluateHits(raw, positions, 2, 48000.f, 100.f).matched.empty());
}

TEST(EvaluateHitsTest, IgnoredHitsDoNotShiftTheSlotsOfKeptHits)
{
    const std::vector<float> positions{0.f, 1.f, 2.f};
    const std::vector<RawOnsetHit> raw{{0, 12000, 24000}, {2, 0, 24000}};
    const auto hits = evaluateHits(raw, positions, 3, 48000.f).matched;
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].slot, 2u);
}

TEST(EvaluateHitsTest, IgnoredHitsKeepTheirBarPosition)
{
    const std::vector<float> positions{0.f, 1.f};
    const std::vector<RawOnsetHit> raw{{0, 100, 24000}, {1, 12000, 24000}, {1, 6000, 24000}};
    const auto evaluated = evaluateHits(raw, positions, 2, 48000.f);
    ASSERT_EQ(evaluated.matched.size(), 1u);
    ASSERT_EQ(evaluated.ignoredBarPositions.size(), 2u);
    EXPECT_NEAR(evaluated.ignoredBarPositions[0], 1.5f, 1e-5f);
    EXPECT_NEAR(evaluated.ignoredBarPositions[1], 1.25f, 1e-5f);
}

TEST(FullBarHistogramBinsTest, CountsPositionsOnTheLeadInShiftedAxis)
{
    const std::vector<float> positions{0.f, 0.f, 3.9f};
    const auto bins = fullBarHistogramBins(positions, 4.f, -0.25f, 0.25f);
    ASSERT_EQ(bins.size(), 16u);
    EXPECT_EQ(bins[1], 2u);
    EXPECT_EQ(bins[0], 1u);
}

TEST(RenderFullBarHistogramTest, DrawsIgnoredHitsInGrayStackedOnTheMatchedBars)
{
    EvaluatedHits hits;
    hits.matched.push_back({0.f, 0, 1.f});
    hits.ignoredBarPositions.push_back(1.f);
    const std::vector<float> positions{0.f, 1.f};
    const auto svg = renderFullBarHistogramSvg(hits, 2, positions, 120.f);
    EXPECT_NE(svg.find(kIgnoredBarColor), std::string::npos);
    EXPECT_NE(svg.find(kMatchedBarColor), std::string::npos);
}

TEST(RenderFullBarHistogramTest, WithoutIgnoredHitsNoGrayBarIsDrawn)
{
    EvaluatedHits hits;
    hits.matched.push_back({0.f, 0, 1.f});
    const std::vector<float> positions{0.f, 1.f};
    const auto svg = renderFullBarHistogramSvg(hits, 2, positions, 120.f);
    EXPECT_EQ(svg.find(kIgnoredBarColor), std::string::npos);
}

TEST(BuildReportHtmlTest, EmbedsBootstrapCdnAndSvgCharts)
{
    RawOnsetCollector collector;
    collector.push({0, 100, 24000});
    collector.push({1, 0, 24000});
    collector.push({2, 12000, 24000});
    const auto grid = builtInGrid(AbacDsp::SubdivType::Eighth, "8th", 4, 1.5f);
    const auto html = buildReportHtml(collector.hits(), 4, grid, 120.f, 48000.f, "4/4 8th");
    EXPECT_NE(html.find("cdn.jsdelivr.net/npm/bootstrap"), std::string::npos);
    EXPECT_NE(html.find("<svg"), std::string::npos);
    EXPECT_NE(html.find("4/4 8th"), std::string::npos);
    EXPECT_NE(html.find("Beat 1"), std::string::npos);
    EXPECT_NE(html.find("Off-beat"), std::string::npos);
}

TEST(BuildReportHtmlTest, ShowsTheScriptGridNameAndPositionLabels)
{
    RawOnsetCollector collector;
    collector.push({0, 6000, 24000});
    const std::vector<float> positions{0.f, 0.25f, 0.5f};
    const auto html = buildReportHtml(collector.hits(), 4, scriptGrid(positions), 120.f, 48000.f, "4/4");
    EXPECT_NE(html.find("Script (3 positions)"), std::string::npos);
    EXPECT_NE(html.find("Beat 1 + 0.25"), std::string::npos);
}

TEST(BuildReportHtmlTest, WithoutHitsStillListsEveryGridPosition)
{
    RawOnsetCollector collector;
    const auto grid = builtInGrid(AbacDsp::SubdivType::None, "Quarter", 3, 1.5f);
    const auto html = buildReportHtml(collector.hits(), 3, grid, 120.f, 48000.f, "3/4");
    EXPECT_NE(html.find("Beat 3"), std::string::npos);
}

TEST(BuildReportHtmlTest, ShowsHowManyHitsFellOutsideTheMatchWindow)
{
    RawOnsetCollector collector;
    collector.push({0, 100, 24000});
    collector.push({0, 6000, 24000});
    collector.push({0, 12000, 24000});
    const std::vector<float> positions{0.f, 1.f};
    const auto html = buildReportHtml(collector.hits(), 2, scriptGrid(positions), 120.f, 48000.f, "2/4");
    EXPECT_NE(html.find("Ignored (> 70 ms)</h6><p class=\"fs-3 mb-0\">2</p>"), std::string::npos);
    EXPECT_NE(html.find("Hits</h6><p class=\"fs-3 mb-0\">1</p>"), std::string::npos);
}

TEST(BuildReportHtmlTest, LegendNamesMatchedAndIgnoredHits)
{
    RawOnsetCollector collector;
    collector.push({0, 100, 24000});
    const std::vector<float> positions{0.f, 1.f};
    const auto html = buildReportHtml(collector.hits(), 2, scriptGrid(positions), 120.f, 48000.f, "2/4");
    EXPECT_NE(html.find("Matched hits"), std::string::npos);
    EXPECT_NE(html.find("ignored hits in gray"), std::string::npos);
}
