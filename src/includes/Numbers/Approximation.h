#pragma once

#include <cmath>

namespace Approximation
{

// Domain tags
struct DomainMinusOneToOne
{
};
struct DomainMinusPiHalfToPiHalf
{
};

// Template declarations
template <typename Domain>
inline float remezSinP3(float x) noexcept;
template <typename Domain>
inline float remezSinP5(float x) noexcept;
template <typename Domain>
inline float remezCosP4(float x) noexcept;
template <typename Domain>
inline float remezCosP6(float x) noexcept;
template <typename Domain>
inline float remezFullSinP5(float x) noexcept;
template <typename Domain>
inline float remezFullSinP7(float x) noexcept;
template <typename Domain>
inline float remezFullSinP9(float x) noexcept;
template <typename Domain>
inline float remezFullCosP6(float x) noexcept;
template <typename Domain>
inline float remezFullCosP8(float x) noexcept;
template <typename Domain>
inline float remezFullCosP10(float x) noexcept;

// Max error: 4.49e-03
template <>
inline float remezSinP3<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return x * (-0.142566726f * x2 + 0.985529543f);
}

// Max error: 6.77e-05
template <>
inline float remezSinP5<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return x * ((0.007514377f * x2 + -0.165673079f) * x2 + 0.999696773f);
}

// Max error: 5.97e-04
template <>
inline float remezCosP4<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return ((0.036791683f * x2 + -0.495580849f) * x2 + 0.999403229f);
}

// Max error: 6.70e-06
template <>
inline float remezCosP6<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.001271209f * x2 + 0.041487748f) * x2 + -0.499912440f) * x2 + 0.999993295f);
}

// Max error: 4.49e-03
template <>
inline float remezSinP3<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return x * (-0.552557921f * x2 + 1.548066186f);
}

// Max error: 6.77e-05
template <>
inline float remezSinP5<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return x * ((0.071860854f * x2 + -0.642113167f) * x2 + 1.570320019f);
}

// Max error: 5.97e-04
template <>
inline float remezCosP4<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return ((0.223990274f * x2 + -1.222796733f) * x2 + 0.999403229f);
}

// Max error: 6.70e-06
template <>
inline float remezCosP6<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.019095735f * x2 + 0.252580239f) * x2 + -1.233484504f) * x2 + 0.999993295f);
}

// Max error: 6.85e-03
template <>
inline float remezFullSinP5<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return x * ((0.005465398f * x2 + -0.153462571f) * x2 + 0.984415719f);
}

// Max error: 2.50e-04
template <>
inline float remezFullSinP7<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return x * (((-0.000145077f * x2 + 0.007958062f) * x2 + -0.165666986f) * x2 + 0.999275871f);
}

// Max error: 5.85e-06
template <>
inline float remezFullSinP9<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return x * ((((0.000002148f * x2 + -0.000192650f) * x2 + 0.008308985f) * x2 + -0.166624385f) * x2 + 0.999979388f);
}

// Max error: 1.39e-03
template <>
inline float remezFullCosP6<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.000969668f * x2 + 0.039227680f) * x2 + -0.495349576f) * x2 + 0.998606596f);
}

// Max error: 4.02e-05
template <>
inline float remezFullCosP8<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return ((((0.000018792f * x2 + -0.001339266f) * x2 + 0.041496019f) * x2 + -0.499793125f) * x2 + 0.999959795f);
}

// Max error: 7.78e-07
template <>
inline float remezFullCosP10<DomainMinusPiHalfToPiHalf>(float x) noexcept
{
    const auto x2 = x * x;
    return (((((-0.000000220f * x2 + 0.000024204f) * x2 + -0.001385892f) * x2 + 0.041659822f) * x2 + -0.499994268f) *
                x2 +
            0.999999222f);
}

// Max error: 6.85e-03
template <>
inline float remezFullSinP5<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return x * ((1.672519510f * x2 + -4.758302930f) * x2 + 3.092633191f);
}

// Max error: 2.50e-04
template <>
inline float remezFullSinP7<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return x * (((-0.438175043f * x2 + 2.435323560f) * x2 + -5.136716395f) * x2 + 3.139317734f);
}

// Max error: 5.85e-06
template <>
inline float remezFullSinP9<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return x * ((((0.064026175f * x2 + -0.581859413f) * x2 + 2.542712980f) * x2 + -5.166401789f) * x2 + 3.141527899f);
}

// Max error: 1.39e-03
template <>
inline float remezFullCosP6<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return (((-0.932228331f * x2 + 3.821132684f) * x2 + -4.888904352f) * x2 + 0.998606596f);
}

// Max error: 4.02e-05
template <>
inline float remezFullCosP8<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return ((((0.178306867f * x2 + -1.287555484f) * x2 + 4.042089452f) * x2 + -4.932760424f) * x2 + 0.999959795f);
}

// Max error: 7.78e-07
template <>
inline float remezFullCosP10<DomainMinusOneToOne>(float x) noexcept
{
    const auto x2 = x * x;
    return (((((-0.020582795f * x2 + 0.229664214f) * x2 + -1.332381204f) * x2 + 4.058045414f) * x2 + -4.934745629f) *
                x2 +
            0.999999222f);
}

} // namespace Approximation
