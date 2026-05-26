/**
 * @file Approximations_generated.h
 * @brief Minimax (Remez) polynomial approximations of sin and cos.
 *
 * All functions are single-precision, branch-free, and use Horner evaluation.
 * Coefficients are computed by Sollya at prec=64 (half-wave) or prec=128 (full-wave).
 *
 * ## Domain tags
 * | Tag                      | Input range        | Notes                              |
 * |--------------------------|--------------------|------------------------------------|
 * | DomainMinusPiHalfToPiHalf| [-π/2,  π/2]       | Half-wave, natural radian input    |
 * | DomainMinusOneToOne      | [-1,    1]         | Normalised; mapped to ±π/2 or ±π  |
 * | DomainMinusPiToPi        | [-π,    π]         | Full-wave, natural radian input    |
 *
 * ## Which variant to use
 *
 * **Half-wave (`remezSin*`, `remezCos*`)**
 * Use when the caller already range-reduces to [-π/2, π/2], e.g. inside a
 * CORDIC loop or a wavetable oscillator that folds quadrants externally.
 * Lowest degree (P3/P4) suffices for control-rate modulation (~1e-3 error).
 * P5/P6 is adequate for audio-rate oscillators (~1e-5 error).
 *
 * **Full-wave symmetric (`remezFullCos*` with DomainMinusPiToPi / DomainMinusOneToOne)**
 * Use when the input covers the full cycle and no external range reduction
 * is available. Fits even-only coefficients on the full domain; same cost
 * as half-wave at equal degree but somewhat larger error due to wider domain.
 *
 * **Full-wave abs-folded (`remezFullCosAbsFold*`)**
 * Folds the domain to [0, π] via std::abs(x) before polynomial evaluation.
 * All polynomial degrees are active (not just even), giving a tighter minimax
 * fit for the same degree compared to the symmetric variant.
 * Preferred for full-wave cosine when a single extra abs is acceptable.
 * No equivalent for sine (sine is odd, not even; abs-folding breaks it).
 *
 * **Normalised domain (`DomainMinusOneToOne`)**
 * Use when the phase is already in a unit range, e.g. a phasor oscillator
 * producing values in [-1, 1]. Avoids an explicit multiply by π at the call site.
 *
 * ## Error budget summary (approximate, post-rounding)
 * | Function                              | Max error  |
 * |---------------------------------------|------------|
 * | remezSinP3 / remezCosP4               | ~4e-3      |
 * | remezSinP5 / remezCosP6               | ~7e-5      |
 * | remezFullSinP5 / remezFullCosP6       | ~2e-3      |
 * | remezFullSinP7 / remezFullCosP8       | ~2e-5      |
 * | remezFullSinP9 / remezFullCosP10      | ~2e-7      |
 * | remezFullCosAbsFoldP4                 | ~2e-4      |
 * | remezFullCosAbsFoldP6                 | ~2e-7      |
 */
#pragma once

#include <cmath>

namespace Approximation
{

struct DomainMinusOneToOne
{
};
struct DomainMinusPiHalfToPiHalf
{
};
struct DomainMinusPiToPi
{
};

template <typename Domain>
[[nodiscard]] inline float remezSinP3(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezSinP5(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezCosP4(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezCosP6(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullSinP5(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullSinP7(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullSinP9(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullCosP6(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullCosP8(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullCosP10(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullCosAbsFoldP4(const float x) noexcept;
template <typename Domain>
[[nodiscard]] inline float remezFullCosAbsFoldP6(const float x) noexcept;

// Max error: 4.49e-03
template <>
[[nodiscard]] inline float remezSinP3<DomainMinusPiHalfToPiHalf>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * (-0.142566727f * x2 + 0.985529543f);
}

// Max error: 6.77e-05
template <>
[[nodiscard]] inline float remezSinP5<DomainMinusPiHalfToPiHalf>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * ((0.007514377f * x2 + -0.165673079f) * x2 + 0.999696773f);
}

// Max error: 5.97e-04
template <>
[[nodiscard]] inline float remezCosP4<DomainMinusPiHalfToPiHalf>(const float x) noexcept
{
    const auto x2 = x * x;
    return ((0.036791683f * x2 + -0.495580849f) * x2 + 0.999403229f);
}

// Max error: 6.70e-06
template <>
[[nodiscard]] inline float remezCosP6<DomainMinusPiHalfToPiHalf>(const float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.001271209f * x2 + 0.041487748f) * x2 + -0.499912440f) * x2 + 0.999993295f);
}

// Max error: 4.49e-03
template <>
[[nodiscard]] inline float remezSinP3<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * (-0.552557921f * x2 + 1.548066186f);
}

// Max error: 6.77e-05
template <>
[[nodiscard]] inline float remezSinP5<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * ((0.071860854f * x2 + -0.642113167f) * x2 + 1.570320019f);
}

// Max error: 5.97e-04
template <>
[[nodiscard]] inline float remezCosP4<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return ((0.223990274f * x2 + -1.222796733f) * x2 + 0.999403229f);
}

// Max error: 6.70e-06
template <>
[[nodiscard]] inline float remezCosP6<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.019095736f * x2 + 0.252580240f) * x2 + -1.233484504f) * x2 + 0.999993295f);
}

