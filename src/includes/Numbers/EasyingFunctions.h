#pragma once

/**
 * @file
 * @ingroup numbers
 * @brief Rate-shaping curves used to ease a value from one place to another.
 *
 * These are not the usual 0-to-1 smoothsteps. Each returns a rate near 1 with a
 * bump of height c in the middle, so integrating it over [0, 1] advances by
 * slightly more or less than 1. That makes them velocity profiles for a moving
 * read head rather than position curves.
 *
 * The mean bump matters when sizing a ramp: the quadratic averages 2c/3 over
 * the interval, the quartic 8c/15. A ramp length derived for one is 25 percent
 * wrong for the other.
 */

namespace AbacDsp::Easying
{
/// @ingroup numbers
/// @brief Quartic rate bump, f(x) = 1 + 16*c*x^2*(x-1)^2. Mean over [0, 1] is 1 + 8c/15.
/// Zero slope at both ends, so acceleration is continuous where a quadratic's would jump.
template <typename T>
[[nodiscard]] constexpr T smoothStep4(const T x, const T c = T(0)) noexcept
{
    const T t = x * (x - T(1));
    return T(1) + T(16) * c * t * t;
}

/// @ingroup numbers
/// @brief Quadratic rate bump, f(x) = 1 - 4*c*x^2 + 4*c*x. Mean over [0, 1] is 1 + 2c/3.
/// Cheaper than the quartic but starts and ends with nonzero slope.
template <typename T>
[[nodiscard]] constexpr T smoothStep2(const T x, const T c = T(0)) noexcept
{
    const T x2 = x * x;
    return T(1) - T(4) * c * x2 + T(4) * c * x;
}

/// @ingroup numbers
/// @brief Closed-form integral of smoothStep4(), giving distance travelled by x without summing steps.
/// F(x) = x + (16c/5)x^5 - 8c*x^4 + (16c/3)x^3.
template <typename T>
[[nodiscard]] constexpr T smoothStep4Integral(const T x, const T c = T(0)) noexcept
{
    const T x2 = x * x;
    const T x3 = x2 * x;
    const T x4 = x2 * x2;
    const T x5 = x3 * x2;
    return x + (T(16) * c / T(5)) * x5 - T(8) * c * x4 + (T(16) * c / T(3)) * x3;
}
}