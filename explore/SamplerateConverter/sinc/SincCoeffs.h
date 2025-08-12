#pragma once

#include <iostream>

#include <ostream>
#include <vector>

class SincCoeffs
{
  public:
    struct InitParam
    {
        int increment;
        std::vector<float> coeffs;
    };

    SincCoeffs(const InitParam& sincParam) noexcept
        : m_increment(sincParam.increment)
        , m_coeffs(sincParam.coeffs)
        , m_coeffsDelta(
              [this]()
              {
                  std::vector<float> delta(m_coeffs.size() - 1);
                  for (size_t idx = 0; idx < m_coeffs.size() - 1; ++idx)
                  {
                      delta[idx] = m_coeffs[idx + 1] - m_coeffs[idx];
                  }
                  return delta;
              }())
    {
    }

    [[nodiscard]] int getBufferSize(const float maxRatio, const int channels) const noexcept
    {
        auto width = 3 * lrint(m_coeffs.size() / 2 / m_increment * maxRatio) + 1;
        return 1 + channels * (1 + std::max<int>(width, 4096));
    }

    [[nodiscard]] int halfCoeffWidth() const noexcept
    {
        return m_coeffs.size() / 2 - 1;
    }

    [[nodiscard]] float getFractionFromIndex(const int idx, const float fraction) const noexcept
    {
        int m_idx = idx + halfCoeffWidth();
        return m_coeffs[m_idx] + fraction * (m_coeffs[m_idx + 1] - m_coeffs[m_idx]);
    }

    [[nodiscard]] int increment() const noexcept
    {
        return m_increment;
    }

  private:
    const int m_increment{};
    const std::vector<float> m_coeffs{};
    const std::vector<float> m_coeffsDelta{};
};

extern const std::vector<SincCoeffs> sincCoeffs;