// Max error: 6.85e-03
template <>
[[nodiscard]] inline float remezFullSinP5<DomainMinusPiToPi>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * ((0.005465398f * x2 + -0.153462570f) * x2 + 0.984415719f);
}

// Max error: 2.50e-04
template <>
[[nodiscard]] inline float remezFullSinP7<DomainMinusPiToPi>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * (((-0.000145077f * x2 + 0.007958062f) * x2 + -0.165666986f) * x2 + 0.999275871f);
}

// Max error: 5.85e-06
template <>
[[nodiscard]] inline float remezFullSinP9<DomainMinusPiToPi>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * ((((0.000002148f * x2 + -0.000192650f) * x2 + 0.008308985f) * x2 + -0.166624385f) * x2 + 0.999979388f);
}

// Max error: 1.39e-03
template <>
[[nodiscard]] inline float remezFullCosP6<DomainMinusPiToPi>(const float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.000969668f * x2 + 0.039227680f) * x2 + -0.495349576f) * x2 + 0.998606596f);
}

// Max error: 4.02e-05
template <>
[[nodiscard]] inline float remezFullCosP8<DomainMinusPiToPi>(const float x) noexcept
{
    const auto x2 = x * x;
    return ((((0.000018792f * x2 + -0.001339266f) * x2 + 0.041496019f) * x2 + -0.499793125f) * x2 + 0.999959795f);
}

// Max error: 7.78e-07
template <>
[[nodiscard]] inline float remezFullCosP10<DomainMinusPiToPi>(const float x) noexcept
{
    const auto x2 = x * x;
    return (((((-0.000000220f * x2 + 0.000024204f) * x2 + -0.001385892f) * x2 + 0.041659822f) * x2 + -0.499994268f) *
                x2 +
            0.999999222f);
}

// Max error: 6.85e-03
template <>
[[nodiscard]] inline float remezFullSinP5<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * ((1.672519451f * x2 + -4.758302910f) * x2 + 3.092633191f);
}

// Max error: 2.50e-04
template <>
[[nodiscard]] inline float remezFullSinP7<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * (((-0.438175042f * x2 + 2.435323557f) * x2 + -5.136716393f) * x2 + 3.139317734f);
}

// Max error: 5.85e-06
template <>
[[nodiscard]] inline float remezFullSinP9<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return x * ((((0.064026175f * x2 + -0.581859413f) * x2 + 2.542712980f) * x2 + -5.166401789f) * x2 + 3.141527899f);
}

// Max error: 1.39e-03
template <>
[[nodiscard]] inline float remezFullCosP6<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.932228331f * x2 + 3.821132684f) * x2 + -4.888904352f) * x2 + 0.998606596f);
}

// Max error: 4.02e-05
template <>
[[nodiscard]] inline float remezFullCosP8<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return ((((0.178306867f * x2 + -1.287555484f) * x2 + 4.042089452f) * x2 + -4.932760424f) * x2 + 0.999959795f);
}

// Max error: 7.78e-07
template <>
[[nodiscard]] inline float remezFullCosP10<DomainMinusOneToOne>(const float x) noexcept
{
    const auto x2 = x * x;
    return (((((-0.020582795f * x2 + 0.229664214f) * x2 + -1.332381204f) * x2 + 4.058045414f) * x2 + -4.934745629f) *
                x2 +
            0.999999222f);
}

// Max error: 4.49e-03
template <>
[[nodiscard]] inline float remezFullCosAbsFoldP4<DomainMinusPiToPi>(const float x) noexcept
{
    const auto ax = std::abs(x);
    return ((((0.000000000f * ax + 0.142566726f) * ax + -0.671829871f) * ax + 0.069778350f) * ax + 0.995508265f);
}

// Max error: 6.77e-05
template <>
[[nodiscard]] inline float remezFullCosAbsFoldP6<DomainMinusPiToPi>(const float x) noexcept
{
    const auto ax = std::abs(x);
    return (
        (((((-0.000000000f * ax + -0.007514377f) * ax + 0.059017780f) * ax + -0.019736746f) * ax + -0.489474921f) * ax +
         -0.002091162f) *
            ax +
        1.000067706f);
}

// Max error: 4.49e-03
template <>
[[nodiscard]] inline float remezFullCosAbsFoldP4<DomainMinusOneToOne>(const float x) noexcept
{
    const auto ax = std::abs(x);
    return ((((0.000000001f * ax + 4.420463366f) * ax + -6.630695050f) * ax + 0.219215153f) * ax + 0.995508265f);
}

// Max error: 6.77e-05
template <>
[[nodiscard]] inline float remezFullCosAbsFoldP6<DomainMinusOneToOne>(const float x) noexcept
{
    const auto ax = std::abs(x);
    return (
        (((((-0.000000002f * ax + -2.299547329f) * ax + 5.748868331f) * ax + -0.611962999f) * ax + -4.830923835f) * ax +
         -0.006569579f) *
            ax +
        1.000067706f);
}

} // namespace Approximation
