#pragma once

#include <array>
#include <cmath>

#include "BlockProcessorBase.h"
#include "Delays/PitchFadeWindowDelay.h"
#include "Filters/OnePoleFilter.h"

namespace AbacDsp::BlockProc
{

template <size_t BlockSize>
class Pitch final : public BlockProcessorBase<BlockSize>
{
  public:
    static constexpr size_t DelayBufferSize = 4800;

    explicit Pitch(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_lp(sampleRate)
    {
        m_pdl.setSize(DelayBufferSize / 2);
        m_pdl.setFadeTime(DelayBufferSize / 2);
        m_lp.setCutoff(12000.f);
    }

    void setPitch(const float semiTones) noexcept
    {
        m_pdl.setPitch(semiTones);
    }

    void setPitchMix(const float value)
    {
        m_mixPitch = value;
        m_mixPlain = 1 - value;
    }
    void setReverse(const bool reverse) noexcept
    {
        m_pdl.setReverse(reverse);
    }

    void process(std::array<float, BlockSize>& blk) noexcept override
    {
        std::array<float, BlockSize> tmp;
        m_pdl.processBlock(blk.data(), tmp.data(), BlockSize);
        m_lp.processBlock(tmp.data(), BlockSize);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            blk[i] = tmp[i] * m_mixPitch + blk[i] * m_mixPlain;
        }
    }

    void reset() noexcept override {}

  private:
    float m_sampleRate;
    float m_mixPitch{0.5f};
    float m_mixPlain{0.5f};
    PitchFadeWindowDelay<DelayBufferSize> m_pdl;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> m_lp;
};
}
