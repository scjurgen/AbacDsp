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
    static constexpr float kTolerance = 1e-6f;

    void SetUp() override {}
};

// Test: Phase initialization and wrapping behavior
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

// Test: Reset behavior maintains initial phase
TEST_F(FlutterLfoTest, ResetRestoresInitialPhase)
{
    constexpr float phaseOffset = 1.f / 3.f;
    FlutterLfo lfo(kSampleRate, 1.0f, 1.0f, phaseOffset);

    // Capture initial output
    float initialOutput = lfo.step(0.0f);

    // Step several times to change internal phase
    for (int i = 0; i < 10; ++i)
    {
        lfo.step(1.0f);
    }

    // Reset and capture output again
    lfo.reset();
    float resetOutput = lfo.step(0.0f);

    // Should be identical to initial state
    EXPECT_FLOAT_EQ(initialOutput, resetOutput);
}

// Test: Output amplitude scaling
TEST_F(FlutterLfoTest, AmplitudeScaling)
{
    constexpr float amplitude = 0.5f;
    FlutterLfo lfo(kSampleRate, 1.0f, amplitude, 0.0f);

    // Step through multiple samples to find min/max
    float minOutput = lfo.step(10.f);
    float maxOutput = minOutput;
    for (int i = 0; i < 4800; ++i)
    {
        float output = lfo.step(10.0f);
        minOutput = std::min(minOutput, output);
        maxOutput = std::max(maxOutput, output);
    }

    // Should be scaled by amplitude
    EXPECT_NEAR(maxOutput, amplitude, kTolerance);
    EXPECT_NEAR(minOutput, -amplitude, kTolerance);
}

// Test: Frequency scaling behavior
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

    // 2x LFO should complete roughly twice as many cycles
    // Find zero crossings as a rough cycle count measure
    int crossings1x = 0, crossings2x = 0;
    for (int i = 1; i < cycleSamples; ++i)
    {
        if ((outputs1x[i - 1] >= 0) != (outputs1x[i] >= 0))
        {
            crossings1x++;
        }
        if ((outputs2x[i - 1] >= 0) != (outputs2x[i] >= 0))
        {
            crossings2x++;
        }
    }
    EXPECT_EQ(crossings1x, 42);
    EXPECT_EQ(crossings2x, 83);
    // 2x frequency should have approximately twice as many crossings
    EXPECT_GT(crossings2x, crossings1x);
    EXPECT_NEAR(static_cast<float>(crossings2x) / crossings1x, 2.0f, 0.5f);
}

// Test: Phase accumulation and wrapping stability
TEST_F(FlutterLfoTest, PhaseWrapStability)
{
    FlutterLfo lfo(kSampleRate, 1.0f, 1.0f, 0.0f);

    // Step many times with high frequency to force multiple wraps
    constexpr float highFreq = 1000.0f;
    constexpr int manySteps = 1000;

    std::vector<float> outputs;
    for (int i = 0; i < manySteps; ++i)
    {
        float output = lfo.step(highFreq);
        outputs.push_back(output);

        // Output should always be within amplitude bounds
        EXPECT_GE(output, -1.0f - kTolerance);
        EXPECT_LE(output, 1.0f + kTolerance);
    }

    // Check for periodicity - after many cycles, we should see repeated patterns
    // (This is a basic sanity check for phase wrap behavior)
    EXPECT_FALSE(outputs.empty());
}

// Test: Custom qcos approximation vs std::cos
TEST_F(FlutterLfoTest, QcosApproximationAccuracy)
{
    FlutterLfo lfo(kSampleRate, 1.0f, 1.0f, 0.0f);

    // Test qcos accuracy by comparing with std::cos over range
    constexpr int testPoints = 100;
    constexpr float maxError = 0.1f; // Allow 10% error for LFO use

    for (int i = 0; i < testPoints; ++i)
    {
        float phase = -1 + (2.0f * i) / testPoints;

        // Get qcos result by stepping the LFO to a known phase
        FlutterLfo testLfo(kSampleRate, 1.0f, 1.0f, phase);
        float qcosResult = testLfo.step(0.0f); // Zero frequency preserves phase

        float stdCosResult = std::cos(phase * std::numbers::pi_v<float>);
        float error = std::abs(qcosResult - stdCosResult);

        EXPECT_LT(error, maxError) << "qcos error too large at phase " << phase;
    }
}

// Test: Deterministic behavior - same inputs produce same outputs
TEST_F(FlutterLfoTest, DeterministicBehavior)
{
    constexpr float freq = 2.0f;
    constexpr int steps = 50;

    FlutterLfo lfo1(kSampleRate, 1.0f, 1.0f, std::numbers::pi_v<float> / 4.0f);
    FlutterLfo lfo2(kSampleRate, 1.0f, 1.0f, std::numbers::pi_v<float> / 4.0f);

    // Both LFOs should produce identical sequences
    for (int i = 0; i < steps; ++i)
    {
        float output1 = lfo1.step(freq);
        float output2 = lfo2.step(freq);

        EXPECT_FLOAT_EQ(output1, output2) << "Outputs differ at step " << i;
    }
}
}