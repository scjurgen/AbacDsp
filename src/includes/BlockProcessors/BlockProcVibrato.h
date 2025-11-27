#pragma once

#include <array>
#include <cmath>

#include "Modulation/Wow.h"
#include "BlockProcessorBase.h"

namespace AbacDsp::BlockProc
{

template <size_t BlockSize>
class Vibrato final : public BlockProcessorBase<BlockSize>
{
  public:
    static constexpr size_t DelayBufferSize = 4800;
    static constexpr size_t MaxDepth{(DelayBufferSize - 100) / 2};
    explicit Vibrato(const float sampleRate)
        : m_wow(sampleRate)
        , m_sampleRate(sampleRate)
    {
    }

    void setModSpeed(const float v) noexcept
    {
        m_wow.setRate(v);
    }
    void setModDepth(const float v) noexcept
    {
        m_wow.setDepth(v);
    }
    void setVariance(const float v) noexcept
    {
        m_wow.setVariance(v);
    }
    void setDrift(const float v) noexcept
    {
        m_wow.setDrift(v);
    }

    void process(std::array<float, BlockSize>& blk) noexcept override
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_buffer[m_writePos] = blk[i];

            const float depth = std::clamp(m_wow.stepNormalized(), -0.95f, 0.95f) * MaxDepth;

            const float readPos = static_cast<float>(m_writePos) + depth - MaxDepth;
            const float wrapped = std::fmod(readPos + m_buffer.size(), static_cast<float>(m_buffer.size()));

            const size_t idx0 = static_cast<size_t>(wrapped);
            const size_t idx1 = (idx0 + 1) % m_buffer.size();
            const float frac = wrapped - static_cast<float>(idx0);

            blk[i] = m_buffer[idx0] * (1.0f - frac) + m_buffer[idx1] * frac;
            m_writePos = (m_writePos + 1) % m_buffer.size();
        }
    }

    void reset() noexcept override
    {
        m_buffer.fill(0.0f);
        m_writePos = 0;
        m_readPos = 0;
    }

  private:
    const float m_sampleRate;

    Wow m_wow;
    std::array<float, DelayBufferSize> m_buffer{};
    size_t m_writePos{0};
    size_t m_readPos{0};
};
}
