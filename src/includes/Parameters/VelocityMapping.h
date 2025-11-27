#pragma once

#include <algorithm>
#include <cmath>

namespace AbacDsp
{
class VelocityMapping
{
  public:
    static float getNormalized(const float xNormalized, const float dynamicRangeNormalized)
    {
        const auto p = 1.f - std::cbrt(dynamicRangeNormalized);
        const auto gain = xNormalized * p + 1 - p;
        return gain * gain * gain;
    }

    static float get(const uint8_t velocity, const float dynamicRangeNormalized)
    {
        const auto p = 1.f - std::cbrt(dynamicRangeNormalized);
        const auto gain = std::min<float>(velocity, 127.f) / 127.f * p + 1.f - p;
        return gain * gain * gain;
    }
};
}