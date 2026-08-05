#pragma once

#include <vector>

namespace AbacDsp
{

/**
 * @ingroup filters
 * @brief Windowed-sinc interpolation kernel, oversampled and stored for phase-major access.
 *
 * The kernel is tabulated at `increment` subsample phases and read back with
 * linear interpolation between neighbouring entries, which is what lets one
 * table serve any fractional delay. First differences are precomputed into a
 * parallel array so that interpolation is a single multiply-add rather than a
 * second, unpredictable table fetch.
 *
 * The table is also held in an interleaved copy where all coefficients sharing
 * a phase sit contiguously. A convolution pass runs at one fixed phase, so this
 * layout turns a stride-`increment` walk into a sequential one. Both copies are
 * padded to a whole multiple of `increment` so no pass can run off the end.
 *
 * Coefficient sets are generated offline; see documentation/Filters/SincFilterDesign.
 * @see https://ccrma.stanford.edu/~jos/resample/
 */
class SincFilter
{
  public:
    /// @brief Number of subsample phases the kernel was tabulated at, and the kernel itself.
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

    /// @brief Delay-line size that keeps the kernel in range at ratios up to maxRatio.
    /// Heuristic: three kernel widths scaled by the ratio, with a 4096-sample floor.
    [[nodiscard]] size_t getBufferSize(const float maxRatio, const size_t channels) const noexcept
    {
        const auto width = static_cast<size_t>(
            3 * lrint(static_cast<float>(m_coeffs.size()) / static_cast<float>(m_increment) * maxRatio) + 1);
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

    /// @brief Maps a kernel index to its position in the phase-major copy.
    /// Phase (index % increment) selects the block, index / increment the offset within it.
    [[nodiscard]] size_t getInterleavedIndex(size_t originalIndex) const noexcept
    {
        return (originalIndex % m_increment) * m_sizeInterleaved + originalIndex / m_increment;
    }

    /// @brief Convolves one kernel half at a fixed phase, walking the buffer forwards.
    /// Kernel indices descend through the phase-major copy, so the coefficient reads stay sequential.
    template <size_t NumChannels>
    void processFixUp(const size_t items, const float* buffer, size_t bIdx, const float fraction, const size_t idx,
                      float* result) const noexcept
    {
        size_t ilvdIdx = getInterleavedIndex(idx);
        for (size_t i = 0; i < items; ++i)
        {
            const float iCoeff = getFractionFromInterleaved(ilvdIdx--, fraction);
            for (size_t c = 0; c < NumChannels; ++c)
            {
                result[c] += iCoeff * buffer[bIdx++];
            }
        }
    }

    /// @brief Mirror of processFixUp() for the other kernel half, walking the buffer backwards.
    /// The kernel is symmetric, so the two halves share coefficients and differ only in buffer direction.
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

    /**
     * @brief Convolves one kernel half while the phase advances, as it must when the rates differ.
     *
     * Walks the kernel in fixed-point: filterIdx counts in units of 1/DiscreteSteps, so the
     * integer part indexes the table and the low bits give the interpolation fraction. The
     * fraction is then carried by subtraction and wrapped, avoiding a divide per tap.
     * DiscreteSteps must be a power of two for the mask on filterIdx to be valid.
     */
    template <size_t NumChannels, size_t DiscreteSteps, int32_t DataStep, int32_t term>
    void processFilterHalf(int32_t filterIdx, const float* buffer, size_t bIdx, const int32_t increment,
                           float* result) const noexcept
    {
        float intPart{};
        const auto addFraction = std::modf(static_cast<float>(increment) / static_cast<float>(DiscreteSteps), &intPart);
        float fraction =
            static_cast<float>(filterIdx & static_cast<int32_t>(DiscreteSteps - 1)) / static_cast<float>(DiscreteSteps);
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