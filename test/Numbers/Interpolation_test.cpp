#include "gtest/gtest.h"

#include <cmath>
#include "Numbers/Interpolation.h"

TEST(NumberInterpolationTests, dirac)
{
    constexpr std::array input = {0.f, 0.f, 0.0f, 1.0f, 0.f, 0.f, 0.f};
    const std::array expected = {
        0.f,        0.000325521f, 0.00260417f, 0.00878906f, 0.0208333f,   0.0406901f, 0.0703125f, 0.111654f, 0.166667f,
        0.236003f,  0.315104f,    0.398112f,   0.479167f,   0.552409f,    0.611979f,  0.652018f,  0.666667f, 0.652018f,
        0.611979f,  0.552409f,    0.479167f,   0.398112f,   0.315104f,    0.236003f,  0.166667f,  0.111654f, 0.0703125f,
        0.0406901f, 0.0208333f,   0.00878905f, 0.00260417f, 0.000325516f, 0.f,
    };
    for (size_t f = 0; f <= 32; ++f)
    {
        float output = 0.0f;
        float x = static_cast<float>(f) * 0.125f;
        size_t idx = floor(x);
        MultichannelInterpolation<1>::bspline_43x(&input[idx], &output, x - idx);
        EXPECT_NEAR(expected[f], output, 1E-5f);
    }
}

TEST(NumberInterpolationTests, bspline43SingleChannelSweep)
{
    constexpr std::array input = {1.0f, 2.0f, 0.0f, -1.0f};
    const std::array expected = {
        1.5f, 1.41536f, 1.29167f, 1.13672f, 0.958333f, 0.764323f, 0.5625f, 0.360677f, 0.166667f,
    };

    for (size_t f = 0; f <= 8; ++f)
    {
        float output = 0.0f;
        float x = static_cast<float>(f) * 0.125f;
        MultichannelInterpolation<1>::bspline_43x(input.data(), &output, x);
        EXPECT_NEAR(output, expected[f], 1e-5) << "failed at x=" << x;
    }
}

TEST(NumberInterpolationTests, bspline43StereoIsMirrored)
{
    // 1,2,3,4, and 4,3,2,1, mirror results
    constexpr std::array input = {
        1.0f, 4.0f, // frame 0: L, R
        2.0f, 3.0f, // frame 1: L, R
        3.0f, 2.0f, // frame 2: L, R
        4.0f, 1.0f  // frame 3: L, R
    };

    constexpr int N = 9;
    float left[N], right[N];

    for (int i = 0; i < N; ++i)
    {
        const float x = static_cast<float>(i) * 0.125f;
        float output[2] = {0.0f, 0.0f};
        MultichannelInterpolation<2>::bspline_43x(input.data(), output, x);
        left[i] = output[0];
        right[i] = output[1];
    }
    // l_x should equal r_(1.0-x)
    for (int i = 0; i < N; ++i)
    {
        EXPECT_NEAR(left[i], right[N - 1 - i], 1e-5) << "failed at x=" << (static_cast<float>(i) * 0.125f)
                                                     << ": left=" << left[i] << " right=" << right[N - 1 - i];
    }
}
