#pragma once

#include <cmath>
#include <cstddef>

#include "Parameters/SmoothingParameter.h"

namespace AbacDsp
{

/**
 * @ingroup parameters
 * @brief Glides a speed ratio at a fixed rate in octaves per second, faster up than down.
 *
 * Transition time is the octaves to travel over the rate, so it does not depend on the
 * starting ratio. Speeding up is quicker than slowing down, like a tape transport
 * accelerating and braking. Ratios must be positive; the caller clamps to its own range.
 */
class OctaveGlide
{
  public:
    static constexpr float kAccelOctavesPerSec{6.f};
    static constexpr float kBrakeOctavesPerSec{3.f};

    explicit OctaveGlide(const float sampleRate, const float initialRatio = 1.f,
                         const float accelOctavesPerSec = kAccelOctavesPerSec,
                         const float brakeOctavesPerSec = kBrakeOctavesPerSec) noexcept
        : m_sampleRate(sampleRate)
        , m_accel(accelOctavesPerSec)
        , m_brake(brakeOctavesPerSec)
        , m_smoothing(initialRatio)
    {
    }

    void setTarget(const float ratio, const bool force = false) noexcept
    {
        const auto last = m_smoothing.getLastValue();
        const auto octaves = std::abs(std::log2(ratio / last));
        m_smoothing.newTransition(ratio, octaves / (ratio > last ? m_accel : m_brake), m_sampleRate, force);
    }

    /// Advances by the given number of samples and returns the ratio reached.
    [[nodiscard]] float getValue(const size_t samples = 1) noexcept
    {
        return m_smoothing.getValue(samples);
    }

  private:
    float m_sampleRate;
    float m_accel;
    float m_brake;
    LinearSmoothing m_smoothing;
};

}
