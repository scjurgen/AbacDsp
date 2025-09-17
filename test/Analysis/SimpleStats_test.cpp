#include <gtest/gtest.h>
#include <sstream>
#include <cmath>
#include "Analysis/SimpleStats.h"
namespace AbacDsp::Test
{
class SimpleStatsTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_testData = {0.f, 1.f, 0.f, -1.f, 2.f, 0.f, 1.f};

        // Expected results:
        // Count: 7
        // Mean: (0+1+0-1+2+0+1)/7 = 3/7 ≈ 0.428571
        // Min: -1, Max: 2, Range: 3
        // Variance: sample variance with n-1
        // Median (middle value of sorted): 0 (4th element in 7-element sorted array)
        // Q1 (25th percentile): 0
        // Q3 (75th percentile): 1
    }

    std::vector<float> m_testData;
    static constexpr double TOLERANCE = 1e-6;
};

TEST_F(SimpleStatsTest, EmptyStats)
{
    SimpleStats<float> stats;

    EXPECT_EQ(stats.getCount(), 0);
    EXPECT_DOUBLE_EQ(stats.getMean(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getVariance(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getStdDev(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMin(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMax(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getRange(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMedian(), 0.0);
}

TEST_F(SimpleStatsTest, SingleValue)
{
    SimpleStats<float> stats;
    stats.addDataPoint(5.0f);

    EXPECT_EQ(stats.getCount(), 1);
    EXPECT_DOUBLE_EQ(stats.getMean(), 5.0);
    EXPECT_DOUBLE_EQ(stats.getVariance(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getStdDev(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMin(), 5.0);
    EXPECT_DOUBLE_EQ(stats.getMax(), 5.0);
    EXPECT_DOUBLE_EQ(stats.getRange(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMedian(), 5.0);
    EXPECT_DOUBLE_EQ(stats.getQ1(), 5.0);
    EXPECT_DOUBLE_EQ(stats.getQ3(), 5.0);
    EXPECT_DOUBLE_EQ(stats.getIQR(), 0.0);
}

TEST_F(SimpleStatsTest, BasicStatistics)
{
    SimpleStats<float> stats;
    for (const auto value : m_testData)
    {
        stats.addDataPoint(value);
    }
    EXPECT_EQ(stats.getCount(), 7);
    const auto expectedMean = 3.0 / 7.0;
    EXPECT_NEAR(stats.getMean(), expectedMean, TOLERANCE);
    EXPECT_DOUBLE_EQ(stats.getMin(), -1.0);
    EXPECT_DOUBLE_EQ(stats.getMax(), 2.0);
    EXPECT_DOUBLE_EQ(stats.getRange(), 3.0);
}

TEST_F(SimpleStatsTest, VarianceAndStdDev)
{
    SimpleStats<float> stats;

    for (const auto value : m_testData)
    {
        stats.addDataPoint(value);
    }

    const auto mean = 3.0 / 7.0;
    auto variance_sum = 0.0;

    for (const auto value : m_testData)
    {
        const auto diff = static_cast<double>(value) - mean;
        variance_sum += diff * diff;
    }

    const auto expectedVariance = variance_sum / 6.0;
    const auto expectedStdDev = std::sqrt(expectedVariance);

    EXPECT_NEAR(stats.getVariance(), expectedVariance, TOLERANCE);
    EXPECT_NEAR(stats.getStdDev(), expectedStdDev, TOLERANCE);
}

TEST_F(SimpleStatsTest, Percentiles)
{
    SimpleStats<float> stats;

    for (const auto value : m_testData)
    {
        stats.addDataPoint(value);
    }

    EXPECT_DOUBLE_EQ(stats.getMedian(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getQ1(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getQ3(), 1.0);
    EXPECT_DOUBLE_EQ(stats.getIQR(), 1.0);
    EXPECT_DOUBLE_EQ(stats.getPercentile(0.0), -1.0);
    EXPECT_DOUBLE_EQ(stats.getPercentile(1.0), 2.0);
}

TEST_F(SimpleStatsTest, OutlierDetection)
{
    SimpleStats<float> stats;

    for (const auto value : m_testData)
    {
        stats.addDataPoint(value);
    }

    EXPECT_FALSE(stats.isOutlier(-1.0f));
    EXPECT_FALSE(stats.isOutlier(0.0f));
    EXPECT_FALSE(stats.isOutlier(1.0f));
    EXPECT_FALSE(stats.isOutlier(2.0f));
    EXPECT_TRUE(stats.isOutlier(-2.0f));
    EXPECT_TRUE(stats.isOutlier(3.0f));
}

TEST_F(SimpleStatsTest, LazyComputation)
{
    SimpleStats<float> stats;

    stats.addDataPoint(1.0f);
    stats.addDataPoint(2.0f);

    const auto mean1 = stats.getMean();
    EXPECT_DOUBLE_EQ(mean1, 1.5);

    const auto mean2 = stats.getMean();
    EXPECT_DOUBLE_EQ(mean2, 1.5);

    stats.addDataPoint(3.0f);
    const auto mean3 = stats.getMean();
    EXPECT_DOUBLE_EQ(mean3, 2.0);
}

TEST_F(SimpleStatsTest, ClearFunctionality)
{
    SimpleStats<float> stats;

    for (const auto value : m_testData)
    {
        stats.addDataPoint(value);
    }

    EXPECT_EQ(stats.getCount(), 7);
    EXPECT_NE(stats.getMean(), 0.0);

    stats.clear();

    EXPECT_EQ(stats.getCount(), 0);
    EXPECT_DOUBLE_EQ(stats.getMean(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getVariance(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMin(), 0.0);
    EXPECT_DOUBLE_EQ(stats.getMax(), 0.0);
}

TEST_F(SimpleStatsTest, StreamOutput)
{
    SimpleStats<float> stats;

    for (const auto value : m_testData)
    {
        stats.addDataPoint(value);
    }

    std::ostringstream oss;
    stats.printSummary(oss, "Test Data");

    const auto output = oss.str();

    EXPECT_NE(output.find("Test Data Statistics:"), std::string::npos);
    EXPECT_NE(output.find("Count:   7"), std::string::npos);
    EXPECT_NE(output.find("Mean:    0.428571"), std::string::npos);
    EXPECT_NE(output.find("Std Dev: 0.9759"), std::string::npos);
    EXPECT_NE(output.find("Min:     -1.00"), std::string::npos);
    EXPECT_NE(output.find("Max:     2.00"), std::string::npos);
    EXPECT_NE(output.find("Range:   3.00"), std::string::npos);
    EXPECT_NE(output.find("Median:  0.00"), std::string::npos);
    EXPECT_NE(output.find("IQR:     1.00"), std::string::npos);
}

TEST_F(SimpleStatsTest, DifferentDataTypes)
{
    SimpleStats<int> intStats;
    intStats.addDataPoint(1);
    intStats.addDataPoint(2);
    intStats.addDataPoint(3);

    EXPECT_EQ(intStats.getCount(), 3);
    EXPECT_DOUBLE_EQ(intStats.getMean(), 2.0);

    SimpleStats<size_t> sizeStats;
    sizeStats.addDataPoint(10U);
    sizeStats.addDataPoint(20U);

    EXPECT_EQ(sizeStats.getCount(), 2);
    EXPECT_DOUBLE_EQ(sizeStats.getMean(), 15.0);
}

TEST_F(SimpleStatsTest, EdgeCasePercentiles)
{
    SimpleStats<float> stats;

    for (int i = 1; i <= 5; ++i)
    {
        stats.addDataPoint(static_cast<float>(i));
    }

    EXPECT_DOUBLE_EQ(stats.getPercentile(0.0), 1.0);
    EXPECT_DOUBLE_EQ(stats.getPercentile(1.0), 5.0);
    EXPECT_DOUBLE_EQ(stats.getPercentile(0.5), 3.0);

    EXPECT_DOUBLE_EQ(stats.getPercentile(0.25), 2.0);

    EXPECT_DOUBLE_EQ(stats.getPercentile(0.75), 4.0);
}
}