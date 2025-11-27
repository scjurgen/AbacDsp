#pragma once

#include <array>
#include <cmath>

namespace AbacDsp
{
class Interpolation
{
  public:
    static float zeroOrderHold(const float* y, const float /*x*/)
    {
        return y[0];
    }

    static float linearPt2(const float* y, const float x)
    {
        return y[0] + (y[1] - y[0]) * x;
    }

    static float bspline43x(const float* y, const float x)
    {
        constexpr auto oneOverSix = 1.f / 6.f;
        constexpr auto twoOverThree = 2.f / 3.f;
        const auto ym1py1 = y[0] + y[2];
        const auto c0 = oneOverSix * ym1py1 + twoOverThree * y[1];
        const auto c1 = 0.5f * (y[2] - y[0]);
        const auto c2 = 0.5f * ym1py1 - y[1];
        const auto c3 = 0.5f * (y[1] - y[2]) + oneOverSix * (y[3] - y[0]);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    static float bspline43z(const float* y, const float x)
    {
        const auto z = x - 0.5f;
        const auto even1 = y[0] + y[3], modd1 = y[3] - y[0];
        const auto even2 = y[1] + y[2], modd2 = y[2] - y[1];
        const auto c0 = 1.f / 48.f * even1 + 23.f / 48.f * even2;
        const auto c2 = 1.f / 4.f * (even1 - even2);
        const auto c1 = 1.f / 8.f * modd1 + 5.f / 8.f * modd2;
        const auto c3 = 1.f / 6.f * modd1 - 0.5f * modd2;
        return ((c3 * z + c2) * z + c1) * z + c0;
    }

    static float lagrange43x(const float* y, const float x)
    {
        const auto c0 = y[1];
        const auto c1 = y[2] - 1.f / 3.f * y[0] - 0.5f * y[1] - 1.f / 6.f * y[3];
        const auto c2 = 0.5f * (y[0] + y[2]) - y[1];
        const auto c3 = 1.f / 6.f * (y[3] - y[0]) + 0.5f * (y[1] - y[2]);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    static float lagrange43z(const float* y, const float x)
    {
        const auto z = x - 1.f / 2.f;
        const auto even1 = y[0] + y[3], odd1 = y[0] - y[3];
        const auto even2 = y[1] + y[2], odd2 = y[1] - y[2];
        const auto c0 = 9.f / 16.f * even2 - 1.f / 16.f * even1;
        const auto c1 = 1.f / 24.f * odd1 - 9.f / 8.f * odd2;
        const auto c2 = 1.f / 4.f * (even1 - even2);
        const auto c3 = 0.5f * odd2 - 1.f / 6.f * odd1;
        return ((c3 * z + c2) * z + c1) * z + c0;
    }

    static float hermite43x(const float* y, const float x)
    {
        const auto c0 = y[1];
        const auto c1 = 0.5f * (y[2] - y[0]);
        const auto c2 = y[0] - 5.f / 2.f * y[1] + 2.f * y[2] - 0.5f * y[3];
        const auto c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    static float hermite43z(const float* y, const float x)
    {
        const auto z = x - 1.f / 2.f;
        const auto even1 = y[0] + y[3], odd1 = y[0] - y[3];
        const auto even2 = y[1] + y[2], odd2 = y[1] - y[2];
        const auto c0 = 9.f / 16.f * even2 - 1.f / 16.f * even1;
        const auto c1 = 1.f / 8.f * odd1 - 11.f / 8.f * odd2;
        const auto c2 = 1.f / 4.f * (even1 - even2);
        const auto c3 = 1.5f * odd2 - 0.5f * odd1;
        return ((c3 * z + c2) * z + c1) * z + c0;
    }

    static float bspline65x(const float* y, const float x)
    {
        const auto ym2py2 = y[0] + y[4], ym1py1 = y[1] + y[3];
        const auto y2mym2 = y[4] - y[0], y1mym1 = y[3] - y[1];
        const auto sixthym1py1 = 1.f / 6.f * ym1py1;
        const auto c0 = 1.f / 120.f * ym2py2 + 13.f / 60.f * ym1py1 + 11.f / 20.f * y[2];
        const auto c1 = 1.f / 24.f * y2mym2 + 5.f / 12.f * y1mym1;
        const auto c2 = 1.f / 12.f * ym2py2 + sixthym1py1 - 0.5f * y[2];
        const auto c3 = 1.f / 12.f * y2mym2 - 1.f / 6.f * y1mym1;
        const auto c4 = 1.f / 24.f * ym2py2 - sixthym1py1 + 1.f / 4.f * y[2];
        const auto c5 = 1.f / 120.f * (y[5] - y[0]) + 1.f / 24.f * (y[1] - y[4]) + 1.f / 12.f * (y[3] - y[2]);
        return ((((c5 * x + c4) * x + c3) * x + c2) * x + c1) * x + c0;
    }

    static float lagrange65x(const float* y, const float x)
    {
        const auto ym1py1 = y[1] + y[3];
        const auto twentyfourthym2py2 = 1.f / 24.f * (y[0] + y[4]);
        const auto c0 = y[2];
        const auto c1 =
            1.f / 20.f * y[0] - 0.5f * y[1] - 1.f / 3.f * y[2] + y[3] - 1.f / 4.f * y[4] + 1.f / 30.f * y[5];
        const auto c2 = 2.f / 3.f * ym1py1 - 5.f / 4.f * y[2] - twentyfourthym2py2;
        const auto c3 = 5.f / 12.f * y[2] - 7.f / 12.f * y[3] + 7.f / 24.f * y[4] - 1.f / 24.f * (y[0] + y[1] + y[5]);
        const auto c4 = 1.f / 4.f * y[2] - 1.f / 6.f * ym1py1 + twentyfourthym2py2;
        const auto c5 = 1.f / 120.f * (y[5] - y[0]) + 1.f / 24.f * (y[1] - y[4]) + 1.f / 12.f * (y[3] - y[2]);
        return ((((c5 * x + c4) * x + c3) * x + c2) * x + c1) * x + c0;
    }

    static float lagrange65z(const float* y, const float x)
    {
        const auto z = x - 1.f / 2.f;
        const auto even1 = y[0] + y[5], odd1 = y[0] - y[5];
        const auto even2 = y[1] + y[4], odd2 = y[1] - y[4];
        const auto even3 = y[2] + y[3], odd3 = y[2] - y[3];
        const auto c0 = 3.f / 256.f * even1 - 25.f / 256.f * even2 + 75.f / 128.f * even3;
        const auto c1 = 25.f / 384.f * odd2 - 75.f / 64.f * odd3 - 3.f / 640.f * odd1;
        const auto c2 = 13.f / 32.f * even2 - 17.f / 48.f * even3 - 5.f / 96.f * even1;
        const auto c3 = 1.f / 48.f * odd1 - 13.f / 48.f * odd2 + 17.f / 24.f * odd3;
        const auto c4 = 1.f / 48.f * even1 - 1.f / 16.f * even2 + 1.f / 24.f * even3;
        const auto c5 = 1.f / 24.f * odd2 - 1.f / 12.f * odd3 - 1.f / 120.f * odd1;
        return ((((c5 * z + c4) * z + c3) * z + c2) * z + c1) * z + c0;
    }

    static float hermite63x(const float* y, const float x)
    {
        const auto c0 = y[2];
        const auto c1 = 1.f / 12.f * (y[0] - y[4]) + 2.f / 3.f * (y[3] - y[1]);
        const auto c2 =
            5.f / 4.f * y[1] - 7.f / 3.f * y[2] + 5.f / 3.f * y[3] - 0.5f * y[4] + 1.f / 12.f * y[5] - 1.f / 6.f * y[0];
        const auto c3 = 1.f / 12.f * (y[0] - y[5]) + 7.f / 12.f * (y[4] - y[1]) + 4.f / 3.f * (y[2] - y[3]);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    static float hermite63z(const float* y, const float x)
    {
        const auto z = x - 1.f / 2.f;
        const auto even1 = y[0] + y[5], odd1 = y[0] - y[5];
        const auto even2 = y[1] + y[4], odd2 = y[1] - y[4];
        const auto even3 = y[2] + y[3], fourthirdthodd3 = 4.f / 3.f * (y[2] - y[3]);
        const auto c0 = 1.f / 96.f * even1 - 3.f / 32.f * even2 + 7.f / 12.f * even3;
        const auto c1 = 7.f / 48.f * odd2 - fourthirdthodd3 - 1.f / 48.f * odd1;
        const auto c2 = 3.f / 8.f * even2 - 1.f / 3.f * even3 - 1.f / 24.f * even1;
        const auto c3 = 1.f / 12.f * odd1 - 7.f / 12.f * odd2 + fourthirdthodd3;
        return ((c3 * z + c2) * z + c1) * z + c0;
    }

    static float hermite65x(const float* y, const float x)
    {
        const auto eighthym2 = 1.f / 8.f * y[0];
        const auto eleventwentyfourthy2 = 11.f / 24.f * y[4];
        const auto twelfthy3 = 1.f / 12.f * y[5];
        const auto c0 = y[2];
        const auto c1 = 1.f / 12.f * (y[0] - y[4]) + 2.f / 3.f * (y[3] - y[1]);
        const auto c2 =
            13.f / 12.f * y[1] - 25.f / 12.f * y[2] + 1.5f * y[3] - eleventwentyfourthy2 + twelfthy3 - eighthym2;
        const auto c3 = 5.f / 12.f * y[2] - 7.f / 12.f * y[3] + 7.f / 24.f * y[4] - 1.f / 24.f * (y[0] + y[1] + y[5]);
        const auto c4 = eighthym2 - 7.f / 12.f * y[1] + 13.f / 12.f * y[2] - y[3] + eleventwentyfourthy2 - twelfthy3;
        const auto c5 = 1.f / 24.f * (y[5] - y[0]) + 5.f / 24.f * (y[1] - y[4]) + 5.f / 12.f * (y[3] - y[2]);
        return ((((c5 * x + c4) * x + c3) * x + c2) * x + c1) * x + c0;
    }

    static float hermite65z(const float* y, const float x)
    {
        const auto z = x - 1.f / 2.f;
        const auto even1 = y[0] + y[5], odd1 = y[0] - y[5];
        const auto even2 = y[1] + y[4], odd2 = y[1] - y[4];
        const auto even3 = y[2] + y[3], odd3 = y[2] - y[3];
        const auto c0 = 3.f / 256.f * even1 - 25.f / 256.f * even2 + 75.f / 128.f * even3;
        const auto c1 = -3 / 128.f * odd1 + 61.f / 384.f * odd2 - 87.f / 64.f * odd3;
        const auto c2 = -5 / 96.f * even1 + 13.f / 32.f * even2 - 17.f / 48.f * even3;
        const auto c3 = 5.f / 48.f * odd1 - 11.f / 16.f * odd2 + 37.f / 24.f * odd3;
        const auto c4 = 1.f / 48.f * even1 - 1.f / 16.f * even2 + 1.f / 24.f * even3;
        const auto c5 = -1 / 24.f * odd1 + 5.f / 24.f * odd2 - 5.f / 12.f * odd3;
        return ((((c5 * z + c4) * z + c3) * z + c2) * z + c1) * z + c0;
    }
};
}