#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Biquad.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(BiquadNodeTest, ModeFromConfigParsesKnownNamesAndDefaultsToLowPass)
{
    EXPECT_EQ(biquadModeFromConfig("lowpass"), BiquadMode::LowPass);
    EXPECT_EQ(biquadModeFromConfig("highpass"), BiquadMode::HighPass);
    EXPECT_EQ(biquadModeFromConfig("notch"), BiquadMode::Notch);
    EXPECT_EQ(biquadModeFromConfig("peak"), BiquadMode::Peak);
    EXPECT_EQ(biquadModeFromConfig("unknown"), BiquadMode::LowPass);
}

TEST(BiquadNodeTest, LowPassPassesDcAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    Biquad filter{kSampleRate, BiquadMode::LowPass};
    filter.setParameter(0, 1000.f);

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 5000; ++i)
    {
        filter.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 1.f, 1e-2f);
}

TEST(BiquadNodeTest, HighPassBlocksDcAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    Biquad filter{kSampleRate, BiquadMode::HighPass};
    filter.setParameter(0, 1000.f);

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 5000; ++i)
    {
        filter.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 0.f, 1e-2f);
}

}
