#pragma once

#include <cmath>
#include <cstdint>

#include "Numbers/Random.h"

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Ornstein-Uhlenbeck process with the exact transition over a step, for any step rate: a random walk pulled
 * back towards mu, dx = theta (mu - x) dt + sigma_w dW.
 *
 * Power spectrum flat below the reversion rate theta / (2 pi), falling 6 dB per octave above it; the stationary
 * distribution is Gaussian with mean mu and standard deviation sigma_w / sqrt(2 theta), where sigma_w = sigma / 2.33
 * is the noise scale. theta = 20 sigma + 1 and mu = sigma follow from sigma, so one control moves amplitude, centre
 * and reversion rate together and the process cannot be set to a state where it stops reverting.
 *
 * Each step applies x' = mu + (x - mu) e^(-theta dt) + sigma_w sqrt((1 - e^(-2 theta dt)) / (2 theta)) N(0, 1) with
 * dt = 1 / stepRate. An Euler-Maruyama step agrees for small theta dt but overshoots above 1 and diverges above 2,
 * which happens when the process is stepped a few times per second; the exact update is stable and statistically
 * correct at any rate. Deterministic per seed through Random; real-time safe.
 * @see https://en.wikipedia.org/wiki/Ornstein%E2%80%93Uhlenbeck_process
 * @see D. T. Gillespie, "Exact numerical simulation of the Ornstein-Uhlenbeck process and its integral",
 * Physical Review E 54(2), 1996
 */
class ExactOrnsteinUhlenbeck
{
  public:
    /// Ratio between sigma and the standard deviation of the driving noise.
    static constexpr float kNoiseDivisor{2.33f};

    explicit ExactOrnsteinUhlenbeck(const float stepRate, const uint64_t seedValue = Random::kDefaultSeed) noexcept
        : m_random(seedValue)
    {
        setStepRate(stepRate);
    }

    /// Steps per second; dt = 1 / stepRate.
    void setStepRate(const float stepRate) noexcept
    {
        m_dt = 1.0f / stepRate;
        updateTransition();
    }

    void setSigma(const float sigma) noexcept
    {
        m_sigma = sigma;
        m_theta = sigma * 20.0f + 1.0f;
        m_mu = sigma;
        updateTransition();
    }

    [[nodiscard]] float getMu() const noexcept
    {
        return m_mu;
    }

    [[nodiscard]] float getTheta() const noexcept
    {
        return m_theta;
    }

    /// Standard deviation of the stationary distribution, sigma_w / sqrt(2 theta).
    [[nodiscard]] float getStationaryDeviation() const noexcept
    {
        return m_sigma / kNoiseDivisor / std::sqrt(2.0f * m_theta);
    }

    void seed(const uint64_t seedValue) noexcept
    {
        m_random.seed(seedValue);
    }

    /// Advances by dt and returns the new value.
    [[nodiscard]] float step() noexcept
    {
        m_x = m_mu + (m_x - m_mu) * m_decay + m_noiseGain * m_random.gaussian();
        return m_x;
    }

    [[nodiscard]] float getValue() const noexcept
    {
        return m_x;
    }

    /// Sets the current value, e.g. to mu to start in the stationary centre without a transient.
    void setValue(const float value) noexcept
    {
        m_x = value;
    }

    /// Sets the value to 0; the random sequence continues.
    void reset() noexcept
    {
        m_x = 0.0f;
    }

    /// Sets the value to 0 and restarts the random sequence from a seed.
    void reset(const uint64_t seedValue) noexcept
    {
        m_x = 0.0f;
        m_random.seed(seedValue);
    }

  private:
    void updateTransition() noexcept
    {
        m_decay = std::exp(-m_theta * m_dt);
        m_noiseGain = m_sigma / kNoiseDivisor * std::sqrt((1.0f - m_decay * m_decay) / (2.0f * m_theta));
    }

    Random m_random;
    float m_dt{1.0f};
    float m_sigma{0.0f};
    float m_theta{1.0f};
    float m_mu{0.0f};
    float m_decay{0.0f};
    float m_noiseGain{0.0f};
    float m_x{0.0f};
};

}
