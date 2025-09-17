#include "gtest/gtest.h"

#include "Numbers/Interpolation.h"

#include <array>
#include <tuple>
#include <string>

namespace AbacDsp::Test
{

namespace
{

constexpr float epsilon{1E-5f};

constexpr std::array dataSet{0.f, 1.f, -0.25f, 0.5f, -0.75f, 0.f};
constexpr std::array ratios{0.f, 0.25f, 0.5f, 0.75f, 1.f};

using InterpolationFunc = float (*)(const float*, float);

struct InterpolationTestParam
{
    InterpolationFunc m_func;
    std::string m_name;
    std::array<float, 5> m_expected;
};

}

class InterpolationTest : public ::testing::TestWithParam<InterpolationTestParam>
{
};

TEST_P(InterpolationTest, TestAllMethods)
{
    const auto& param = GetParam();
    for (size_t i = 0; i < ratios.size(); ++i)
    {
        const auto result = param.m_func(dataSet.data(), ratios[i]);
        EXPECT_NEAR(result, param.m_expected[i], epsilon);
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllInterpolationMethods, InterpolationTest,
    ::testing::Values(InterpolationTestParam{Interpolation::zeroOrderHold, "zeroOrderHold", {0.f, 0.f, 0.f, 0.f, 0.f}},
                      InterpolationTestParam{Interpolation::linearPt2, "linearPt2", {0.f, 0.25f, 0.5f, 0.75f, 1.f}},

                      InterpolationTestParam{Interpolation::bspline43x,
                                             "bspline43x",
                                             {0.625f, 0.534505f, 0.369792f, 0.197266f, 0.0833333f}},
                      InterpolationTestParam{Interpolation::bspline43z,
                                             "bspline43z",
                                             {0.625f, 0.534505f, 0.369792f, 0.197266f, 0.0833333f}},
                      InterpolationTestParam{Interpolation::lagrange43x,
                                             "lagrange43x",
                                             {1.f, 0.732422f, 0.390625f, 0.0410157f, -0.25f}},
                      InterpolationTestParam{Interpolation::lagrange43z,
                                             "lagrange43z",
                                             {1.f, 0.732422f, 0.390625f, 0.0410157f, -0.25f}},
                      InterpolationTestParam{Interpolation::hermite43x,
                                             "hermite43x",
                                             {1.f, 0.798828f, 0.390625f, -0.0253906f, -0.25f}},
                      InterpolationTestParam{Interpolation::hermite43z,
                                             "hermite_43z",
                                             {1.f, 0.798828f, 0.390625f, -0.0253906f, -0.25f}},
                      InterpolationTestParam{Interpolation::bspline65x,
                                             "bspline65x",
                                             {0.18125f, 0.14f, 0.124935f, 0.109503f, 0.0666667f}},
                      InterpolationTestParam{Interpolation::lagrange65x,
                                             "lagrange65x",
                                             {-0.25f, -0.109772f, 0.12207f, 0.355499f, 0.5f}},
                      InterpolationTestParam{Interpolation::lagrange65z,
                                             "lagrange65z",
                                             {-0.25f, -0.109772f, 0.12207f, 0.355499f, 0.5f}},
                      InterpolationTestParam{Interpolation::hermite63x,
                                             "hermite63x",
                                             {-0.25f, -0.15918f, 0.122396f, 0.405273f, 0.5f}},
                      InterpolationTestParam{Interpolation::hermite63z,
                                             "hermite63z",
                                             {-0.25f, -0.15918f, 0.122396f, 0.405273f, 0.5f}},
                      InterpolationTestParam{Interpolation::hermite65x,
                                             "hermite65x",
                                             {-0.25f, -0.165314f, 0.12207f, 0.411041f, 0.5f}},
                      InterpolationTestParam{Interpolation::hermite65z,
                                             "hermite65z",
                                             {-0.25f, -0.165314f, 0.12207f, 0.411041f, 0.5f}}));


TEST(InterpolationProperties, ConstantInputPreservation)
{
    std::array constantData{5.f, 5.f, 5.f, 5.f, 5.f, 5.f};

    for (const auto ratio : ratios)
    {
        EXPECT_NEAR(Interpolation::zeroOrderHold(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::linearPt2(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::bspline43x(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::lagrange43x(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::hermite43x(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::bspline65x(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::lagrange65x(constantData.data(), ratio), 5.f, epsilon);
        EXPECT_NEAR(Interpolation::hermite65x(constantData.data(), ratio), 5.f, epsilon);
    }
}

}