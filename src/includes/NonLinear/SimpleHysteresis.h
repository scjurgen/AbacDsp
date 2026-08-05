#pragma once

#include <array>
#include <cmath>
#include <numbers>

namespace AbacDsp
{

/**
 * @ingroup nonlinear
 * @brief Asymmetric envelope whose attack and decay rates differ, and are themselves smoothed.
 *
 * Rising and falling at different rates makes the output depend on the
 * direction of travel, so a given input maps to different outputs depending on
 * where it came from: hysteresis, the behaviour of magnetic media and of many
 * analog envelope circuits.
 *
 * The rate coefficients are themselves ramped over a millisecond, so switching
 * direction does not put a discontinuity into the envelope.
 * @see https://en.wikipedia.org/wiki/Hysteresis
 */
class SimpleHysteresis
{
  public:
    static constexpr float SmoothingTimeSeconds = 0.001f;

    explicit SimpleHysteresis(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_smoothingCoeff(calculateSmoothingCoeff(SmoothingTimeSeconds, sampleRate))
    {
        setFrequencyResponse(5000.f, 9000.f);
    }

    void setFrequencyResponse(const float attackHz, const float decayHz) noexcept
    {
        m_targetAttackRate = std::exp(-2.0f * std::numbers::pi_v<float> * attackHz / m_sampleRate);
        m_targetDecayRate = std::exp(-2.0f * std::numbers::pi_v<float> * decayHz / m_sampleRate);
    }

    [[nodiscard]] float step(const float x) noexcept
    {
        m_attackRate += (m_targetAttackRate - m_attackRate) * m_smoothingCoeff;
        m_decayRate += (m_targetDecayRate - m_decayRate) * m_smoothingCoeff;

        const auto error1 = x - m_state[0];
        m_state[0] += error1 * (error1 > 0.f ? m_attackRate : m_decayRate);
        const auto error2 = m_state[0] - m_state[1];
        m_state[1] += error2 * (error2 > 0.f ? m_attackRate : m_decayRate);
        return m_state[1];
    }

  private:
    [[nodiscard]] static float calculateSmoothingCoeff(const float timeConstant, const float sampleRate) noexcept
    {
        // Coefficient for exponential smoothing
        // alpha = 1 - exp(-deltaT / timeConstant)
        const float deltaT = 1.0f / sampleRate;
        return 1.0f - std::exp(-deltaT / timeConstant);
    }

    const float m_sampleRate;
    std::array<float, 2> m_state{};
    float m_attackRate{0.1f};
    float m_decayRate{0.05f};
    float m_targetAttackRate{0.1f};
    float m_targetDecayRate{0.05f};
    const float m_smoothingCoeff;
};

}