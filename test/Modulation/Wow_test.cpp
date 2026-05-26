
#include <algorithm>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <numeric>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Analysis/SimpleStats.h"
#include "Analysis/ZeroCrossings.h"
#include "Modulation/Wow.h"

namespace AbacDsp::Test
{

class WowTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_wow = std::make_unique<Wow>(m_sampleRate);
    }

    void TearDown() override
    {
        m_wow.reset();
    }

    float m_sampleRate{48000.f};
    std::unique_ptr<Wow> m_wow;

    std::vector<float> generateSteps(const size_t numSteps)
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
            (void) m_wow->step();
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
    SimpleStats<float> stats;
    for (auto v : result)
    {
        stats.addDataPoint(v);
    }
    EXPECT_NEAR(stats.getMin(), -0.12566f, 1E-3f);
    EXPECT_NEAR(stats.getMax(), 0.12566f, 1E-3f);
    const auto periodLength =
        periodLengthByZeroCrossingAverage(result.data(), result.size(), [](const float in) { return in; });
    EXPECT_NEAR(periodLength, m_sampleRate / f, 0.1f);
}

TEST_F(WowTest, VarianceAffectsVariability)
{
    m_wow->seed(42);
    m_wow->setRate(3.5f);
    m_wow->setDepth(0.8f);
    m_wow->setDrift(0.0f);

    // Test with low variance
    m_wow->setVariance(0.0f);
    settle(3000);
    const auto lowVarianceResults = generateSteps(1000000);
    const auto lowVarianceStats = computeStats(lowVarianceResults);

    // Test with very high variance
    m_wow->setVariance(2.0f);
    const auto highVarianceResults = generateSteps(1000000);
    const auto highVarianceStats = computeStats(highVarianceResults);
    // Higher variance should result in greater standard deviation
    EXPECT_GT(highVarianceStats.stddev, lowVarianceStats.stddev);
}

TEST_F(WowTest, OutputRangeIsReasonable)
{
    m_wow->seed(42);
    m_wow->setRate(0.7f);
    m_wow->setDepth(1.0f);
    m_wow->setVariance(1.0f);
    m_wow->setDrift(1.0f);

    settle(3000);

    const auto results = generateSteps(2000000);
    const auto stats = computeStats(results);
    EXPECT_GT(stats.min, -0.02f);
    EXPECT_LT(stats.max, 0.02f);
    EXPECT_NEAR(stats.mean, 0.f, 0.001f);
    EXPECT_GT(stats.stddev, 0.001f);
}

TEST_F(WowTest, DriftAffectsFrequencyStability)
{
    m_wow->seed(42);
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
    EXPECT_LT(withDriftStats.min, -0.0001f);
    EXPECT_GT(withDriftStats.max, 0.0001f);
}

TEST_F(WowTest, BoundaryValues)
{
    for (std::vector testValues = {0.0f, 0.001f, 0.999f, 1.0f}; auto rate : testValues)
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
    EXPECT_NEAR(stats.meanPeriodLen, 6865.f, 50.f);
    EXPECT_NEAR(stats.minPeriodLength, 6274.f, 10.f);
    EXPECT_NEAR(stats.maxPeriodLength, 7564.f, 20.f);
    EXPECT_NEAR(stats.standardDeviation, 381.f, 15.f);
    EXPECT_NEAR(stats.periodCount, 581.0, 10.0);
}

}