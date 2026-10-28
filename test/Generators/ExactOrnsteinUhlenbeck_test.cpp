#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numeric>
#include <tuple>
#include <vector>

#include "Generators/ExactOrnsteinUhlenbeck.h"

namespace AbacDsp::Test
{

namespace
{
struct Moments
{
    double mean;
    double deviation;
};

Moments moments(const std::vector<float>& data)
{
    const double mean = std::accumulate(data.begin(), data.end(), 0.0) / static_cast<double>(data.size());
    double sum = 0.0;
    for (const float v : data)
    {
        sum += (v - mean) * (v - mean);
    }
    return {mean, std::sqrt(sum / static_cast<double>(data.size()))};
}

std::vector<float> run(ExactOrnsteinUhlenbeck& process, const size_t burnIn, const size_t count)
{
    for (size_t i = 0; i < burnIn; ++i)
    {
        std::ignore = process.step();
    }
    std::vector<float> data(count);
    for (auto& v : data)
    {
        v = process.step();
    }
    return data;
}
}

TEST(ExactOrnsteinUhlenbeck, ZeroSigmaProducesConstantOutput)
{
    ExactOrnsteinUhlenbeck process(48000.0f, 42u);
    process.setSigma(0.0f);
    for (int i = 0; i < 4800; ++i)
    {
        ASSERT_NEAR(process.step(), 0.0f, 0.01f);
    }
}

TEST(ExactOrnsteinUhlenbeck, HigherSigmaProducesLargerFluctuations)
{
    ExactOrnsteinUhlenbeck process(48000.0f);
    process.reset(42u);
    process.setSigma(0.1f);
    const auto low = moments(run(process, 0, 4800));
    process.reset(42u);
    process.setSigma(0.8f);
    const auto high = moments(run(process, 0, 4800));
    EXPECT_GT(high.deviation, low.deviation);
}

TEST(ExactOrnsteinUhlenbeck, ResetReproducesTheSequence)
{
    ExactOrnsteinUhlenbeck process(48000.0f);
    process.setSigma(0.6f);
    for (int i = 0; i < 1234; ++i)
    {
        std::ignore = process.step();
    }
    process.reset(4242u);
    const float afterReset = process.step();
    process.reset(4242u);
    EXPECT_FLOAT_EQ(process.step(), afterReset);
}

TEST(ExactOrnsteinUhlenbeck, OutputRemainsConfined)
{
    constexpr float sigma = 1.0f;
    ExactOrnsteinUhlenbeck process(48000.0f, 789u);
    process.setSigma(sigma);
    for (int i = 0; i < 2000; ++i)
    {
        std::ignore = process.step();
    }
    // 3 sigma / sqrt(2 theta) around mu, wider than the true deviation
    const float theta = sigma * 20.0f + 1.0f;
    const float bound = 3.0f * std::sqrt(sigma * sigma / (2.0f * theta));
    for (int i = 0; i < 4800 * 1000; ++i)
    {
        const float x = process.step();
        ASSERT_GT(x, sigma - bound) << "sample " << i;
        ASSERT_LT(x, sigma + bound) << "sample " << i;
    }
}

TEST(ExactOrnsteinUhlenbeck, MeanRevertsToMu)
{
    ExactOrnsteinUhlenbeck process(48000.0f, 42u);
    process.setSigma(0.7f);
    EXPECT_NEAR(moments(run(process, 4800 * 5, 4800 * 5)).mean, 0.7, 0.07);
}

TEST(ExactOrnsteinUhlenbeck, SetValueMovesTheStateAndStepsContinueFromIt)
{
    ExactOrnsteinUhlenbeck a(10.0f, 3u);
    ExactOrnsteinUhlenbeck b(10.0f, 3u);
    a.setSigma(0.4f);
    b.setSigma(0.4f);
    a.setValue(a.getMu());
    EXPECT_EQ(a.getValue(), a.getMu());
    // same noise, different start: the difference decays by e^(-theta dt) per step
    const float decay = std::exp(-a.getTheta() / 10.0f);
    const float a1 = a.step();
    const float b1 = b.step();
    EXPECT_NEAR(a1 - b1, a.getMu() * decay, 1e-6f);
}

TEST(ExactOrnsteinUhlenbeck, SameSeedGivesSameSequence)
{
    ExactOrnsteinUhlenbeck a(100.0f, 7u);
    ExactOrnsteinUhlenbeck b(100.0f, 7u);
    a.setSigma(0.5f);
    b.setSigma(0.5f);
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(a.step(), b.step());
    }
}

