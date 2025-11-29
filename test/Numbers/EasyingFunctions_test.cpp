#include <gtest/gtest.h>
#include <array>
#include "Numbers/EasyingFunctions.h"

namespace AbacDsp
{

TEST(EasyingFunctionsTest, SmoothStep2BoundaryValues)
{
    EXPECT_DOUBLE_EQ(Easying::smoothStep2(0.0, 1.0), 1.0);
    EXPECT_DOUBLE_EQ(Easying::smoothStep2(1.0, 1.0), 1.0);
}

TEST(EasyingFunctionsTest, SmoothStep2MidPoint)
{
    const double mid = Easying::smoothStep2(0.5, 1.0);
    EXPECT_GT(mid, 0.0);
    EXPECT_LE(mid, 2.0);
}

TEST(EasyingFunctionsTest, SmoothStep2Symmetry)
{
    const double c = 0.5;
    const double v025 = Easying::smoothStep2(0.25, c);
    const double v075 = Easying::smoothStep2(0.75, c);
    EXPECT_DOUBLE_EQ(v025, v075);
}

TEST(EasyingFunctionsTest, SmoothStep2StrictValues)
{
    constexpr std::array<double, 11> expected{1.0000000000000000, 1.3599999999999999, 1.6400000000000001,
                                              1.8400000000000001, 1.9600000000000000, 2.0000000000000000,
                                              1.9600000000000000, 1.8399999999999999, 1.6399999999999997,
                                              1.3599999999999999, 1.0000000000000000};

    constexpr double c = 1.0;
    for (size_t i = 0; i < 11; ++i)
    {
        const double x = i * 0.1;
        EXPECT_DOUBLE_EQ(Easying::smoothStep2(x, c), expected[i]) << "Mismatch at x=" << x;
    }
}

TEST(EasyingFunctionsTest, SmoothStep4BoundaryValues)
{
    EXPECT_DOUBLE_EQ(Easying::smoothStep4(0.0, 1.0), 1.0);
    EXPECT_DOUBLE_EQ(Easying::smoothStep4(1.0, 1.0), 1.0);
}

TEST(EasyingFunctionsTest, SmoothStep4MidPoint)
{
    const double mid = Easying::smoothStep4(0.5, 1.0);
    EXPECT_GT(mid, 0.0);
    EXPECT_LE(mid, 2.0);
}

TEST(EasyingFunctionsTest, SmoothStep4Symmetry)
{
    const double c = 0.5;
    const double v025 = Easying::smoothStep4(0.25, c);
    const double v075 = Easying::smoothStep4(0.75, c);
    EXPECT_DOUBLE_EQ(v025, v075);
}

TEST(EasyingFunctionsTest, SmoothStep4BellCurve)
{
    const double c = 0.5;
    const double v01 = Easying::smoothStep4(0.1, c);
    const double v025 = Easying::smoothStep4(0.25, c);
    const double v05 = Easying::smoothStep4(0.5, c);
    const double v075 = Easying::smoothStep4(0.75, c);
    const double v09 = Easying::smoothStep4(0.9, c);

    EXPECT_LT(v01, v025);
    EXPECT_LT(v025, v05);
    EXPECT_GT(v05, v075);
    EXPECT_GT(v075, v09);
}

TEST(EasyingFunctionsTest, SmoothStep4StrictValues)
{
    constexpr std::array<double, 11> expected{1.0000000000000000, 1.1295999999999999, 1.4096000000000002,
                                              1.7056000000000000, 1.9216000000000000, 2.0000000000000000,
                                              1.9216000000000000, 1.7055999999999998, 1.4095999999999997,
                                              1.1295999999999999, 1.0000000000000000};

    constexpr double c = 1.0;
    for (size_t i = 0; i < 11; ++i)
    {
        const double x = i * 0.1;
        EXPECT_DOUBLE_EQ(Easying::smoothStep4(x, c), expected[i]) << "Mismatch at x=" << x;
    }
}

TEST(EasyingFunctionsTest, SmoothStep4IntegralBoundaryValues)
{
    EXPECT_DOUBLE_EQ(Easying::smoothStep4Integral(0.0, 1.0), 0.0);
}

TEST(EasyingFunctionsTest, SmoothStep4IntegralMonotonicIncrease)
{
    const double c = 0.5;
    const double i0 = Easying::smoothStep4Integral(0.0, c);
    const double i05 = Easying::smoothStep4Integral(0.5, c);
    const double i1 = Easying::smoothStep4Integral(1.0, c);
    EXPECT_LT(i0, i05);
    EXPECT_LT(i05, i1);
}

TEST(EasyingFunctionsTest, SmoothStep4IntegralStrictValues)
{
    constexpr std::array<double, 11> expected{0.0000000000000000, 0.1045653333333333, 0.2308906666666667,
                                              0.3869760000000001, 0.5693013333333333, 0.7666666666666666,
                                              0.9640320000000000, 1.1463573333333332, 1.3024426666666660,
                                              1.4287679999999998, 1.5333333333333332};

    constexpr double c = 1.0;
    for (size_t i = 0; i < 11; ++i)
    {
        const double x = i * 0.1;
        EXPECT_DOUBLE_EQ(Easying::smoothStep4Integral(x, c), expected[i]) << "Mismatch at x=" << x;
    }
}

}
