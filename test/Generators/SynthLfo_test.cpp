#include <cmath>
#include <tuple>

#include "gtest/gtest.h"

#include "Generators/SynthLfo.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.0f;
}

TEST(SynthLfo, sineCompletesOneCycleAtSetFrequency)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Sine);
    lfo.setFrequency(100.0f);

    const auto samplesPerCycle = static_cast<int>(kSampleRate / 100.0f);
    float maxAbs = 0.0f;
    for (int i = 0; i < samplesPerCycle; ++i)
    {
        maxAbs = std::max(maxAbs, std::abs(lfo.step()));
    }
    EXPECT_NEAR(maxAbs, 1.0f, 0.05f);
}

TEST(SynthLfo, triangleStaysWithinUnitRange)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Triangle);
    lfo.setFrequency(10.0f);
    for (int i = 0; i < 20000; ++i)
    {
        const auto v = lfo.step();
        ASSERT_GE(v, -1.0001f);
        ASSERT_LE(v, 1.0001f);
    }
}

TEST(SynthLfo, sawRampsMonotonicallyWithinOneCycle)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Saw);
    lfo.setFrequency(50.0f);

    float previous = lfo.step();
    const auto samplesPerCycle = static_cast<int>(kSampleRate / 50.0f);
    for (int i = 1; i < samplesPerCycle - 1; ++i)
    {
        const auto v = lfo.step();
        EXPECT_GT(v, previous);
        previous = v;
    }
}

TEST(SynthLfo, squareAlternatesBetweenPlusAndMinusOne)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Square);
    lfo.setFrequency(100.0f);
    bool sawHigh = false;
    bool sawLow = false;
    for (int i = 0; i < 1000; ++i)
    {
        const auto v = lfo.step();
        if (v > 0.9f)
        {
            sawHigh = true;
        }
        if (v < -0.9f)
        {
            sawLow = true;
        }
    }
    EXPECT_TRUE(sawHigh);
    EXPECT_TRUE(sawLow);
}

TEST(SynthLfo, zeroFrequencyStopsAdvancingAndHoldsLastValue)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Sine);
    lfo.setFrequency(10.0f);
    for (int i = 0; i < 100; ++i)
    {
        std::ignore = lfo.step();
    }
    lfo.setFrequency(0.0f);
    const auto held = lfo.step();
    for (int i = 0; i < 100; ++i)
    {
        EXPECT_FLOAT_EQ(lfo.step(), held);
    }
}

TEST(SynthLfo, stopResetsPhaseAndValue)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Sine);
    lfo.setFrequency(10.0f);
    for (int i = 0; i < 100; ++i)
    {
        std::ignore = lfo.step();
    }
    lfo.stop();
    EXPECT_FLOAT_EQ(lfo.step(), 0.0f);
}

TEST(SynthLfo, sampleHoldNoiseHoldsBetweenUpdatesRatherThanChangingEverySample)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::SampleHoldNoise);
    lfo.setFrequency(1.0f);

    int changes = 0;
    float previous = lfo.step();
    for (int i = 0; i < 100000; ++i)
    {
        const auto v = lfo.step();
        if (v != previous)
        {
            ++changes;
        }
        previous = v;
    }
    EXPECT_LT(changes, 100);
    EXPECT_GT(changes, 0);
}

TEST(SynthLfo, brownNoiseOutputStaysFinite)
{
    LfoGenerators lfo{kSampleRate};
    lfo.setWaveForm(LfoType::BrownNoise);
    lfo.setFrequency(2.0f);
    for (int i = 0; i < 20000; ++i)
    {
        ASSERT_TRUE(std::isfinite(lfo.step()));
    }
}

TEST(SynthLfo, smoothedValueDoesNotJumpAsSharplyAsRawStep)
{
    SynthLfo lfo{kSampleRate};
    lfo.setWaveForm(LfoType::Square);
    lfo.setSpeed(200.0f);

    float maxDelta = 0.0f;
    float previous = lfo.getValue();
    for (int i = 0; i < 2000; ++i)
    {
        const auto v = lfo.getValue();
        maxDelta = std::max(maxDelta, std::abs(v - previous));
        previous = v;
    }
    EXPECT_LT(maxDelta, 2.0f);
}

}
