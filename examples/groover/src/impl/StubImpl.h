#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class StubImpl final : public EffectBase
{
  public:
    explicit StubImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setPlay(const bool value)
    {
        m_play = value;
    }
    void setHostSync(const bool value)
    {
        m_hostSync = value;
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setGrooveVariation(const float value)
    {
        m_grooveVariation = value;
    }
    void setOutputLevel(const float value)
    {
        m_outputLevel = std::pow(10.f, value / 20.f);
    }
    void setInputGain(const float value)
    {
        m_inputGain = std::pow(10.f, value / 20.f);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = in(i, 0);
            out(i, 1) = in(i, 1);
        }
    }

  private:
    bool m_play{};
    bool m_hostSync{};
    float m_bpm{};
    float m_grooveVariation{};
    float m_outputLevel{};
    float m_inputGain{};
};