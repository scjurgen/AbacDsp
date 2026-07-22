#pragma once

#include <array>
#include <cmath>
#include <cstddef>

#include "BlockProcessorBase.h"
#include "Spectral/PhaseVocoderPitcher.h"

namespace AbacDsp::BlockProc
{

/**
 * @brief Realtime mono pitch shifter built directly on the streaming
 * PhaseVocoderPitcher: no time-stretch/resample stage and no seeking, so the
 * only latency is the vocoder's own fixed analysis window
 * (PhaseVocoderPitcher::latencySamples()).
 */
template <size_t BlockSize>
class PhaseVocoderPitch final : public BlockProcessorBase<BlockSize>
{
  public:
    explicit PhaseVocoderPitch(const float sampleRate)
        : m_pvp(sampleRate)
    {
    }

    PhaseVocoderPitch(const PhaseVocoderPitch&) = delete;
    PhaseVocoderPitch& operator=(const PhaseVocoderPitch&) = delete;
    PhaseVocoderPitch(PhaseVocoderPitch&&) = delete;
    PhaseVocoderPitch& operator=(PhaseVocoderPitch&&) = delete;
    ~PhaseVocoderPitch() override = default;

    void setPitch(const float semitones) noexcept
    {
        m_pvp.setPitchRatio(std::pow(2.0f, semitones / 12.0f));
    }

    void setPitchMix(const float value) noexcept
    {
        m_mixPitch = value;
        m_mixPlain = 1.0f - value;
    }

    void process(std::array<float, BlockSize>& blk) noexcept override
    {
        std::array<float, BlockSize> wet{};
        m_pvp.processBlock(blk.data(), wet.data(), BlockSize);

        for (size_t i = 0; i < BlockSize; ++i)
        {
            blk[i] = wet[i] * m_mixPitch + blk[i] * m_mixPlain;
        }
    }

    void reset() noexcept override {}

  private:
    PhaseVocoderPitcher m_pvp;
    float m_mixPitch{0.5f};
    float m_mixPlain{0.5f};
};

}
