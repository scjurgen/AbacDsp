#pragma once

#include <cmath>
#include <numbers>
#include <random>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Band-limited brown noise: a leaky random walk with a DC-blocking high-pass.
 *
 * The center frequency sets both the low-pass integration rate and the
 * high-pass rate that removes drift, via a fixed ratio between the two, so a
 * single control moves the whole passband rather than two independent
 * cutoffs. compensationGain keeps output level roughly constant as the
 * center frequency changes.
 */
class BrownModulation
{
  public:
    explicit BrownModulation(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        updateCutoffs(1.0f);
    }

    void setCenterFrequency(const float centerFrequency) noexcept
    {
        updateCutoffs(centerFrequency);
    }

    [[nodiscard]] float step() noexcept
    {
        const float white = m_dist(m_rng);
        m_brown = white + m_alphaLPF * (m_brown - white);
        const float output = m_brown - m_dcState;
        m_dcState = m_brown + m_alphaHPF * (m_dcState - m_brown);
        return m_compensationGain * output;
    }

    void seed(const std::mt19937::result_type seedValue) noexcept
    {
        m_rng.seed(seedValue);
    }

  private:
    static constexpr float kLpfHpfRatio{200.0f};

    void updateCutoffs(const float centerFrequency) noexcept
    {
        const float sqrtRatio = std::sqrt(kLpfHpfRatio);
        const float factor = -2.0f * std::numbers::pi_v<float> / m_sampleRate * centerFrequency;
        m_alphaLPF = std::exp(factor * sqrtRatio);
        m_alphaHPF = std::exp(factor / sqrtRatio);
        m_compensationGain = 0.25f * std::sqrt((1.0f + m_alphaLPF) / (1.0f - m_alphaLPF));
    }

    const float m_sampleRate;
    float m_alphaLPF{0.0f};
    float m_alphaHPF{0.0f};
    float m_compensationGain{1.0f};
    float m_brown{0.0f};
    float m_dcState{0.0f};

    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_dist{-1.0f, 1.0f};
};

}
