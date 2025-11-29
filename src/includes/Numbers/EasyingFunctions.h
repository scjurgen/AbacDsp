#pragma once

#include <cmath>
#include <algorithm>
#include <iostream>

namespace AbacDsp::Easying
{
// Quartic smoothstep: f(x) = 1 + 16*c*x²*(x-1)²
template <typename T>
constexpr T smoothStep4(T x, T c = T(0)) noexcept
{
    const T t = x * (x - T(1));
    return T(1) + T(16) * c * t * t;
}

// Quadratic smoothstep: f(x) = 1 - 4*c*x² + 4*c*x
template <typename T>
constexpr T smoothStep2(T x, T c = T(0)) noexcept
{
    const T x2 = x * x;
    return T(1) - T(4) * c * x2 + T(4) * c * x;
}


// Integral: F(x) = x + (16c/5)x⁵ - 8c*x⁴ + (16c/3)x³ + C
template <typename T>
constexpr T smoothStep4Integral(T x, T c = T(0)) noexcept
{
    const T x2 = x * x;
    const T x3 = x2 * x;
    const T x4 = x2 * x2;
    const T x5 = x3 * x2;
    return x + (T(16) * c / T(5)) * x5 - T(8) * c * x4 + (T(16) * c / T(3)) * x3;
}
}