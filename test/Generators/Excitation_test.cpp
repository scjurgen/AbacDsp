#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Generators/Excitation.h"

namespace AbacDsp::Test
{

TEST(Excitation, getInterpolatedValueBlendsSineAndNoise)
{
    Excitation excitation(256);
    EXPECT_EQ(excitation.getPatternLength(), 256u);
    EXPECT_EQ(excitation.getSinePattern().size(), 257u);
    EXPECT_EQ(excitation.getNoisePattern().size(), Excitation::NumNoise);

    excitation.setNoise(0.5f);
    EXPECT_FLOAT_EQ(excitation.getNoiseFactor(), 0.5f);

    for (float pos = 0.f; pos < 250.f; pos += 1.5f)
    {
        const float v = excitation.getInterpolatedValue(pos);
        EXPECT_TRUE(std::isfinite(v));
    }
}

TEST(Excitation, regenerateNoiseKeepsPatternSize)
{
    Excitation excitation;
    excitation.regenerateNoise();
    EXPECT_EQ(excitation.getNoisePattern().size(), Excitation::NumNoise);
}

TEST(WindowFunctions, hannWindowStartsAndEndsNearZero)
{
    const auto window = WindowFunctions::hannWindow<float>(64);
    ASSERT_EQ(window.size(), 64u);
    EXPECT_NEAR(window.front(), 0.f, 1E-6f);
    EXPECT_GT(window[32], 0.9f);
}

TEST(WindowFunctions, blackmanHarrisWindowScalesInPlace)
{
    std::vector<float> data(64, 1.f);
    WindowFunctions::blackmanHarrisWindow<float>(data);
    EXPECT_NEAR(data.front(), 0.35875f - 0.48829f + 0.14128f + 0.01168f, 1E-4f);
}

}
