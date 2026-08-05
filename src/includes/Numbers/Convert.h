#pragma once

#include <cmath>
#include <limits>
#include <numbers>

#include "Numbers/Approximation.h"

/**
 * @file
 * @ingroup numbers
 * @brief Unit conversions between the domains a control and the DSP work in.
 *
 * Panning here is the sin/cos constant-power law, not a linear crossfade: two
 * uncorrelated signals summed at linear half gain lose about 3 dB in the
 * middle of the sweep, which a listener hears as a hole in the centre.
 * @see https://en.wikipedia.org/wiki/Decibel
 */

namespace Convert
{
/// @ingroup numbers
/// @brief Constant-power pan factors, angle given in percent over -100 to +100.
template <std::floating_point T>
void getPanFactor(const T angleInPercent, T& left, T& right)
{
    const T f = std::sqrt(T(2)) / T(2);
    const T angle = angleInPercent * (std::numbers::pi_v<T> / T(400));
    const T cosVal = std::cos(angle);
    const T sinVal = std::sin(angle);
    left = f * (cosVal - sinVal);
    right = f * (cosVal + sinVal);
}

/// @ingroup numbers
/// @brief Constant-power pan factors from a normalised -1 to +1 angle.
/// Uses the minimax polynomial sin and cos, so it is cheap enough to call per sample.
template <std::floating_point T>
void getPanFactorNormalized(const T angleNormalized, T& left, T& right)
{
    constexpr T f = static_cast<T>(0.7071067811865476);

    const T angle = angleNormalized * static_cast<T>(std::numbers::pi_v<T> / 4);
    const T cosVal = Approximation::remezCosP6<Approximation::DomainMinusPiHalfToPiHalf>(static_cast<float>(angle));
    const T sinVal = Approximation::remezSinP5<Approximation::DomainMinusPiHalfToPiHalf>(static_cast<float>(angle));
    left = f * (cosVal - sinVal);
    right = f * (cosVal + sinVal);
}

/// @ingroup numbers
/// @brief Decibels to an amplitude ratio: 10^(dB/20).
template <std::floating_point T>
[[nodiscard]] T dbToGain(const T dB)
{
    return std::pow(T(10), dB / T(20));
}

/// @ingroup numbers
/// @brief Amplitude ratio to decibels, returning negative infinity at zero rather than a domain error.
template <std::floating_point T>
[[nodiscard]] T gainToDb(const T gain)
{
    if (gain <= T(0))
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
[[nodiscard]] T frequencyToNote(const T f, const T orchestraTuning = T(440))
{
    return std::log2(f / orchestraTuning) * T(12) + T(69);
}

template <std::floating_point T>
[[nodiscard]] T noteToFrequency(const T note, const T orchestraTuning = T(440))
{
    return orchestraTuning * std::exp2((note - T(69)) / T(12));
}

template <std::floating_point T>
[[nodiscard]] T noteToFrequency(const int note, const T orchestraTuning = T(440))
{
    return orchestraTuning * std::exp2((static_cast<T>(note) - T(69)) / T(12));
}

template <std::floating_point T>
[[nodiscard]] T noteIntervalToRatio(const T interval)
{
    return std::exp2(interval / T(12));
}

template <std::floating_point T>
[[nodiscard]] T ratioToNoteInterval(const T ratio)
{
    return std::log2(ratio) * T(12);
}

template <std::floating_point T>
[[nodiscard]] T centsToRelativePitch(const T cents)
{
    return std::exp2(cents / T(1200));
}

template <std::floating_point T>
[[nodiscard]] T metersToSamples(const T meters, const T sampleRate, const T speedOfSoundMps = T(333.3))
{
    return sampleRate * meters / speedOfSoundMps;
}

template <std::floating_point T>
[[nodiscard]] T samplesToMeters(const T samples, const T sampleRate, const T speedOfSoundMps = T(333.3))
{
    return samples * speedOfSoundMps / sampleRate;
}

}