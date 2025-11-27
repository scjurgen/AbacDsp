#pragma once

#include "EffectBase.h"
#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"

#include <cassert>
#include <cstdint>
#include <cmath>
#include <functional>

template <size_t BlockSize>
class GenericImpl final : public EffectBase
{
  public:
    GenericImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setOnOff(const bool value)
    {
        m_onOff = value;
    }
    void setInput(const float value)
    {
        m_input = std::pow(10.f, value / 20.f);
    }
    void setModulationDepth(const float value)
    {
        m_modulationDepth = value;
    }
    void setMix(const float value)
    {
        m_mix = value;
    }
    void setDensity(const float value)
    {
        m_density = value;
    }
    void setThreshold(const float value)
    {
        m_threshold = value;
    }
    void setKnee(const float value)
    {
        m_knee = std::pow(10.f, value / 20.f);
    }
    void setDropIt(const size_t value)
    {
        m_dropIt = value;
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
    size_t m_onOff{};
    float m_input{};
    float m_modulationDepth{};
    float m_mix{};
    float m_density{};
    float m_threshold{};
    float m_knee{};
    bool m_dropIt{};
};