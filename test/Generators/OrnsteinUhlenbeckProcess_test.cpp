#include <cmath>
#include <gtest/gtest.h>
#include <numeric>
#include <tuple>
#include <vector>

#include "Generators/OrnsteinUhlenbeckProcess.h"

namespace AbacDsp::Test
{

class OrnsteinUhlenbeckProcessTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_ouProcess = std::make_unique<OrnsteinUhlenbeckProcess>(m_sampleRate);
    }

    float m_sampleRate{48000.0f};
    std::unique_ptr<OrnsteinUhlenbeckProcess> m_ouProcess;

    static constexpr float m_tolerance = 1e-6f;
    static constexpr int m_numSamples = 4800;
};


TEST_F(OrnsteinUhlenbeckProcessTest, ZeroVarianceProducesConstantOutput)
{
    m_ouProcess->seed(42u);
    constexpr float zeroSigma = 0.0f;
    m_ouProcess->setSigma(zeroSigma);

    // Burn-in until process converges to zero
    for (int i = 0; i < m_numSamples * 1000; ++i)
    {
        if (std::abs(m_ouProcess->step()) < 0.0001f)
        {
            break;
        }
    }

    std::vector<float> samples;
    samples.reserve(m_numSamples);
    for (int i = 0; i < m_numSamples; ++i)
    {
        samples.push_back(m_ouProcess->step());
    }

    for (const float sample : samples)
    {
        EXPECT_NEAR(sample, 0.0f, 0.01f) << "Sample should be close to zero with zero sigma";
    }
}

TEST_F(OrnsteinUhlenbeckProcessTest, HigherVarianceProducesLargerFluctuations)
{
    constexpr float lowSigma = 0.1f;
    constexpr float highSigma = 0.8f;
    constexpr auto seed = 42u;

    auto computeStdDev = [](const std::vector<float>& data) -> float
    {
        const float mean = std::accumulate(data.begin(), data.end(), 0.0f) / data.size();
        float sumSquaredDiffs = 0.0f;
        for (const float value : data)
        {
            const float diff = value - mean;
            sumSquaredDiffs += diff * diff;
        }
        return std::sqrt(sumSquaredDiffs / data.size());
    };

    m_ouProcess->reset(seed);
    m_ouProcess->setSigma(lowSigma);
    std::vector<float> lowSigmaSamples;
    lowSigmaSamples.reserve(m_numSamples);
    for (int i = 0; i < m_numSamples; ++i)
    {
        lowSigmaSamples.push_back(m_ouProcess->step());
    }

    m_ouProcess->reset(seed);
    m_ouProcess->setSigma(highSigma);
    std::vector<float> highSigmaSamples;
    highSigmaSamples.reserve(m_numSamples);
    for (int i = 0; i < m_numSamples; ++i)
    {
        highSigmaSamples.push_back(m_ouProcess->step());
    }

    const float lowStdDev = computeStdDev(lowSigmaSamples);
    const float highStdDev = computeStdDev(highSigmaSamples);

    EXPECT_GT(highStdDev, lowStdDev) << "Higher sigma should produce larger fluctuations. "
                                     << "Low sigma std dev: " << lowStdDev << ", High sigma std dev: " << highStdDev;
}

TEST_F(OrnsteinUhlenbeckProcessTest, ResetClearsState)
{
    constexpr float sigma = 0.6f;
    constexpr auto seed = 4242u;
    m_ouProcess->setSigma(sigma);

    // Advance to arbitrary state, then reset
    for (int i = 0; i < 1234; ++i)
    {
        std::ignore = m_ouProcess->step();
    }
    m_ouProcess->reset(seed);
    const float outputAfterReset = m_ouProcess->step();

    // Fresh instance from same seed should match
    m_ouProcess->reset(seed);
    const float outputFresh = m_ouProcess->step();

    EXPECT_FLOAT_EQ(outputAfterReset, outputFresh);
}


TEST_F(OrnsteinUhlenbeckProcessTest, OutputRemainsConfined)
{
    m_ouProcess->seed(789u);
    constexpr float highSigma = 1.0f;
    m_ouProcess->setSigma(highSigma);

    constexpr int burnInSamples = 2000;
    for (int i = 0; i < burnInSamples; ++i)
    {
        std::ignore = m_ouProcess->step();
    }

    // Calculate theoretical steady-state bounds from OU parameters
    constexpr float theta = highSigma * 20.0f + 1.0f;
    constexpr float mu = highSigma;
    const float steadyStateStd = std::sqrt((highSigma * highSigma) / (2.0f * theta));

    // 3-sigma bound (99.7% confidence)
    const float theoreticalUpperBound = std::abs(mu) + 3.0f * steadyStateStd;
    const float theoreticalLowerBound = std::abs(mu) - 3.0f * steadyStateStd;

#if NDEBUG
    constexpr size_t Slices{1000};
#else
    constexpr size_t Slices{10};
#endif

    for (size_t i = 0; i < m_numSamples * Slices; ++i)
    {
        const float sample = m_ouProcess->step();
        EXPECT_GT(sample, theoreticalLowerBound) << "Output below 3-sigma bound at sample " << i;
        EXPECT_LT(sample, theoreticalUpperBound) << "Output above 3-sigma bound at sample " << i;
    }
}


TEST_F(OrnsteinUhlenbeckProcessTest, MeanReversionBehavior)
{
    m_ouProcess->seed(42u);
    constexpr float targetMean = 0.7f;

    std::vector<float> longSequence;
    longSequence.resize(m_numSamples * 5);
    m_ouProcess->setSigma(targetMean);
    // Burn-in to reach steady state
    for (size_t i = 0; i < longSequence.size(); ++i)
    {
        std::ignore = m_ouProcess->step();
    }
    for (float& sample : longSequence)
    {
        sample = m_ouProcess->step();
    }

    const float mean =
        std::accumulate(longSequence.begin(), longSequence.end(), 0.0f) / static_cast<float>(longSequence.size());

    EXPECT_NEAR(mean, targetMean, 0.07f) << "Long-term mean should approach the target mean due to mean reversion. "
                                         << "Actual mean: " << mean << ", Expected: " << targetMean;
}


}