#pragma once

#include <cmath>
#include <numbers>
#include <iostream>

namespace AbacDsp
{
/*
 * possible usage:
    void setHysteresis(const float v)
    {
        const float a = 18000 - v * 12000;
        const float d = 18000 - v * 6000;
        m_hysteresis.setFrequencyResponse(a, d);

    }
 */

class SimpleHysteresis
{
  public:
    static constexpr float SmoothingTimeSeconds = 0.001f;

    explicit SimpleHysteresis(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_smoothingCoeff(calculateSmoothingCoeff(SmoothingTimeSeconds, sampleRate))
    {
        setFrequencyResponse(5000, 9000);
    }

    void setFrequencyResponse(const float attackHz, const float decayHz) noexcept
    {
        m_targetAttackRate = std::exp(-2.0f * std::numbers::pi_v<float> * attackHz / m_sampleRate);
        m_targetDecayRate = std::exp(-2.0f * std::numbers::pi_v<float> * decayHz / m_sampleRate);
    }

    float step(const float x) noexcept
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
    static const float calculateSmoothingCoeff(const float timeConstant, const float sampleRate) noexcept
    {
        // Coefficient for exponential smoothing
        // alpha = 1 - exp(-deltaT / timeConstant)
        const float deltaT = 1.0f / sampleRate;
        return 1.0f - std::exp(-deltaT / timeConstant);
    }

    float m_sampleRate;
    float m_state[2]{};
    float m_attackRate{0.1f};
    float m_decayRate{0.05f};
    float m_targetAttackRate{0.1f};
    float m_targetDecayRate{0.05f};
    float m_smoothingCoeff;
};
}