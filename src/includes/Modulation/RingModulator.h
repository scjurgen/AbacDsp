#pragma once

#include <cmath>
#include <numbers>

namespace AbacDsp
{

/**
 * @ingroup modulation
 * @brief Multiplies the signal by a sine oscillator, producing sum/difference sidebands.
 *
 * Unlike amplitude modulation (Tremolo), the carrier here runs at audio rate
 * with no DC offset, so the output carries no trace of the original pitch -
 * only the sum and difference of every input partial against the carrier
 * frequency. Classic bell/metallic tones come from this inharmonic sideband
 * spread.
 * @see https://en.wikipedia.org/wiki/Ring_modulation
 */
class RingModulator
{
  public:
    explicit RingModulator(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void setFrequency(const float hz) noexcept
    {
        m_phaseInc = 2.f * std::numbers::pi_v<float> * hz / m_sampleRate;
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        const auto out = in * std::sin(m_phase);
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
        return out;
    }

  private:
    const float m_sampleRate;
    float m_phaseInc{0.f};
    float m_phase{0.f};
};

}
