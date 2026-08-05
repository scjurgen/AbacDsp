#pragma once

#include <cstddef>

/**
 * @ingroup numbers
 * @brief Fractional-delay interpolators, templated on sample type.
 *
 * The type-generic twin of AbacDsp::Interpolation, for the paths that need
 * double precision. Same contract: caller guarantees the support width.
 * @see https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf
 */
template <typename T_>
class Interpolation
{
  public:
    [[nodiscard]] static T_ zeroOrderHold(const T_* y, const T_ /*x*/) noexcept
    {
        return y[0];
    }

    [[nodiscard]] static T_ linearPt2(const T_* y, const T_ x) noexcept
    {
        return y[0] + (y[1] - y[0]) * x;
    }

    // 4-point, 3rd-order Catmull-Rom / cubic Hermite
    [[nodiscard]] static T_ catmullRom(const T_* y, const T_ x) noexcept
    {
        const auto c0 = y[1];
        const auto c1 = T_(0.5) * (y[2] - y[0]);
        const auto c2 = y[0] - T_(2.5) * y[1] + T_(2.0) * y[2] - T_(0.5) * y[3];
        const auto c3 = T_(0.5) * (y[3] - y[0]) + T_(1.5) * (y[1] - y[2]);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    // Niemitalo 4-point, 3rd-order optimal (z-form, 2x oversampled)
    [[nodiscard]] static T_ optimal_43x(const T_* y, const T_ x) noexcept
    {
        const auto even1 = y[1] + y[2], odd1 = y[2] - y[1];
        const auto even2 = y[0] + y[3], odd2 = y[3] - y[0];
        const auto c0 = T_(0.45868970870461538) * even1 + T_(0.04131029129538465) * even2;
        const auto c1 = T_(0.48068024766578432) * odd1 + T_(0.01931975233421571) * odd2;
        const auto c2 = T_(0.22156994685004179) * even2 - T_(0.22156994685004179) * even1;
        const auto c3 = T_(0.01263841022099028) * odd2 - T_(0.49736158977900974) * odd1;
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    // Niemitalo 6-point, 5th-order optimal (z-form, 2x oversampled)
    [[nodiscard]] static T_ optimal_65z(const T_* y, const T_ x) noexcept
    {
        const auto z = x - T_(0.5);
        const auto even1 = y[2] + y[3], odd1 = y[3] - y[2];
        const auto even2 = y[1] + y[4], odd2 = y[4] - y[1];
        const auto even3 = y[0] + y[5], odd3 = y[5] - y[0];
        const auto c0 =
            T_(0.40513396007145713) * even1 + T_(0.09251794688891659) * even2 + T_(0.00234805281832539) * even3;
        const auto c1 =
            T_(0.28342806338906690) * odd1 + T_(0.21703277607228049) * odd2 + T_(0.00007475004966218) * odd3;
        const auto c2 =
            -T_(0.19036558707135655) * even1 + T_(0.26631290118970940) * even2 - T_(0.07594731312055482) * even3;
        const auto c3 =
            -T_(0.20588936433493325) * odd1 + T_(0.30852174138399130) * odd2 - T_(0.10263237705105900) * odd3;
        const auto c4 =
            T_(0.06425981967073851) * even1 - T_(0.12939122779999440) * even2 + T_(0.06513140780507520) * even3;
        const auto c5 =
            T_(0.06962862029025083) * odd1 - T_(0.12424330547643248) * odd2 + T_(0.05461498395990161) * odd3;
        return ((((c5 * z + c4) * z + c3) * z + c2) * z + c1) * z + c0;
    }
};

/**
 * @ingroup numbers
 * @brief Interpolates every channel of an interleaved frame at one shared fraction.
 *
 * Reading all channels at the same offset in a single pass keeps the frame in
 * cache and, more importantly, guarantees the channels stay sample-aligned:
 * interpolating them separately invites a drift that would smear the image.
 */
template <size_t NumChannels>
    requires(NumChannels > 0)
class MultichannelInterpolation
{
  public:
    static void linearPt2(const float* interleaved, float* out, const float x)
    {
        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            const float y0 = interleaved[NumChannels * 0 + ch];
            const float y1 = interleaved[NumChannels * 1 + ch];
            out[ch] = y0 + x * (y1 - y0);
        }
    }

    static void catmullRom(const float* interleaved, float* out, const float x)
    {
        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            const float y[4] = {interleaved[NumChannels * 0 + ch], interleaved[NumChannels * 1 + ch],
                                interleaved[NumChannels * 2 + ch], interleaved[NumChannels * 3 + ch]};
            out[ch] = Interpolation<float>::catmullRom(y, x);
        }
    }

    static void optimal_43(const float* interleaved, float* out, const float x)
    {
        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            const float y[4] = {interleaved[NumChannels * 0 + ch], interleaved[NumChannels * 1 + ch],
                                interleaved[NumChannels * 2 + ch], interleaved[NumChannels * 3 + ch]};
            out[ch] = Interpolation<float>::optimal_43x(y, x);
        }
    }

    static void optimal_65(const float* interleaved, float* out, const float x)
    {
        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            const float y[6] = {interleaved[NumChannels * 0 + ch], interleaved[NumChannels * 1 + ch],
                                interleaved[NumChannels * 2 + ch], interleaved[NumChannels * 3 + ch],
                                interleaved[NumChannels * 4 + ch], interleaved[NumChannels * 5 + ch]};
            out[ch] = Interpolation<float>::optimal_65z(y, x);
        }
    }
};

/// @ingroup numbers
/// @brief Kernel tag: 2-point linear. Support, Pre and Post let a caller size its guard region.
struct LinearKernel
{
    static constexpr int Support = 2;
    static constexpr int Pre = 0;
    static constexpr int Post = 1;
    [[nodiscard]] static float eval(const float* y, const float frac) noexcept
    {
        return Interpolation<float>::linearPt2(y, frac);
    }
};

/// @ingroup numbers
/// @brief Kernel tag: 4-point Catmull-Rom, the usual default for delay-line reads.
struct CatmullRomKernel
{
    static constexpr int Support = 4;
    static constexpr int Pre = 1;
    static constexpr int Post = 2;
    [[nodiscard]] static float eval(const float* y, const float frac) noexcept
    {
        return Interpolation<float>::catmullRom(y, frac);
    }
};

/// @ingroup numbers
/// @brief Kernel tag: 4-point optimal. Same support as Catmull-Rom, flatter response, coefficients fitted rather than derived.
struct Optimal4Kernel
{
    static constexpr int Support = 4;
    static constexpr int Pre = 1;
    static constexpr int Post = 2;
    [[nodiscard]] static float eval(const float* y, const float frac) noexcept
    {
        return Interpolation<float>::optimal_43x(y, frac);
    }
};

struct Optimal6Kernel
{
    static constexpr int Support = 6;
    static constexpr int Pre = 2;
    static constexpr int Post = 3;
    [[nodiscard]] static float eval(const float* y, const float frac) noexcept
    {
        return Interpolation<float>::optimal_65z(y, frac);
    }
};
