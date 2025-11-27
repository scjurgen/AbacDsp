#pragma once

#include "BlockProcessorBase.h"

#include <numbers>

namespace AbacDsp
{
namespace BlockProc
{
template <size_t BlockSize>
class Highpass final : public BlockProcessorBase<BlockSize>
{
  public:
    explicit Highpass(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_coeff(1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * 1000.f / sampleRate))
        , m_state(0.0f)
    {
    }

    void process(std::array<float, BlockSize>& blk) noexcept override
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_state += m_coeff * (blk[i] - m_state);
            blk[i] = m_state - blk[i]; // leaky highpass
        }
    }

    void setCutoff(const float cutoffFreq)
    {
        m_coeff = 1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * cutoffFreq / m_sampleRate);
    }

  private:
    float m_sampleRate;
    float m_coeff;
    float m_state;
};
}
}