TEST(ExactOrnsteinUhlenbeck, DifferentSeedsGiveDifferentSequences)
{
    ExactOrnsteinUhlenbeck a(100.0f, 7u);
    ExactOrnsteinUhlenbeck b(100.0f, 8u);
    a.setSigma(0.5f);
    b.setSigma(0.5f);
    int equal = 0;
    for (int i = 0; i < 1000; ++i)
    {
        equal += a.step() == b.step() ? 1 : 0;
    }
    EXPECT_LT(equal, 5);
}

class ExactOrnsteinUhlenbeckStepRates : public ::testing::TestWithParam<float>
{
};

TEST_P(ExactOrnsteinUhlenbeckStepRates, StationaryDistributionMatchesTheory)
{
    for (const float sigma : {0.2f, 0.5f, 1.0f})
    {
        ExactOrnsteinUhlenbeck process(GetParam(), 11u);
        process.setSigma(sigma);
        // Statistics need many correlation times 1 / theta, not many samples: over K correlation
        // times the standard error of the mean is about sqrt(2 / K) and that of the deviation about
        // sqrt(1 / K) stationary deviations. K = 1000 gives 0.045 and 0.032; tolerances are ~4 of those.
        const float stepsPerCorrelationTime = GetParam() / process.getTheta();
        const auto burnIn = static_cast<size_t>(10.0f * stepsPerCorrelationTime) + 1;
        const auto count = std::max<size_t>(static_cast<size_t>(1000.0f * stepsPerCorrelationTime), 100000);
        const auto m = moments(run(process, burnIn, count));
        const double expected = process.getStationaryDeviation();
        EXPECT_NEAR(m.mean, process.getMu(), 0.2 * expected) << "sigma " << sigma;
        EXPECT_NEAR(m.deviation, expected, 0.12 * expected) << "sigma " << sigma;
    }
}

TEST_P(ExactOrnsteinUhlenbeckStepRates, LagOneCorrelationIsExpOfMinusThetaDt)
{
    const float stepRate = GetParam();
    ExactOrnsteinUhlenbeck process(stepRate, 3u);
    process.setSigma(0.5f);
    const auto data = run(process, 100000, 400000);
    const auto m = moments(data);
    double sum = 0.0;
    for (size_t i = 1; i < data.size(); ++i)
    {
        sum += (data[i] - m.mean) * (data[i - 1] - m.mean);
    }
    const double correlation = sum / static_cast<double>(data.size() - 1) / (m.deviation * m.deviation);
    EXPECT_NEAR(correlation, std::exp(-process.getTheta() / stepRate), 0.01);
}

// 7 steps per second: slow enough that an Euler step diverges
INSTANTIATE_TEST_SUITE_P(StepRates, ExactOrnsteinUhlenbeckStepRates, ::testing::Values(7.0f, 100.0f, 48000.0f));

TEST(ExactOrnsteinUhlenbeck, SlowStepRateStaysStableForLargeSigma)
{
    // theta dt = 21 / 7 = 3: an Euler step would multiply the displacement by -2 each step
    ExactOrnsteinUhlenbeck process(7.0f, 5u);
    process.setSigma(1.0f);
    const float limit = 8.0f * process.getStationaryDeviation();
    for (int i = 0; i < 100000; ++i)
    {
        ASSERT_LT(std::abs(process.step() - process.getMu()), limit + 1.0f) << "step " << i;
    }
}

}
