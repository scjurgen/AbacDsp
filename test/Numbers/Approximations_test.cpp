#include "Numbers/Approximation.h"

#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

namespace AbacDsp::Test
{

constexpr int rangeGranularity = 10000;

struct ApproxTestCase
{
    const char* m_name;
    float m_epsilon;
    bool m_is_sine;
    bool m_is_normalized_domain;
    int m_degree;
};

class ApproximationTest : public ::testing::TestWithParam<ApproxTestCase>
{
};

TEST_P(ApproximationTest, AccuracyCheck)
{
    float maxError = 0;
    const auto& test_case = GetParam();

    for (int i = -rangeGranularity; i <= rangeGranularity; ++i)
    {
        float x, expected, actual;

        if (test_case.m_is_normalized_domain)
        {
            x = static_cast<float>(i) / static_cast<float>(rangeGranularity);

            if (test_case.m_is_sine)
            {
                expected = std::sin(x * std::numbers::pi_v<float> / 2.0f);

                if (test_case.m_degree == 3)
                {
                    actual = Approximation::remezSinP3<Approximation::DomainMinusOneToOne>(x);
                }
                else
                {
                    actual = Approximation::remezSinP5<Approximation::DomainMinusOneToOne>(x);
                }
            }
            else
            {
                expected = std::cos(x * std::numbers::pi_v<float> / 2.0f);

                if (test_case.m_degree == 4)
                {
                    actual = Approximation::remezCosP4<Approximation::DomainMinusOneToOne>(x);
                }
                else
                {
                    actual = Approximation::remezCosP6<Approximation::DomainMinusOneToOne>(x);
                }
            }
        }
        else
        {
            const float normalized = static_cast<float>(i) / static_cast<float>(rangeGranularity);
            x = normalized * std::numbers::pi_v<float> / 2.0f;

            if (test_case.m_is_sine)
            {
                expected = std::sin(x);

                if (test_case.m_degree == 3)
                {
                    actual = Approximation::remezSinP3<Approximation::DomainMinusPiHalfToPiHalf>(x);
                }
                else
                {
                    actual = Approximation::remezSinP5<Approximation::DomainMinusPiHalfToPiHalf>(x);
                }
            }
            else
            {
                expected = std::cos(x);

                if (test_case.m_degree == 4)
                {
                    actual = Approximation::remezCosP4<Approximation::DomainMinusPiHalfToPiHalf>(x);
                }
                else
                {
                    actual = Approximation::remezCosP6<Approximation::DomainMinusPiHalfToPiHalf>(x);
                }
            }
        }
        const float dt = std::abs(actual - expected);
        maxError = std::max(maxError, dt);
    }
    EXPECT_LT(maxError, test_case.m_epsilon);
}

INSTANTIATE_TEST_SUITE_P(All, ApproximationTest,
                         ::testing::Values(ApproxTestCase{"SinP3_MinusOneToOne", 4.5e-3f, true, true, 3},
                                           ApproxTestCase{"SinP5_MinusOneToOne", 7e-5f, true, true, 5},
                                           ApproxTestCase{"SinP3_MinusPiHalfToPiHalf", 4.5e-3f, true, false, 3},
                                           ApproxTestCase{"SinP5_MinusPiHalfToPiHalf", 7e-5f, true, false, 5},
                                           ApproxTestCase{"CosP4_MinusOneToOne", 6e-4f, false, true, 4},
                                           ApproxTestCase{"CosP6_MinusOneToOne", 7e-6f, false, true, 6},
                                           ApproxTestCase{"CosP4_MinusPiHalfToPiHalf", 6e-4f, false, false, 4},
                                           ApproxTestCase{"CosP6_MinusPiHalfToPiHalf", 7e-6f, false, false, 6}),
                         [](const ::testing::TestParamInfo<ApproxTestCase>& info) { return info.param.m_name; });

}
