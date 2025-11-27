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
    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }
    void setWet(const float value)
    {
        m_wet = std::pow(10.f, value / 20.f);
    }
    void setBaseSize(const float value)
    {
        m_baseSize = value;
    }
    void setSizeFactor(const float value)
    {
        m_sizeFactor = value;
    }
    void setBulge(const float value)
    {
        m_bulge = value;
    }
    void setDecay(const float value)
    {
        m_decay = value;
    }
    void setLowPass(const float value)
    {
        m_lowPass = value;
    }
    void setHighPass(const float value)
    {
        m_highPass = value;
    }
    void setModulationDepth(const float value)
    {
        m_modulationDepth = value;
    }
    void setModulationSpeed(const float value)
    {
        m_modulationSpeed = value;
    }
    void setPitchStrength(const float value)
    {
        m_pitchStrength = value;
    }
    void setPitch1Inplace(const float value)
    {
        m_pitch1Inplace = value;
    }
    void setPitch2Inplace(const float value)
    {
        m_pitch2Inplace = value;
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
    float m_dry{};
    float m_wet{};
    float m_baseSize{};
    float m_sizeFactor{};
    float m_bulge{};
    float m_decay{};
    float m_lowPass{};
    float m_highPass{};
    float m_modulationDepth{};
    float m_modulationSpeed{};
    float m_pitchStrength{};
    float m_pitch1Inplace{};
    float m_pitch2Inplace{};
};