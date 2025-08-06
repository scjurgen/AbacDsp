#pragma once

#include <concepts>
#include <cstddef>

template <typename T_>
class Interpolation
{
  public:
    [[maybe_unused]] static T_ zeroOrderHold(const T_* y, const T_ /*x*/)
    {
        return y[0];
    }

    [[maybe_unused]] static T_ linearPt2(const T_* y, const T_ x)
    {
        return y[0] + (y[1] - y[0]) * x;
    }
    [[maybe_unused]] static T_ bspline_43x(const T_* y, const T_ x)
    {
        const auto ym1py1 = y[0] + y[2];
        const auto c0 = 1.f / 6.f * ym1py1 + 2.f / 3.f * y[1];
        const auto c1 = 1.f / 2.f * (y[2] - y[0]);
        const auto c2 = 1.f / 2.f * ym1py1 - y[1];
        const auto c3 = 1.f / 2.f * (y[1] - y[2]) + 1.f / 6.f * (y[3] - y[0]);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    // 4-point, 3rd-order B-spline (z-form)
    [[maybe_unused]] static T_ bspline_43z(const T_* y, const T_ x)
    {
        const auto z = x - 1.f / 2.f;
        const auto even1 = y[0] + y[3], modd1 = y[3] - y[0];
        const auto even2 = y[1] + y[2], modd2 = y[2] - y[1];
        const auto c0 = 1.f / 48.f * even1 + 23.f / 48.f * even2;
        const auto c1 = 1.f / 8.f * modd1 + 5.f / 8.f * modd2;
        const auto c2 = 1.f / 4.f * (even1 - even2);
        const auto c3 = 1.f / 6.f * modd1 - 1.f / 2.f * modd2;
        return ((c3 * z + c2) * z + c1) * z + c0;
    }
    // 6-point, 5th-order B-spline (x-form)
    [[maybe_unused]] static T_ bspline_65x(const T_* y, const T_ x)
    {
        const auto ym2py2 = y[0] + y[4], ym1py1 = y[1] + y[3];
        const auto y2mym2 = y[4] - y[0], y1mym1 = y[3] - y[1];
        const auto sixthym1py1 = 1.f / 6.f * ym1py1;
        const auto c0 = 1.f / 120.f * ym2py2 + 13.f / 60.f * ym1py1 + 11.f / 20.f * y[2];
        const auto c1 = 1.f / 24.f * y2mym2 + 5.f / 12.f * y1mym1;
        const auto c2 = 1.f / 12.f * ym2py2 + sixthym1py1 - 1.f / 2.f * y[2];
        const auto c3 = 1.f / 12.f * y2mym2 - 1.f / 6.f * y1mym1;
        const auto c4 = 1.f / 24.f * ym2py2 - sixthym1py1 + 1.f / 4.f * y[2];
        const auto c5 = 1.f / 120.f * (y[5] - y[0]) + 1.f / 24.f * (y[1] - y[4]) + 1.f / 12.f * (y[3] - y[2]);
        return ((((c5 * x + c4) * x + c3) * x + c2) * x + c1) * x + c0;
    }
};

template <size_t NumChannels>
    requires(NumChannels > 0)
class MultichannelInterpolation
{
  public:
    // needs at lease point to 4 interleaved frames
    static void bspline_43x(const float* interleaved, float* out, float x)
    {
        static_assert(NumChannels > 0, "Need at least one channel");

        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            const float y0 = interleaved[NumChannels * 0 + ch];
            const float y1 = interleaved[NumChannels * 1 + ch];
            const float y2 = interleaved[NumChannels * 2 + ch];
            const float y3 = interleaved[NumChannels * 3 + ch];

            const float ym1py1 = y0 + y2;
            const float c0 = 1.0f / 6.0f * ym1py1 + 2.0f / 3.0f * y1;
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = 0.5f * ym1py1 - y1;
            const float c3 = 0.5f * (y1 - y2) + 1.0f / 6.0f * (y3 - y0);

            out[ch] = ((c3 * x + c2) * x + c1) * x + c0;
        }
    }
};
