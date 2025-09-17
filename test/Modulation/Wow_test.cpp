
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "Analysis/SimpleStats.h"
#include "Analysis/ZeroCrossings.h"
#include "AudioFile/SaveWav.h"
#include "Modulation/Wow.h"

namespace AbacDsp::Test
{

class WowTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_wow = std::make_unique<AbacDsp::Wow>(m_sampleRate);
    }

    void TearDown() override
    {
        m_wow.reset();
    }

    float m_sampleRate{48000.f};
    std::unique_ptr<AbacDsp::Wow> m_wow;

    // Helper to generate multiple steps and analyze behavior
    std::vector<float> generateSteps(size_t numSteps)
    {
        std::vector<float> results;
        results.reserve(numSteps);
        for (size_t i = 0; i < numSteps; ++i)
        {
            const auto v = m_wow->step();
            results.push_back(v);
        }
        return results;
    }

    // Helper to calculate statistics
    struct Stats
    {
        float mean;
        float min;
        float max;
        float stddev;
    };

    Stats computeStats(const std::vector<float>& data)
    {
        Stats stats{};
        if (data.empty())
        {
            return stats;
        }

        const float sum = std::accumulate(data.begin(), data.end(), 0.0f);
        stats.mean = sum / static_cast<float>(data.size());
        auto [fst, snd] = std::minmax_element(data.begin(), data.end());
        stats.min = *fst;
        stats.max = *snd;

        float variance = 0.0f;
        for (const auto& val : data)
        {
            variance += (val - stats.mean) * (val - stats.mean);
        }
        variance /= static_cast<float>(data.size());
        stats.stddev = std::sqrt(variance);
        return stats;
    }

    void settle(const size_t count = 2000)
    {
        for (size_t i = 0; i < count; ++i)
        {
            m_wow->step();
        }
    }
};

TEST_F(WowTest, DepthAffectsModulation)
{
    m_wow->setRate(0.5f);
    m_wow->setVariance(0.0f);
    m_wow->setDrift(0.0f);

    m_wow->setDepth(0.1f);
    settle(2000);
    const auto lowDepthResults = generateSteps(500);
    const auto lowDepthStats = computeStats(lowDepthResults);

    m_wow->setDepth(0.8f);
    settle(2000);
    const auto highDepthResults = generateSteps(500);
    const auto highDepthStats = computeStats(highDepthResults);

    const auto lowDepthRange = lowDepthStats.max - lowDepthStats.min;
    const auto highDepthRange = highDepthStats.max - highDepthStats.min;

    EXPECT_GT(highDepthRange, lowDepthRange);
}

TEST_F(WowTest, RateAffectsFrequency)
{
    m_wow->setDepth(0.5f);
    m_wow->setVariance(0.0f);
    m_wow->setDrift(0.0f);
    constexpr float f = 40.f;
    m_wow->setRate(f);
    settle(2000);
    const auto result = generateSteps(100000);
    const auto periodLength =
        AbacDsp::periodLengthByZeroCrossingAverage(result.data(), result.size(), [](const float in) { return in - 1; });
    EXPECT_NEAR(periodLength, m_sampleRate / f, 0.1f);
}

TEST_F(WowTest, VarianceAffectsVariability)
{
    m_wow->setRate(3.5f);
    m_wow->setDepth(0.8f);
    m_wow->setDrift(0.0f);

    // Test with low variance
    m_wow->setVariance(0.0f);
    settle(3000);
    const auto lowVarianceResults = generateSteps(100000);
    const auto lowVarianceStats = computeStats(lowVarianceResults);

    // Test with very high variance
    m_wow->setVariance(2.0f);
    const auto highVarianceResults = generateSteps(100000);
    const auto highVarianceStats = computeStats(highVarianceResults);
    // Higher variance should result in greater standard deviation
    EXPECT_GT(highVarianceStats.stddev, lowVarianceStats.stddev);
}

TEST_F(WowTest, OutputRangeIsReasonable)
{
    m_wow->setRate(0.7f);
    m_wow->setDepth(1.0f);
    m_wow->setVariance(1.0f);
    m_wow->setDrift(1.0f);

    settle(3000);

    const auto results = generateSteps(200000);
    const auto stats = computeStats(results);

    EXPECT_GT(stats.min, 0.7f);
    EXPECT_LT(stats.max, 1.3f);
    EXPECT_NEAR(stats.mean, 1.f, 0.1f);
    EXPECT_GT(stats.stddev, 0.f);
}

TEST_F(WowTest, DriftAffectsFrequencyStability)
{
    m_wow->setRate(0.5f);
    m_wow->setDepth(0.3f);
    m_wow->setVariance(0.1f);

    // Generate long sequences to observe frequency drift effects
    m_wow->setDrift(0.0f);
    settle(2000);
    const auto noDriftResults = generateSteps(100000);

    // Reset and test with drift
    m_wow = std::make_unique<AbacDsp::Wow>(m_sampleRate);
    m_wow->setRate(5.f);
    m_wow->setDepth(1.f);
    m_wow->setVariance(0.0f);
    m_wow->setDrift(1.f);
    settle(2000);
    const auto withDriftResults = generateSteps(100000);

    auto noDriftStats = computeStats(noDriftResults);
    auto withDriftStats = computeStats(withDriftResults);
    EXPECT_GT(noDriftStats.min, withDriftStats.min);
    EXPECT_LT(noDriftStats.max, withDriftStats.max);
    EXPECT_LT(withDriftStats.min, 0.7f);
    EXPECT_GT(withDriftStats.max, 1.3f);
}

