#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Graph/Nodes/CrossoverLR4.h"

namespace AbacDsp::Graph::Nodes::Test
{

// highOut is derived as in - lowOut (see CrossoverLR4.h) specifically to
// guarantee this property; this test is the proof, not just documentation.
TEST(CrossoverLR4NodeTest, LowPlusHighReconstructsTheInputWithinTolerance)
{
    constexpr float kSampleRate = 48000.f;
    CrossoverLR4 crossover{kSampleRate};
    crossover.setParameter(0, 1000.f);

    constexpr int kNumSamples = 4000;
    constexpr int kMeasureFrom = 500; // skip the filters' settling transient
    float sumSquaresError = 0.f;
    float sumSquaresInput = 0.f;
    for (int i = 0; i < kNumSamples; ++i)
    {
        const float t = static_cast<float>(i) / kSampleRate;
        const float in = 0.3f * std::sin(2.f * std::numbers::pi_v<float> * 100.f * t) +
                         0.3f * std::sin(2.f * std::numbers::pi_v<float> * 1000.f * t) +
                         0.3f * std::sin(2.f * std::numbers::pi_v<float> * 8000.f * t);
        std::array<float, 2> out{};
        std::array<const float*, 1> ins{&in};
        std::array<float*, 2> outs{&out[0], &out[1]};
        crossover.process(ins, outs, 1);

        if (i >= kMeasureFrom)
        {
            const float reconstructed = out[0] + out[1];
            const float error = reconstructed - in;
            sumSquaresError += error * error;
            sumSquaresInput += in * in;
        }
    }

    const float rmsError = std::sqrt(sumSquaresError / static_cast<float>(kNumSamples - kMeasureFrom));
    const float rmsInput = std::sqrt(sumSquaresInput / static_cast<float>(kNumSamples - kMeasureFrom));
    EXPECT_LT(rmsError, 0.05f * rmsInput);
}

TEST(CrossoverLR4NodeTest, LowFrequencyEnergyStaysInLowOutput)
{
    constexpr float kSampleRate = 48000.f;
    CrossoverLR4 crossover{kSampleRate};
    crossover.setParameter(0, 1000.f);

    constexpr int kNumSamples = 4000;
    constexpr int kMeasureFrom = 2000;
    float sumSquaresLow = 0.f;
    float sumSquaresHigh = 0.f;
    for (int i = 0; i < kNumSamples; ++i)
    {
        const float t = static_cast<float>(i) / kSampleRate;
        const float in = std::sin(2.f * std::numbers::pi_v<float> * 100.f * t);
        std::array<float, 2> out{};
        std::array<const float*, 1> ins{&in};
        std::array<float*, 2> outs{&out[0], &out[1]};
        crossover.process(ins, outs, 1);

        if (i >= kMeasureFrom)
        {
            sumSquaresLow += out[0] * out[0];
            sumSquaresHigh += out[1] * out[1];
        }
    }

    EXPECT_GT(sumSquaresLow, sumSquaresHigh * 10.f);
}

}
