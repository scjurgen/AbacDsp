#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Filters/Biquad.h"
#include "Graph/Nodes/CrossoverLR4.h"

namespace AbacDsp::Graph::Nodes::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr float kCrossoverHz = 1000.f;
constexpr float kButterworthQ = 0.70710678f;
constexpr int kNumSamples = 8000;
constexpr int kMeasureFrom = 4000;

struct BandRms
{
    float low;
    float high;
};

[[nodiscard]] BandRms measureBandRms(const float testHz)
{
    CrossoverLR4 crossover{kSampleRate};
    crossover.setParameter(0, kCrossoverHz);

    float sumSquaresLow = 0.f;
    float sumSquaresHigh = 0.f;
    float sumSquaresInput = 0.f;
    for (int i = 0; i < kNumSamples; ++i)
    {
        const float t = static_cast<float>(i) / kSampleRate;
        const float in = std::sin(2.f * std::numbers::pi_v<float> * testHz * t);
        std::array<float, 2> out{};
        std::array<const float*, 1> ins{&in};
        std::array<float*, 2> outs{&out[0], &out[1]};
        crossover.process(ins, outs, 1);

        if (i >= kMeasureFrom)
        {
            sumSquaresLow += out[0] * out[0];
            sumSquaresHigh += out[1] * out[1];
            sumSquaresInput += in * in;
        }
    }
    return {std::sqrt(sumSquaresLow / sumSquaresInput), std::sqrt(sumSquaresHigh / sumSquaresInput)};
}
}

TEST(CrossoverLR4NodeTest, LowPlusHighIsTheAllpassOfTheInput)
{
    CrossoverLR4 crossover{kSampleRate};
    crossover.setParameter(0, kCrossoverHz);
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::AllPass> allpass;
    allpass.computeCoefficients(kSampleRate, kCrossoverHz, kButterworthQ, 0.f);

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
        const float expected = allpass.singleStepAllPass(in);

        if (i >= kMeasureFrom)
        {
            const float error = out[0] + out[1] - expected;
            sumSquaresError += error * error;
            sumSquaresInput += in * in;
        }
    }

    EXPECT_LT(std::sqrt(sumSquaresError / sumSquaresInput), 1e-3f);
}

TEST(CrossoverLR4NodeTest, BothBandsAreMinusSixDbAtTheCrossoverFrequency)
{
    const auto [low, high] = measureBandRms(kCrossoverHz);
    EXPECT_NEAR(low, 0.5f, 0.01f);
    EXPECT_NEAR(high, 0.5f, 0.01f);
}

TEST(CrossoverLR4NodeTest, LowFrequencyEnergyStaysInLowOutput)
{
    const auto [low, high] = measureBandRms(100.f);
    EXPECT_GT(low, 0.99f);
    EXPECT_LT(high, 1e-3f);
}

TEST(CrossoverLR4NodeTest, HighFrequencyEnergyStaysInHighOutput)
{
    const auto [low, high] = measureBandRms(8000.f);
    EXPECT_LT(low, 1e-3f);
    EXPECT_GT(high, 0.99f);
}

TEST(CrossoverLR4NodeTest, BandsRollOffAtTwentyFourDbPerOctave)
{
    const float lowAtTwoKhz = measureBandRms(2000.f).low;
    const float lowAtFourKhz = measureBandRms(4000.f).low;
    const float dropPerOctaveDb = 20.f * std::log10(lowAtTwoKhz / lowAtFourKhz);
    EXPECT_NEAR(dropPerOctaveDb, 24.f, 2.f);
}

}
