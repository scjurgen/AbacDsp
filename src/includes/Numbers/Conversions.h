#pragma once

#include <cmath>
#include <numbers>
#include <numeric>

namespace Convert
{
template <std::floating_point T>
void getPanFactor(const T angleInPercent, T& left, T& right)
{
    const T f = static_cast<T>(std::sqrt(T(2)) / T(2));
    T angle = angleInPercent * T(std::numbers::pi / 400);
    T cosVal = std::cos(angle);
    T sinVal = std::sin(angle);
    left = f * (cosVal - sinVal);
    right = f * (cosVal + sinVal);
}

template <std::floating_point T>
void getPanFactorNormalized(const T angleNormalized, T& left, T& right)
{
    const T f = static_cast<T>(std::sqrt(T(2)) / T(2));

    T angle = angleNormalized * T(std::numbers::pi / 4);
    T cosVal = std::cos(angle);
    T sinVal = std::sin(angle);
    left = f * (cosVal - sinVal);
    right = f * (cosVal + sinVal);
}


template <std::floating_point T>
[[nodiscard]] static T dbToGain(T dB)
{
    return pow(T(10), dB / T(20));
}

template <std::floating_point T>
[[nodiscard]] static T gainToDb(T gain)
{
    if (gain <= 0)
    {
        if (std::numeric_limits<T>::is_iec559)
        {
            return -std::numeric_limits<T>::infinity();
        }
        return std::log10(T(1) / static_cast<T>(1 << 23)) * T(20);
    }
    return std::log10(gain) * T(20);
}
}