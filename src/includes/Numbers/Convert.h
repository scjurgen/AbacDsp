#pragma once

#include <cmath>
#include <numbers>
#include <numeric>

#include "Numbers/Approximation.h"

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
    constexpr T f = static_cast<T>(0.7071067811865476);

    const T angle = angleNormalized * static_cast<T>(std::numbers::pi / 4);
    const T cosVal = Approximation::remezCosP6<Approximation::DomainMinusPiHalfToPiHalf>(static_cast<float>(angle));
    const T sinVal = Approximation::remezSinP5<Approximation::DomainMinusPiHalfToPiHalf>(static_cast<float>(angle));
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


template <std::floating_point T>
static T frequencyToNote(const T f, const T orchestraTuning = 440.f)
{
    return std::log(f / orchestraTuning) / std::log(static_cast<T>(2.0)) * static_cast<T>(12) + static_cast<T>(69);
}

template <std::floating_point T>
static T noteToFrequency(const T note, const T orchestraTuning = 440.f)
{
    return orchestraTuning * std::exp2((note - static_cast<T>(69)) / static_cast<T>(12));
}

template <std::floating_point T>
static T noteIntervalToRatio(const T interval)
{
    return std::exp2(interval / static_cast<T>(12));
}

template <std::floating_point T>
static T ratioToNoteInterval(const T ratio)
{
    return std::log(ratio) / std::log(static_cast<T>(2.0)) * static_cast<T>(12);
}

template <std::floating_point T>
static T centsToRelativePitch(T cents)
{
    return std::exp2(cents / static_cast<T>(1200));
}

}