TEST_F(WowTest, BoundaryValues)
{
    for (std::vector<float> testValues = {0.0f, 0.001f, 0.999f, 1.0f}; float rate : testValues)
    {
        for (float depth : testValues)
        {
            for (float variance : testValues)
            {
                for (float drift : testValues)
                {
                    m_wow->setRate(rate);
                    m_wow->setDepth(depth);
                    m_wow->setVariance(variance);
                    m_wow->setDrift(drift);

                    // Run a few steps to ensure no crashes
                    for (int i = 0; i < 100; ++i)
                    {
                        const auto result = m_wow->step();
                        EXPECT_TRUE(std::isfinite(result)) << "Invalid result with rate=" << rate << ", depth=" << depth
                                                           << ", variance=" << variance << ", drift=" << drift;
                    }
                }
            }
        }
    }
}

TEST_F(WowTest, ResultLong)
{
    std::vector<float> allResults;
    constexpr size_t longRun = 2000000;
    allResults.reserve(longRun);

    m_wow->setRate(10.f);
    m_wow->setDepth(1.0f);
    m_wow->setVariance(1.f);
    m_wow->setDrift(1.f);

    for (size_t i = 0; i < longRun; ++i)
    {
        float result = m_wow->step() - 1;

        allResults.push_back(result);

        ASSERT_TRUE(std::isfinite(result)) << "Non-finite value at step " << i;
    }
    auto [mean, min, max, stddev] = computeStats(allResults);
    EXPECT_NEAR(mean, 0.f, 0.001f);
    EXPECT_GT(stddev, 0.4f);
    EXPECT_LT(min, -0.6f);
    EXPECT_GT(min, -0.8f);
    EXPECT_LT(max, 0.8f);
    EXPECT_GT(max, 0.6f);
}

TEST_F(WowTest, driftTiming)
{
    constexpr size_t longRun = 4000000;
    std::vector<float> allResults;
    allResults.reserve(longRun);
    m_wow->seed(42);
    m_wow->setRate(7.f);
    m_wow->setDepth(1.0f);
    m_wow->setVariance(0.f);
    m_wow->setDrift(1.f);

    for (size_t i = 0; i < longRun; ++i)
    {
        allResults.push_back(m_wow->step());
    }
    auto stats = AbacDsp::calculateZeroCrossingStatistics(allResults.data(), allResults.size(), true);
    // be generous, but not overly
    EXPECT_NEAR(stats.meanPeriodLen, 6858.f, 50.f);
    EXPECT_NEAR(stats.minPeriodLength, 6274.f, 10.f);
    EXPECT_NEAR(stats.maxPeriodLength, 7554.f, 20.f);
    EXPECT_NEAR(stats.standardDeviation, 374.f, 15.f);
    EXPECT_NEAR(stats.periodCount, 582.0, 10.0);
}


TEST_F(WowTest, driftTimingStatsCollection)
{
    GTEST_SKIP() << "Skipping generation test results";
    std::vector<float> allResults;
    constexpr size_t longRun = 4000000;
    allResults.reserve(longRun);
    SimpleStats<float> meanPeriodStats;
    SimpleStats<float> minPeriodStats;
    SimpleStats<float> maxPeriodStats;
    SimpleStats<float> stdDevStats;
    SimpleStats<size_t> countStats;

    for (size_t seed = 1; seed < 10000; ++seed)
    {
        allResults.clear();
        m_wow->seed(seed);

        m_wow->setRate(7.f);
        m_wow->setDepth(1.0f);
        m_wow->setVariance(0.f);
        m_wow->setDrift(1.f);

        for (size_t i = 0; i < longRun; ++i)
        {
            allResults.push_back(m_wow->step());
        }

        auto stats = AbacDsp::calculateZeroCrossingStatistics(allResults.data(), allResults.size(), true);

        meanPeriodStats.addDataPoint(stats.meanPeriodLen);
        minPeriodStats.addDataPoint(stats.minPeriodLength);
        maxPeriodStats.addDataPoint(stats.maxPeriodLength);
        stdDevStats.addDataPoint(stats.standardDeviation);
        countStats.addDataPoint(stats.periodCount);
    }

    std::cout << "\n=== Analysis Results ===\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "meanPeriodLen: " << meanPeriodStats.getMean() << " ± " << meanPeriodStats.getStdDev() << "\n";
    std::cout << "minPeriodLength: " << minPeriodStats.getMean() << " ± " << minPeriodStats.getStdDev() << "\n";
    std::cout << "maxPeriodLength: " << maxPeriodStats.getMean() << " ± " << maxPeriodStats.getStdDev() << "\n";
    std::cout << "standardDeviation: " << stdDevStats.getMean() << " ± " << stdDevStats.getStdDev() << "\n";
    std::cout << "periodCount: " << countStats.getMean() << " ± " << countStats.getStdDev() << "\n";
}
}