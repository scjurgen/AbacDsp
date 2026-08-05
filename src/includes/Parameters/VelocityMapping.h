#pragma once

#include <algorithm>
#include <cmath>

namespace AbacDsp
{
/**
 * @ingroup parameters
 * @brief Maps MIDI velocity to gain with an adjustable dynamic range.
 *
 * The cube gives a curve close to how loudness is perceived, so equal velocity
 * steps feel evenly spaced rather than bunching at the top. The range control
 * sets how far the softest note falls below the loudest, which is what makes
 * one mapping usable for both an expressive keyboard part and a drum pad that
 * must stay audible however lightly it is struck.
 */
class VelocityMapping
{
  public:
    [[nodiscard]] static float getNormalized(const float xNormalized, const float dynamicRangeNormalized) noexcept
    {
        const auto p = 1.f - std::cbrt(dynamicRangeNormalized);
        const auto gain = xNormalized * p + 1.f - p;
        return gain * gain * gain;
    }

    [[nodiscard]] static float get(const uint8_t velocity, const float dynamicRangeNormalized) noexcept
    {
        const auto p = 1.f - std::cbrt(dynamicRangeNormalized);
        const auto gain = std::min<float>(velocity, 127.f) / 127.f * p + 1.f - p;
        return gain * gain * gain;
    }
};
}