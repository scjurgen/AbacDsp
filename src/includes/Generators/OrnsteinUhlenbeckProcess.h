#pragma once

#include <cmath>
#include <random>

namespace AbacDsp
{

class OrnsteinUhlenbeckProcess
{
  public:
    explicit OrnsteinUhlenbeckProcess(const float sampleRate)
        : m_dt(1.f / sampleRate)
        , m_sqrtDt(std::sqrt(m_dt))
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
        m_theta = m_sigma * 20.0f + 1.0f;
        m_mu = m_sigma;
    }

    // dx = theta(mu - x)dt + sigma*sqrt(dt)*dW
    [[nodiscard]] float step() noexcept
    {
        const auto dW = m_normalDist(m_rng);
        m_x += m_theta * (m_mu - m_x) * m_dt + m_sigma * m_sqrtDt * dW;
        return m_x;
    }

    void reset() noexcept
    {
        m_x = 0.0f;
    }

    void reset(const std::mt19937::result_type seed) noexcept
    {
        m_x = 0.0f;
        m_rng.seed(seed);
        m_normalDist.reset();
    }

  private:
    const float m_dt;
    const float m_sqrtDt;
    float m_sigma{0.0f};
    float m_theta{1.0f};
    float m_mu{0.0f};
    float m_x{0.0f};

    std::mt19937 m_rng{std::random_device{}()};
    std::normal_distribution<float> m_normalDist;
};

}
