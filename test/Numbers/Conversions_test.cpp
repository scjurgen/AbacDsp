
#include "gtest/gtest.h"

#include "Numbers/Conversions.h"


TEST(DspAudioAlgorithmsTests, getPanFactorNormalized)
{
    constexpr float epsilon = 1.0E-6f;
    float left, right;
    for (int i = -10; i <= 10; ++i)
    {
        auto f = static_cast<float>(i) / 10.f;
        AbacadDsp::getPanFactorNormalized(f, left, right);
        EXPECT_NEAR(left, std::cos((f + 1) / 2 * 3.1415926535 / 2.f), epsilon);
        EXPECT_NEAR(right, std::sin((f + 1) / 2 * 3.1415926535 / 2.f), epsilon);
    }
}

TEST(DspAudioAlgorithmsTests, getFastPanFactorNormalized)
{
    constexpr float epsilon = 2.0E-3f;
    float left, right;
    for (int i = -10; i <= 10; ++i)
    {
        auto f = static_cast<float>(i) / 10.f;
        AbacadDsp::getFastPanFactorNormalized(f, left, right);
        EXPECT_NEAR(left, std::cos((f + 1) / 2 * 3.1415926535 / 2.f), epsilon);
        EXPECT_NEAR(right, std::sin((f + 1) / 2 * 3.1415926535 / 2.f), epsilon);
    }
}
