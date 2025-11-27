#pragma once

#include <vector>

namespace AbacDsp
{

class SincFilter
{
  public:
    struct InitParam
    {
        size_t increment;
        std::vector<float> coeffs;
    };

    explicit SincFilter(const InitParam& sincParam) noexcept
        : m_increment(sincParam.increment)
        , m_sizeInterleaved(1 + (sincParam.coeffs.size() - 1) / sincParam.increment)
        , m_halfCoeffWidth(sincParam.coeffs.size() - 2)
        , m_coeffs(sincParam.coeffs)
        , m_coeffsDelta(
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

    [[nodiscard]] size_t getBufferSize(const float maxRatio, const size_t channels) const noexcept
    {
        const auto width =
            static_cast<size_t>(3 * lrint(static_cast<float>(m_coeffs.size() / m_increment) * maxRatio) + 1);
        return 1 + channels * (1 + std::max<size_t>(width, 4096));
    }

    [[nodiscard]] size_t halfCoeffWidth() const noexcept
    {
        return m_halfCoeffWidth;
    }

    [[nodiscard]] size_t filterWidth() const noexcept
    {
        return m_sizeInterleaved;
    }

    [[nodiscard]] float getFractionFromIndex(const size_t idx, const float fraction) const noexcept
    {
        return m_coeffs[idx] + fraction * m_coeffsDelta[idx];
    }

    [[nodiscard]] size_t increment() const noexcept
    {
        return m_increment;
    }

    [[nodiscard]] float getFractionFromInterleaved(const size_t idx, const float fraction) const noexcept
    {
        return m_interleavedCoeffs[idx] + fraction * m_interleavedCoeffsDelta[idx];
    }

    [[nodiscard]] size_t getInterleavedIndex(size_t originalIndex) const noexcept
    {
        return (originalIndex % m_increment) * m_sizeInterleaved + originalIndex / m_increment;
    }

    template <size_t NumChannels>
    void processFixUp(const size_t items, const float* buffer, size_t bIdx, const float fraction, const size_t idx,
                      float* result) const noexcept
    {
        size_t ilvdIdx = getInterleavedIndex(idx);
        for (size_t i = 0; i < items; ++i)
        {
            const float iCoeff = getFractionFromInterleaved(ilvdIdx--, fraction);
            for (auto c = 0; c < NumChannels; ++c)
            {
                result[c] += iCoeff * buffer[bIdx++];
            }
        }
    }

    template <size_t NumChannels>
    void processFixDown(const size_t items, const float* buffer, size_t bIdx, const float fraction, const size_t idx,
                        float* result) const noexcept
    {
        size_t ilvdIdx = getInterleavedIndex(idx);
        for (size_t i = 0; i < items; ++i)
        {
            const float iCoeff = getFractionFromInterleaved(ilvdIdx--, fraction);
            for (auto c = NumChannels; c > 0; --c)
            {
                result[c - 1] += iCoeff * buffer[bIdx--];
            }
        }
    }

    template <size_t NumChannels, size_t DiscreteSteps, int32_t DataStep, int32_t term>
    void processFilterHalf(int32_t filterIdx, const float* buffer, size_t bIdx, const int32_t increment,
                           float* result) const noexcept
    {
        float intPart;
        const auto addFraction = std::modf(static_cast<float>(increment) / static_cast<float>(DiscreteSteps), &intPart);
        float fraction = (filterIdx & (DiscreteSteps - 1)) / static_cast<float>(DiscreteSteps);
        while (filterIdx > term)
        {
            const auto idx = filterIdx / static_cast<int32_t>(DiscreteSteps);
            const float iCoeff = getFractionFromIndex(static_cast<size_t>(idx), fraction);
            for (size_t c = 0; c < NumChannels; ++c)
            {
                result[c] += iCoeff * buffer[bIdx + c];
            }
            filterIdx -= increment;
            fraction -= addFraction;
            if (fraction < 0)
            {
                fraction += 1.f;
            }
            bIdx += static_cast<size_t>(DataStep) * NumChannels;
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
                const auto idxInterleaved = getInterleavedIndex(j);
                coeffs[idxInterleaved] = m_coeffs[j];
            }
        }
        return coeffs;
    }

    [[nodiscard]] std::vector<float> createInterleavedDeltas() const noexcept
    {
        std::vector coeffs(m_sizeInterleaved * m_increment, 0.f);
        for (size_t i = 0; i < m_increment; ++i)
        {
            for (size_t j = i; j < m_coeffsDelta.size(); j += m_increment)
            {
                const auto idxInterleaved = getInterleavedIndex(j);
                coeffs[idxInterleaved] = m_coeffsDelta[j];
            }
        }
        return coeffs;
    }

    const size_t m_increment{};
    const size_t m_sizeInterleaved{};
    const size_t m_halfCoeffWidth{};
    const std::vector<float> m_coeffs{};
    const std::vector<float> m_coeffsDelta{};
    std::vector<float> m_interleavedCoeffs{};
    std::vector<float> m_interleavedCoeffsDelta{};
};

}