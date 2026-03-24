#include "Numbers/MultichannelInterpolation.h"

#include <array>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

namespace AbacDsp::Test
{

template <size_t N>
bool arraysNear(const float* a, const float* b, const float tol = 1e-6f)
{
    for (size_t i = 0; i < N; ++i)
    {
        if (std::fabs(a[i] - b[i]) > tol)
        {
            return false;
        }
    }
    return true;
}

template <typename T>
class MultichannelInterpolationTest : public ::testing::Test
{
  public:
    static constexpr size_t NumChannels = T::value;
};

using MyTypes = ::testing::Types<std::integral_constant<size_t, 1>, std::integral_constant<size_t, 2>,
                                 std::integral_constant<size_t, 4>>;

TYPED_TEST_SUITE(MultichannelInterpolationTest, MyTypes);

TYPED_TEST(MultichannelInterpolationTest, LinearMidpointIsAverage)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[2 * NumChannels];
    float expected[NumChannels], out[NumChannels];

    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        interleaved[NumChannels * 0 + ch] = 1.0f * static_cast<float>(ch + 1);
        interleaved[NumChannels * 1 + ch] = 3.0f * static_cast<float>(ch + 1);
        expected[ch] = 2.0f * static_cast<float>(ch + 1);
    }
    MultichannelInterpolation<NumChannels>::linearPt2(interleaved, out, 0.5f);
    ASSERT_TRUE(arraysNear<NumChannels>(out, expected));
}

TYPED_TEST(MultichannelInterpolationTest, LinearAtZeroReturnsFirstSample)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[2 * NumChannels];
    float expected[NumChannels], out[NumChannels];

    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        interleaved[NumChannels * 0 + ch] = 10.0f * static_cast<float>(ch + 1);
        interleaved[NumChannels * 1 + ch] = 20.0f * static_cast<float>(ch + 1);
        expected[ch] = interleaved[NumChannels * 0 + ch];
    }
    MultichannelInterpolation<NumChannels>::linearPt2(interleaved, out, 0.0f);
    ASSERT_TRUE(arraysNear<NumChannels>(out, expected));
}

TYPED_TEST(MultichannelInterpolationTest, LinearAtOneReturnsSecondSample)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[2 * NumChannels];
    float expected[NumChannels], out[NumChannels];

    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        interleaved[NumChannels * 0 + ch] = 10.0f * static_cast<float>(ch + 1);
        interleaved[NumChannels * 1 + ch] = 20.0f * static_cast<float>(ch + 1);
        expected[ch] = interleaved[NumChannels * 1 + ch];
    }
    MultichannelInterpolation<NumChannels>::linearPt2(interleaved, out, 1.0f);
    ASSERT_TRUE(arraysNear<NumChannels>(out, expected));
}

// Catmull-Rom passes through y[1] at x=0, y[2] at x=1, and is exact on linear data
TYPED_TEST(MultichannelInterpolationTest, CatmullRomMatchesLinearForLinearData)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[4 * NumChannels];
    float linear[NumChannels], catmull[NumChannels];

    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        for (size_t f = 0; f < 4; ++f)
        {
            interleaved[NumChannels * f + ch] = static_cast<float>(f) * static_cast<float>(ch + 1);
        }
    }

    MultichannelInterpolation<NumChannels>::linearPt2(&interleaved[NumChannels * 1], linear, 0.5f);
    MultichannelInterpolation<NumChannels>::catmullRom(interleaved, catmull, 0.5f);
    ASSERT_TRUE(arraysNear<NumChannels>(catmull, linear, 1e-5f));
}

TYPED_TEST(MultichannelInterpolationTest, CatmullRomAtZeroReturnsY1)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[4 * NumChannels];
    float expected[NumChannels], out[NumChannels];

    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        for (size_t f = 0; f < 4; ++f)
        {
            interleaved[NumChannels * f + ch] = static_cast<float>(f + 1) * static_cast<float>(ch + 1);
        }
        expected[ch] = interleaved[NumChannels * 1 + ch]; // y[1]
    }
    MultichannelInterpolation<NumChannels>::catmullRom(interleaved, out, 0.0f);
    ASSERT_TRUE(arraysNear<NumChannels>(out, expected, 1e-5f));
}

TYPED_TEST(MultichannelInterpolationTest, CatmullRomAtOneReturnsY2)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[4 * NumChannels];
    float expected[NumChannels], out[NumChannels];

    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        for (size_t f = 0; f < 4; ++f)
        {
            interleaved[NumChannels * f + ch] = static_cast<float>(f + 1) * static_cast<float>(ch + 1);
        }
        expected[ch] = interleaved[NumChannels * 2 + ch]; // y[2]
    }
    MultichannelInterpolation<NumChannels>::catmullRom(interleaved, out, 1.0f);
    ASSERT_TRUE(arraysNear<NumChannels>(out, expected, 1e-5f));
}

TYPED_TEST(MultichannelInterpolationTest, VerifyInterpolatedValues)
{
    constexpr size_t NumChannels = TypeParam::value;
    float interleaved[6 * NumChannels];
    float out[NumChannels];

    {
        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            constexpr float seq6[6] = {0.0f, 1.0f, -1.0f, 1.0f, -1.0f, 0.0f};
            for (size_t f = 0; f < 6; ++f)
                interleaved[NumChannels * f + ch] = seq6[f];
        }
    }

    // Catmull-Rom, 4-point (uses [0..3])
    MultichannelInterpolation<NumChannels>::catmullRom(interleaved, out, 0.1f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], 0.903500000000f, 1e-5f) << "catmull x=0.1 ch=" << ch;
    }

    MultichannelInterpolation<NumChannels>::catmullRom(interleaved, out, 0.4f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], 0.224000000000f, 1e-5f) << "catmull x=0.4 ch=" << ch;
    }

    MultichannelInterpolation<NumChannels>::catmullRom(interleaved, out, 0.8f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], -0.808000000000f, 1e-5f) << "catmull x=0.8 ch=" << ch;
    }

    // Optimal 4-point
    MultichannelInterpolation<NumChannels>::optimal_43(interleaved, out, 0.1f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], -0.049670721946f, 1e-5f) << "opt43 x=0.1 ch=" << ch;
    }

    MultichannelInterpolation<NumChannels>::optimal_43(interleaved, out, 0.4f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], -0.235583672662f, 1e-5f) << "opt43 x=0.4 ch=" << ch;
    }

    MultichannelInterpolation<NumChannels>::optimal_43(interleaved, out, 0.8f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], -0.054748403152f, 1e-5f) << "opt43 x=0.8 ch=" << ch;
    }

    // Optimal 6-point
    MultichannelInterpolation<NumChannels>::optimal_65(interleaved, out, 0.1f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], 0.008757894639f, 1e-5f) << "opt65 x=0.1 ch=" << ch;
    }

    MultichannelInterpolation<NumChannels>::optimal_65(interleaved, out, 0.4f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], -0.012254112690f, 1e-5f) << "opt65 x=0.4 ch=" << ch;
    }

    MultichannelInterpolation<NumChannels>::optimal_65(interleaved, out, 0.8f);
    for (size_t ch = 0; ch < NumChannels; ++ch)
    {
        EXPECT_NEAR(out[ch], 0.013001190240f, 1e-5f) << "opt65 x=0.8 ch=" << ch;
    }
}

}
