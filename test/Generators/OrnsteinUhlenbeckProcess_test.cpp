#include <gtest/gtest.h>
#include <vector>
#include <numeric>
#include <cmath>

#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Analysis/SimpleStats.h"

namespace AbacDsp::Test
{

namespace
{
constexpr auto kSampleRate{48000.f};
constexpr auto kSamplesToProcess{7'654'321u};
constexpr auto kEpsilonMean{1E-3f};
constexpr auto kEpsilonVariance{5E-1f};
constexpr auto kSmallSampleSize{1000u};
}

TEST(OrnsteinUhlenbeckProcessTest, driftStatistics)
{
    OrnsteinUhlenbeckProcess sut(kSampleRate);
    sut.seed(42);
    SimpleStats<float> stats;
    sut.setSigma(0.5f);

    for (size_t i = 0; i < kSamplesToProcess; ++i)
    {
        stats.addDataPoint(sut.step());
    }

    EXPECT_NEAR(stats.getMean(), 0.5f, kEpsilonMean);
}

TEST(OrnsteinUhlenbeckProcessTest, meanReversionProperty)
{
    OrnsteinUhlenbeckProcess sut(kSampleRate);
    sut.seed(42);
    SimpleStats<float> stats;
    constexpr auto sigma = 0.3f;
    sut.setSigma(sigma);

    for (size_t i = 0; i < kSamplesToProcess; ++i)
    {
        stats.addDataPoint(sut.step());
    }

    // Test that mean converges to mu (which equals sigma in the implementation)
    EXPECT_NEAR(stats.getMean(), sigma, kEpsilonMean);

    // Test that variance is reasonable for a mean-reverting process
    EXPECT_GT(stats.getVariance(), 0.0);
    EXPECT_LT(stats.getVariance(), 10.0 * sigma * sigma);
}

TEST(OrnsteinUhlenbeckProcessTest, differentSigmaValues)
{
    constexpr std::array<float, 4> sigmaValues{0.1f, 0.5f, 1.0f, 2.0f};

    for (const auto sigma : sigmaValues)
    {
        OrnsteinUhlenbeckProcess sut(kSampleRate);
        sut.seed(42);
        SimpleStats<float> stats;
        sut.setSigma(sigma);

        for (size_t i = 0; i < kSamplesToProcess; ++i)
        {
            stats.addDataPoint(sut.step());
        }

        EXPECT_NEAR(stats.getMean(), sigma, kEpsilonMean) << "Failed for sigma=" << sigma;
        EXPECT_GT(stats.getStdDev(), 0.0) << "Failed for sigma=" << sigma;
    }
}

TEST(OrnsteinUhlenbeckProcessTest, resetFunctionality)
{
    OrnsteinUhlenbeckProcess sut(kSampleRate);
    sut.seed(42);
    sut.setSigma(1.0f);

    // Generate some values to move away from initial state
    for (size_t i = 0; i < 100; ++i)
    {
        sut.step();
    }

    // Reset and check that the next value starts from a reset state
    sut.reset();
    const auto firstValue = sut.step();

    // After reset, the process should start with small values due to x=0 initial condition
    EXPECT_LT(std::abs(firstValue), 5.0f);
}

TEST(OrnsteinUhlenbeckProcessTest, seedReproducibility)
{
    constexpr auto testSeed = 42u;
    constexpr auto testSigma = 0.7f;

    OrnsteinUhlenbeckProcess sut1(kSampleRate);
    OrnsteinUhlenbeckProcess sut2(kSampleRate);

    sut1.seed(testSeed);
    sut1.setSigma(testSigma);

    sut2.seed(testSeed);
    sut2.setSigma(testSigma);

    std::vector<float> sequence1;
    std::vector<float> sequence2;

    for (size_t i = 0; i < kSmallSampleSize; ++i)
    {
        sequence1.push_back(sut1.step());
        sequence2.push_back(sut2.step());
    }

    // Sequences should be identical with the same seed
    for (size_t i = 0; i < kSmallSampleSize; ++i)
    {
        EXPECT_FLOAT_EQ(sequence1[i], sequence2[i]) << "Mismatch at index " << i;
    }
}

TEST(OrnsteinUhlenbeckProcessTest, autocorrelationProperty)
{
    OrnsteinUhlenbeckProcess sut(kSampleRate);
    sut.seed(42);
    sut.setSigma(0.5f);

    std::vector<float> samples;
    samples.reserve(kSmallSampleSize);

    for (size_t i = 0; i < kSmallSampleSize; ++i)
    {
        samples.push_back(sut.step());
    }

    // Calculate lag-1 autocorrelation
    const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0f) / samples.size();

