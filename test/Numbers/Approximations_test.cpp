#include "Numbers/Approximation.h"

#include <gtest/gtest.h>
#include <cmath>
#include <numbers>

namespace AbacDsp::Test
{

constexpr int rangeGranularity = 10000;

enum class FuncVariant
{
    HalfWave,
    FullWave,
    AbsFold
};

struct ApproxTestCase
{
    const char* m_name;
    float m_epsilon;
    bool m_is_sine;
    bool m_is_normalized_domain;
    FuncVariant m_variant;
    int m_degree;
};

class ApproximationTest : public ::testing::TestWithParam<ApproxTestCase>
{
};
static std::string suggestedEpsilon(float maxError)
{
    const int exp = static_cast<int>(std::floor(std::log10(maxError)));
    const int mantissa = static_cast<int>(std::ceil(maxError / std::pow(10.0f, exp)));
    return std::to_string(mantissa) + "e" + std::to_string(exp) + "f";
}
TEST_P(ApproximationTest, AccuracyCheck)
{
    float maxError = 0;
    const auto& tc = GetParam();

    for (int i = -rangeGranularity; i <= rangeGranularity; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(rangeGranularity);
        float x{}, expected{}, actual{};

        const float piHalf = std::numbers::pi_v<float> / 2.0f;
        const float pi = std::numbers::pi_v<float>;

        if (tc.m_is_normalized_domain)
        {
            x = t;
            const float scale = (tc.m_variant == FuncVariant::HalfWave) ? piHalf : pi;
            expected = tc.m_is_sine ? std::sin(x * scale) : std::cos(x * scale);

            switch (tc.m_variant)
            {
                case FuncVariant::HalfWave:
                    if (tc.m_is_sine)
                        actual = (tc.m_degree == 3) ? Approximation::remezSinP3<Approximation::DomainMinusOneToOne>(x)
                                                    : Approximation::remezSinP5<Approximation::DomainMinusOneToOne>(x);
                    else
                        actual = (tc.m_degree == 4) ? Approximation::remezCosP4<Approximation::DomainMinusOneToOne>(x)
                                                    : Approximation::remezCosP6<Approximation::DomainMinusOneToOne>(x);
                    break;
                case FuncVariant::FullWave:
                    if (tc.m_is_sine)
                    {
                        if (tc.m_degree == 5)
                            actual = Approximation::remezFullSinP5<Approximation::DomainMinusOneToOne>(x);
                        else if (tc.m_degree == 7)
                            actual = Approximation::remezFullSinP7<Approximation::DomainMinusOneToOne>(x);
                        else
                            actual = Approximation::remezFullSinP9<Approximation::DomainMinusOneToOne>(x);
                    }
                    else
                    {
                        if (tc.m_degree == 6)
                            actual = Approximation::remezFullCosP6<Approximation::DomainMinusOneToOne>(x);
                        else if (tc.m_degree == 8)
                            actual = Approximation::remezFullCosP8<Approximation::DomainMinusOneToOne>(x);
                        else
                            actual = Approximation::remezFullCosP10<Approximation::DomainMinusOneToOne>(x);
                    }
                    break;
                case FuncVariant::AbsFold:
                    actual = (tc.m_degree == 4)
                                 ? Approximation::remezFullCosAbsFoldP4<Approximation::DomainMinusOneToOne>(x)
                                 : Approximation::remezFullCosAbsFoldP6<Approximation::DomainMinusOneToOne>(x);
                    break;
            }
        }
        else
        {
            const float scale = (tc.m_variant == FuncVariant::HalfWave) ? piHalf : pi;
            x = t * scale;
            expected = tc.m_is_sine ? std::sin(x) : std::cos(x);

            switch (tc.m_variant)
            {
                case FuncVariant::HalfWave:
                    if (tc.m_is_sine)
                        actual = (tc.m_degree == 3)
                                     ? Approximation::remezSinP3<Approximation::DomainMinusPiHalfToPiHalf>(x)
                                     : Approximation::remezSinP5<Approximation::DomainMinusPiHalfToPiHalf>(x);
                    else
                        actual = (tc.m_degree == 4)
                                     ? Approximation::remezCosP4<Approximation::DomainMinusPiHalfToPiHalf>(x)
                                     : Approximation::remezCosP6<Approximation::DomainMinusPiHalfToPiHalf>(x);
                    break;
                case FuncVariant::FullWave:
                    if (tc.m_is_sine)
                    {
                        if (tc.m_degree == 5)
                            actual = Approximation::remezFullSinP5<Approximation::DomainMinusPiToPi>(x);
                        else if (tc.m_degree == 7)
                            actual = Approximation::remezFullSinP7<Approximation::DomainMinusPiToPi>(x);
                        else
                            actual = Approximation::remezFullSinP9<Approximation::DomainMinusPiToPi>(x);
                    }
                    else
                    {
                        if (tc.m_degree == 6)
                            actual = Approximation::remezFullCosP6<Approximation::DomainMinusPiToPi>(x);
                        else if (tc.m_degree == 8)
                            actual = Approximation::remezFullCosP8<Approximation::DomainMinusPiToPi>(x);
                        else
                            actual = Approximation::remezFullCosP10<Approximation::DomainMinusPiToPi>(x);
                    }
                    break;
                case FuncVariant::AbsFold:
                    actual = (tc.m_degree == 4)
                                 ? Approximation::remezFullCosAbsFoldP4<Approximation::DomainMinusPiToPi>(x)
                                 : Approximation::remezFullCosAbsFoldP6<Approximation::DomainMinusPiToPi>(x);
                    break;
            }
        }

        maxError = std::max(maxError, std::abs(actual - expected));
    }
    // std::cout << suggestedEpsilon(maxError) << std::endl;
    EXPECT_LT(maxError, tc.m_epsilon) << "Suggested epsilon: " << suggestedEpsilon(maxError);
    // EXPECT_LT(maxError, 0) << "Suggested epsilon: " << suggestedEpsilon(maxError);
}

INSTANTIATE_TEST_SUITE_P(
    All, ApproximationTest,
    ::testing::Values(
        // Half-wave, [-1, 1]
        ApproxTestCase{"SinP3_MinusOneToOne", 5e-3f, true, true, FuncVariant::HalfWave, 3},
        ApproxTestCase{"SinP5_MinusOneToOne", 7e-5f, true, true, FuncVariant::HalfWave, 5},
        ApproxTestCase{"CosP4_MinusOneToOne", 6e-4f, false, true, FuncVariant::HalfWave, 4},
        ApproxTestCase{"CosP6_MinusOneToOne", 7e-6f, false, true, FuncVariant::HalfWave, 6},
        // Half-wave, [-pi/2, pi/2]
        ApproxTestCase{"SinP3_MinusPiHalfToPiHalf", 5e-3f, true, false, FuncVariant::HalfWave, 3},
        ApproxTestCase{"SinP5_MinusPiHalfToPiHalf", 7e-5f, true, false, FuncVariant::HalfWave, 5},
        ApproxTestCase{"CosP4_MinusPiHalfToPiHalf", 6e-4f, false, false, FuncVariant::HalfWave, 4},
        ApproxTestCase{"CosP6_MinusPiHalfToPiHalf", 7e-6f, false, false, FuncVariant::HalfWave, 6},
        // Full-wave, [-1, 1]
        ApproxTestCase{"FullSinP5_MinusOneToOne", 7e-3f, true, true, FuncVariant::FullWave, 5},
        ApproxTestCase{"FullSinP7_MinusOneToOne", 3e-4f, true, true, FuncVariant::FullWave, 7},
        ApproxTestCase{"FullSinP9_MinusOneToOne", 7e-6f, true, true, FuncVariant::FullWave, 9},
        ApproxTestCase{"FullCosP6_MinusOneToOne", 2e-3f, false, true, FuncVariant::FullWave, 6},
        ApproxTestCase{"FullCosP8_MinusOneToOne", 5e-5f, false, true, FuncVariant::FullWave, 8},
        ApproxTestCase{"FullCosP10_MinusOneToOne", 2e-6f, false, true, FuncVariant::FullWave, 10},
        // Full-wave, [-pi, pi]
        ApproxTestCase{"FullSinP5_MinusPiToPi", 7e-3f, true, false, FuncVariant::FullWave, 5},
        ApproxTestCase{"FullSinP7_MinusPiToPi", 3e-4f, true, false, FuncVariant::FullWave, 7},
        ApproxTestCase{"FullSinP9_MinusPiToPi", 10e-6f, true, false, FuncVariant::FullWave, 9},
        ApproxTestCase{"FullCosP6_MinusPiToPi", 2e-3f, false, false, FuncVariant::FullWave, 6},
        ApproxTestCase{"FullCosP8_MinusPiToPi", 5e-5f, false, false, FuncVariant::FullWave, 8},
        ApproxTestCase{"FullCosP10_MinusPiToPi", 3e-5f, false, false, FuncVariant::FullWave, 10},
        // Abs-fold cosine, [-1, 1]
        ApproxTestCase{"FullCosAbsFoldP4_MinusOneToOne", 5e-3f, false, true, FuncVariant::AbsFold, 4},
        ApproxTestCase{"FullCosAbsFoldP6_MinusOneToOne", 7e-5f, false, true, FuncVariant::AbsFold, 6},
        // Abs-fold cosine, [-pi, pi]
        ApproxTestCase{"FullCosAbsFoldP4_MinusPiToPi", 5e-3f, false, false, FuncVariant::AbsFold, 4},
        ApproxTestCase{"FullCosAbsFoldP6_MinusPiToPi", 7e-5f, false, false, FuncVariant::AbsFold, 6}),
    [](const ::testing::TestParamInfo<ApproxTestCase>& info) { return info.param.m_name; });

} // namespace AbacDsp::Test
