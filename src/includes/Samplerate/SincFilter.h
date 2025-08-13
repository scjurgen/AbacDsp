#pragma once

#include <vector>

namespace AbacDsp
{
/**
 * @brief High-performance, polyphase FIR sinc filter for sample-rate conversion and fractional delay.
 *
 * Implements a windowed-sinc interpolation kernel with precomputed coefficient deltas
 * for fast linear interpolation at arbitrary fractional sample positions.
 * Coefficients are stored in an interleaved phase layout for cache-friendly  access.
 * Supports forward/reverse traversal and compile-time channel configuration.
 */
class SincFilter
{
  public:
    /**
     * @brief Parameters for sinc filter initialization.
     * @param increment Stride between phases in the coefficient table.
     * @param coeffs FIR kernel coefficients (half of a symmetric sinc window).
     */
    struct InitParam
    {
        int increment;
        std::vector<float> coeffs;
    };

    explicit SincFilter(const InitParam& sincParam) noexcept
        : m_increment(sincParam.increment)
        , m_sizeInterleaved(1 + (sincParam.coeffs.size() - 1) / sincParam.increment)
        , m_halfCoeffWidth(sincParam.coeffs.size() - 2)
        , m_coeffs(sincParam.coeffs)
        , m_coeffsDelta(
              // Precompute deltas for fractional interpolation and interleaved phase tables.
              [this]()
              {
                  std::vector<float> delta(m_coeffs.size());
                  for (size_t idx = 0; idx < m_coeffs.size() - 1; ++idx)
                  {
                      delta[idx] = m_coeffs[idx + 1] - m_coeffs[idx];
                  }
                  delta[m_coeffs.size() - 1] = 0.f - m_coeffs[m_coeffs.size() - 1];
                  return delta;
              }())
        , m_interleavedCoeffs(createInterleavedCoeffs())
        , m_interleavedCoeffsDelta(createInterleavedDeltas())
    {
        if (const auto delta = m_interleavedCoeffs.size() % m_increment; delta != 0)
        {
            for (size_t i = 0; i < m_increment - delta; ++i)
            {
                m_interleavedCoeffs.push_back(0.f);
                m_interleavedCoeffsDelta.push_back(0.f);
            }
        }
    }

    [[nodiscard]] int getBufferSize(const float maxRatio, const int channels) const noexcept
    {
        auto width = 3 * lrint(m_coeffs.size() / m_increment * maxRatio) + 1;
        return 1 + channels * (1 + std::max<int>(width, 4096));
    }

    [[nodiscard]] int halfCoeffWidth() const noexcept
    {
        return m_halfCoeffWidth;
    }

    [[nodiscard]] int filterWidth() const noexcept
    {
        return m_sizeInterleaved;
    }

    [[nodiscard]] float getFractionFromIndex(const int idx, const float fraction) const noexcept
    {
        return m_coeffs[idx] + fraction * m_coeffsDelta[idx];
    }

    [[nodiscard]] int increment() const noexcept
    {
        return m_increment;
    }

    [[nodiscard]] float getFractionFromInterleaved(const int idx, const float fraction) const noexcept
    {
        return m_interleavedCoeffs[idx] + fraction * m_interleavedCoeffsDelta[idx];
    }

    [[nodiscard]] size_t getInterleavedIndex(size_t originalIndex) const noexcept
    {
        return (originalIndex % m_increment) * m_sizeInterleaved + originalIndex / m_increment;
    }

    template <size_t NumChannels>
    void processFixUp(const size_t items, const float* buffer, const float fraction, const size_t idx,
                      float* result) const noexcept
    {
        size_t ilvdIdx = getInterleavedIndex(idx);
        for (size_t i = 0; i < items; ++i)
        {
            const float iCoeff = getFractionFromInterleaved(ilvdIdx--, fraction);
            for (auto c = 0; c < NumChannels; ++c)
            {
                result[c] += iCoeff * *buffer++;
            }
        }
    }

    template <size_t NumChannels>
    void processFixDown(const size_t items, const float* buffer, const float fraction, const size_t idx,
                        float* result) const noexcept
    {
        size_t ilvdIdx = getInterleavedIndex(idx);
        for (size_t i = 0; i < items; ++i)
        {
            const float iCoeff = getFractionFromInterleaved(ilvdIdx--, fraction);
            for (auto c = NumChannels; c > 0; --c)
            {
                result[c - 1] += iCoeff * *buffer--;
            }
        }
    }

    template <size_t NumChannels, size_t DiscreteSteps, ssize_t DataStep, ssize_t term>
    void processFilterHalf(int32_t filterIdx, float* buffer, int32_t increment, float* result) const noexcept
    {
        float intPart;
        const auto addFraction = std::modf(increment / static_cast<float>(DiscreteSteps), &intPart);
        float fraction = (filterIdx & (DiscreteSteps - 1)) / static_cast<float>(DiscreteSteps);
        while (filterIdx > term)
        {
            const size_t idx = filterIdx / DiscreteSteps;
            const float iCoeff = getFractionFromIndex(idx, fraction);
            for (auto c = 0; c < NumChannels; ++c)
            {
                result[c] += iCoeff * buffer[c];
            }
            filterIdx -= increment;
            fraction -= addFraction;
            if (fraction < 0)
            {
                fraction += 1.f;
            }
            buffer += DataStep * NumChannels;
        }
    }

  private:
    [[nodiscard]] std::vector<float> createInterleavedCoeffs() const noexcept
    {
        std::vector coeffs(m_sizeInterleaved * m_increment, 0.f);
        for (size_t i = 0; i < m_increment; ++i)
        {
            for (size_t j = i; j < m_coeffs.size(); j += m_increment)
            {
                auto idxInterleaved = getInterleavedIndex(j);
                coeffs[idxInterleaved] = m_coeffs[j];
            }
        }
        return coeffs;
    }

    [[nodiscard]] std::vector<float> createInterleavedDeltas() const noexcept
    {
        std::vector coeffs(m_sizeInterleaved * m_increment, 0.f);
        for (int i = 0; i < m_increment; ++i)
        {
            for (size_t j = i; j < m_coeffsDelta.size(); j += m_increment)
            {
                auto idxInterleaved = getInterleavedIndex(j);
                coeffs[idxInterleaved] = m_coeffsDelta[j];
            }
        }
        return coeffs;
    }

    const int m_increment{};
    const int m_sizeInterleaved{};
    const size_t m_halfCoeffWidth{};
    const std::vector<float> m_coeffs{};
    const std::vector<float> m_coeffsDelta{};
    std::vector<float> m_interleavedCoeffs{};
    std::vector<float> m_interleavedCoeffsDelta{};
};

}