    auto numerator = 0.0f;
    auto denominator = 0.0f;

    for (size_t i = 0; i < samples.size() - 1; ++i)
    {
        const auto x_i = samples[i] - mean;
        const auto x_i1 = samples[i + 1] - mean;
        numerator += x_i * x_i1;
        denominator += x_i * x_i;
    }

    const auto autocorr = numerator / denominator;

    // Ornstein-Uhlenbeck process should show positive autocorrelation
    EXPECT_GT(autocorr, 0.0f);
    EXPECT_LT(autocorr, 1.0f);
}

TEST(OrnsteinUhlenbeckProcessTest, boundedness)
{
    OrnsteinUhlenbeckProcess sut(kSampleRate);
    sut.seed(42);
    constexpr auto sigma = 1.0f;
    sut.setSigma(sigma);

    SimpleStats<float> stats;
    for (size_t i = 0; i < kSamplesToProcess; ++i)
    {
        stats.addDataPoint(sut.step());
    }

    // Values should be reasonably bounded due to mean reversion
    const auto range = stats.getRange();
    EXPECT_LT(range, 50.0f * sigma);

    // Check that extreme values are rare (most values within reasonable bounds)
    const auto mean = stats.getMean();
    const auto stddev = stats.getStdDev();
    const auto lowerBound = mean - 4.0 * stddev;
    const auto upperBound = mean + 4.0 * stddev;

    EXPECT_GT(stats.getMin(), lowerBound - 1.0f);
    EXPECT_LT(stats.getMax(), upperBound + 1.0f);
}

TEST(OrnsteinUhlenbeckProcessTest, differentSampleRates)
{
    constexpr std::array<float, 3> sampleRates{22050.f, 48000.f, 96000.f};
    constexpr auto sigma = 0.5f;

    for (const auto sampleRate : sampleRates)
    {
        OrnsteinUhlenbeckProcess sut(sampleRate);
        sut.seed(42);
        sut.setSigma(sigma);

        SimpleStats<float> stats;

        for (size_t i = 0; i < kSamplesToProcess; ++i)
        {
            stats.addDataPoint(sut.step());
        }

        EXPECT_NEAR(stats.getMean(), sigma, kEpsilonMean * 2) << "Failed for sample rate=" << sampleRate;
        EXPECT_GT(stats.getStdDev(), 0.04f);
        EXPECT_LT(stats.getRange(), 1.f) << "Failed for sample rate=" << sampleRate;
    }
}

TEST(OrnsteinUhlenbeckProcessTest, zenoSigmaBehavior)
{
    OrnsteinUhlenbeckProcess sut(kSampleRate);
    sut.seed(42);
    sut.setSigma(0.0f);

    SimpleStats<float> stats;

    for (size_t i = 0; i < kSmallSampleSize; ++i)
    {
        stats.addDataPoint(sut.step());
    }

    // With sigma=0, the process should converge to 0 (no noise)
    EXPECT_NEAR(stats.getMean(), 0.0f, 1E-7f);
    EXPECT_NEAR(stats.getStdDev(), 0.0f, 1E-7f);
}

}