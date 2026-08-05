#pragma once

#include <numbers>

#include "BlockProcessorBase.h"

namespace AbacDsp
{
namespace BlockProc
{
/**
 * @ingroup blockprocessors
 * @brief Block-wise one-pole lowpass. Filter theory: see Filters/OnePoleFilter.h.
 */
template <size_t BlockSize>
class Lowpass final : public BlockProcessorBase<BlockSize>
{
  public:
    explicit Lowpass(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_coeff(1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * 1000.f / sampleRate))
    {
    }

    void process(std::array<float, BlockSize>& blk) noexcept override
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_state += m_coeff * (blk[i] - m_state);
            blk[i] = m_state;
        }
    }

    void setCutoff(const float cutoffFreq) noexcept
    {
        m_coeff = 1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * cutoffFreq / m_sampleRate);
    }

  private:
    const float m_sampleRate;
    float m_coeff;
    float m_state{};
};
}
}