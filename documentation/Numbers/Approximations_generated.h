#pragma once

#include <cmath>
#include <numbers>

namespace Approximation {

// Domain tags
struct DomainMinusOneToOne {};
struct DomainMinusPiHalfToPiHalf {};

// Template declarations
template<typename Domain> inline float remezSinP3(float x) noexcept;
template<typename Domain> inline float remezSinP5(float x) noexcept;
template<typename Domain> inline float remezCosP4(float x) noexcept;
template<typename Domain> inline float remezCosP6(float x) noexcept;

// Max error: 4.49e-03
template<>
inline float remezSinP3<DomainMinusPiHalfToPiHalf>(float x) noexcept {
    return 0.985529543f * x + -0.142566726f * x * x * x;
}

// Max error: 6.77e-05
template<>
inline float remezSinP5<DomainMinusPiHalfToPiHalf>(float x) noexcept {
    return 0.999696773f * x + -0.165673079f * x * x * x + 0.007514377f * x * x * x * x * x;
}

// Max error: 4.49e-03
template<>
inline float remezSinP3<DomainMinusOneToOne>(float x) noexcept {
    return 1.548066186f * x + -0.552557921f * x * x * x;
}

// Max error: 6.77e-05
template<>
inline float remezSinP5<DomainMinusOneToOne>(float x) noexcept {
    return 1.570320019f * x + -0.642113167f * x * x * x + 0.071860854f * x * x * x * x * x;
}

// Max error: 5.97e-04
template<>
inline float remezCosP4<DomainMinusPiHalfToPiHalf>(float x) noexcept {
    const auto x2 = x * x;
    return 0.999403229f + -0.495580849f * x2 + 0.036791683f * x2 * x2;
}

// Max error: 6.70e-06
template<>
inline float remezCosP6<DomainMinusPiHalfToPiHalf>(float x) noexcept {
    const auto x2 = x * x;
    return 0.999993295f + -0.499912440f * x2 + 0.041487748f * x2 * x2 + -0.001271209f * x2 * x2 * x2;
}

// Max error: 5.97e-04
template<>
inline float remezCosP4<DomainMinusOneToOne>(float x) noexcept {
    const auto x2 = x * x;
    return 0.999403229f + -1.222796733f * x2 + 0.223990274f * x2 * x2;
}

// Max error: 6.70e-06
template<>
inline float remezCosP6<DomainMinusOneToOne>(float x) noexcept {
    const auto x2 = x * x;
    return 0.999993295f + -1.233484504f * x2 + 0.252580239f * x2 * x2 + -0.019095735f * x2 * x2 * x2;
}

} // namespace Approximation
