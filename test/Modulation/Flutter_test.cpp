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
    EXPECT_GE(output1, -1.1f);
    EXPECT_LE(output1, 1.1f);
    EXPECT_GE(output2, -1.1f);
    EXPECT_LE(output2, 1.1f);
}

TEST_F(FlutterLfoTest, ResetRestoresInitialPhase)
{
    FlutterLfo lfo(kSampleRate, 1.0f);
    const auto initialOutput = lfo.step(0.0f);

    for (int i = 0; i < 10; ++i)
    {
        lfo.step(1.0f);
    }
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


}