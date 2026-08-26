#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace AbacDsp
{

/**
 * @ingroup modulation
 * @brief Sine-LFO amplitude modulation, with a drive control that squares off the LFO.
 *
 * Standard tremolo at drive 0: a sine LFO scales the signal between
 * (1 - depth) and 1. Raising drive pushes that same LFO through a
 * tanh waveshaper before it reaches the gain stage, morphing it from a sine
 * toward a near-square wave - "stutter" is just this effect at full drive
 * and depth, where the gain alternates sharply between 0 and 1, not a
 * separate mechanism.
 */
class Tremolo
{
  public:
    static constexpr float kMaxDriveGain = 20.f;

    explicit Tremolo(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void setRate(const float hz) noexcept
    {
        m_phaseInc = 2.f * std::numbers::pi_v<float> * hz / m_sampleRate;
    }

    void setDepth(const float depth) noexcept
    {
        m_depth = std::clamp(depth, 0.f, 1.f);
    }

    void setDrive(const float drive) noexcept
    {
        m_drive = std::clamp(drive, 0.f, 1.f);
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        const auto lfo = std::sin(m_phase);
        m_phase += m_phaseInc;
        constexpr float pi = std::numbers::pi_v<float>;
        while (m_phase > pi)
        {
            m_phase -= 2.f * pi;
        }
        while (m_phase < -pi)
        {
            m_phase += 2.f * pi;
        }

        const auto driveGain = 1.f + m_drive * kMaxDriveGain;
        const auto shaped = std::tanh(lfo * driveGain) / std::tanh(driveGain);
        const auto gain = 1.f - m_depth * 0.5f * (1.f - shaped);
        return in * gain;
    }

  private:
    const float m_sampleRate;
    float m_phaseInc{0.f};
    float m_phase{0.f};
    float m_depth{0.f};
    float m_drive{0.f};
};

}
