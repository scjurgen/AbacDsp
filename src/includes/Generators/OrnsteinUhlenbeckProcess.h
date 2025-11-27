#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <random>

namespace AbacDsp
{
/**
 * @brief Ornstein-Uhlenbeck process generator for creating correlated noise sequences.
 *
 * Implements a continuous-time stochastic process that exhibits mean reversion,
 * commonly used for modeling random fluctuations with temporal correlation.
 * The process is discretized for digital audio processing.
 *
 * see also https://en.wikipedia.org/wiki/Ornstein–Uhlenbeck_process
 */
class OrnsteinUhlenbeckProcess
{
  public:
    explicit OrnsteinUhlenbeckProcess(const float sampleRate)
        : m_sqrtDt(1.0f / std::sqrt(sampleRate))
        , m_dt(1.0f / sampleRate)
        , m_x(0.0f)
        , m_normalDist(0.0f, 1.0f / 2.33f)
    {
    }

    void seed(const std::mt19937::result_type seed) noexcept
    {
        m_rng.seed(seed);
        m_normalDist.reset();
    }

    void setSigma(const float sigma) noexcept
    {
        m_sigma = sigma;
    }

    float step() noexcept
    {
        const auto dW = m_normalDist(m_rng);
        // OU process: dx = θ(μ - x)dt + σ√dt * dW
        const auto theta = m_sigma * 20.0f + 1.0f; // Mean reversion rate
        const auto mu = m_sigma;                   // Long-term mean
        m_x += theta * (mu - m_x) * m_dt + m_sigma * m_sqrtDt * dW;
        return m_x;
    }

    void reset() noexcept
    {
        m_x = 0.0f;
    }

  private:
    const float m_sqrtDt;
    const float m_dt;
    float m_x;
    float m_sigma{0.f};

    std::mt19937 m_rng{std::random_device{}()};
    std::normal_distribution<float> m_normalDist;
};
}
