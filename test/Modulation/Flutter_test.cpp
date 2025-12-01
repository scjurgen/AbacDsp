#include "Analysis/ZeroCrossings.h"


#include <gtest/gtest.h>
#include <numbers>
#include <cmath>
#include "Modulation/Flutter.h"

namespace AbacDsp::Test
{
class FlutterLfoTest : public ::testing::Test
{
  protected:
    static constexpr float kSampleRate = 48000.0f;

    void SetUp() override {}
};

TEST_F(FlutterLfoTest, PhaseInitializationAndWrapping)
{
    // Test phase wrapping during construction
    FlutterLfo lfo1(kSampleRate, 1.0f, 1.0f, 5.0f * std::numbers::pi_v<float>);
    FlutterLfo lfo2(kSampleRate, 1.0f, 1.0f, -3.0f * std::numbers::pi_v<float>);

    // Both should produce valid initial outputs (phases wrapped to [-π, π])
    float output1 = lfo1.step(0.0f); // Zero frequency to test just the initial phase
    float output2 = lfo2.step(0.0f);

    // Outputs should be valid amplitude-scaled cosine values
    EXPECT_GE(output1, -1.0f);
    EXPECT_LE(output1, 1.0f);
    EXPECT_GE(output2, -1.0f);
    EXPECT_LE(output2, 1.0f);
}

TEST_F(FlutterLfoTest, ResetRestoresInitialPhase)
{
    constexpr float phaseOffset = 1.f / 3.f;
    FlutterLfo lfo(kSampleRate, 1.0f, 1.0f, phaseOffset);

    const auto initialOutput = lfo.step(0.0f);

    // Step several times to change internal phase
    for (int i = 0; i < 10; ++i)
    {
        lfo.step(1.0f);
    }
    // Reset and capture output again
    lfo.reset();
    const auto resetOutput = lfo.step(0.0f);
    EXPECT_FLOAT_EQ(initialOutput, resetOutput);
}

TEST_F(FlutterLfoTest, AmplitudeScaling)
{
    static constexpr float kTolerance = 1e-3f;
    constexpr float amplitude = 0.5f;
    FlutterLfo lfo(kSampleRate, 10.0f, amplitude, 0.0f);

    float minOutput = lfo.step(10.f);
    float maxOutput = minOutput;
    for (int i = 0; i < 4800; ++i)
    {
        float output = lfo.step(10.0f);
        minOutput = std::min(minOutput, output);
        maxOutput = std::max(maxOutput, output);
    }
    EXPECT_NEAR(maxOutput, amplitude, kTolerance);
    EXPECT_NEAR(minOutput, -amplitude, kTolerance);
}

TEST_F(FlutterLfoTest, FrequencyScaling)
{
    FlutterLfo lfo1x(kSampleRate, 1.0f, 1.0f, 0.0f);
    FlutterLfo lfo2x(kSampleRate, 2.0f, 1.0f, 0.0f);

    constexpr float baseFreq = 10.0f;
    constexpr int cycleSamples = 100000;

    // Step both LFOs and track phase relationship
    std::vector<float> outputs1x, outputs2x;
    for (int i = 0; i < cycleSamples; ++i)
    {
        outputs1x.push_back(lfo1x.step(baseFreq));
        outputs2x.push_back(lfo2x.step(baseFreq));
    }
    auto periodLen1x =
        periodLengthByZeroCrossingAverage(outputs1x.data(), outputs1x.size(), [](const float in) { return in; });
    auto periodLen2x =
        periodLengthByZeroCrossingAverage(outputs2x.data(), outputs2x.size(), [](const float in) { return in; });
    EXPECT_NEAR(periodLen1x, 4800, 1);
    EXPECT_NEAR(periodLen2x, 2400, 1);
}


TEST_F(FlutterLfoTest, QcosApproximationAccuracy)
{
    FlutterLfo lfo(kSampleRate, 1.0f, 1.0f, 0.0f);

    // Test qcos accuracy by comparing with std::cos over a range
    constexpr int testPoints = 100;
    constexpr float maxError = 0.01f; // Allow 1% error for LFO use

    for (int i = 0; i < testPoints; ++i)
    {
        const auto phase = -1 + (2.0f * static_cast<float>(i)) / testPoints;

        // Get qcos result by stepping the LFO to a known phase
        FlutterLfo testLfo(kSampleRate, 1.0f, 1.0f, phase);
        const auto qcosResult = testLfo.step(0.0f); // Zero frequency preserves phase

        const auto stdCosResult = std::cos(phase * std::numbers::pi_v<float>);
        const auto error = std::abs(qcosResult - stdCosResult);

        EXPECT_LT(error, maxError) << "qcos error too large at phase " << phase;
    }
}

TEST_F(FlutterLfoTest, DeterministicBehavior)
{
    constexpr float freq = 2.0f;
    constexpr int steps = 50;

    FlutterLfo lfo1(kSampleRate, 1.0f, 1.0f, std::numbers::pi_v<float> / 4.0f);
    FlutterLfo lfo2(kSampleRate, 1.0f, 1.0f, std::numbers::pi_v<float> / 4.0f);

    // Both LFOs should produce identical sequences
    for (int i = 0; i < steps; ++i)
    {
        const auto output1 = lfo1.step(freq);
        const auto output2 = lfo2.step(freq);

        EXPECT_FLOAT_EQ(output1, output2) << "Outputs differ at step " << i;
    }
}
}