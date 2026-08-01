#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class GenericImpl final : public EffectBase
{
  public:
    explicit GenericImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setOrder(const size_t value)
    {
        m_order = value;
    }
    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }
    void setWet(const float value)
    {
        m_wet = std::pow(10.f, value / 20.f);
    }
    void setStereoWidth(const float value)
    {
        m_stereoWidth = value;
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
    void setUniqueDelay(const bool value)
    {
        m_uniqueDelay = value;
    }
    void setDecay(const float value)
    {
        m_decay = value;
    }
    void setAllPassUp(const float value)
    {
        m_allPassUp = value;
    }
    void setAllPassDown(const float value)
    {
        m_allPassDown = value;
    }
    void setLowPass(const float value)
    {
        m_lowPass = value;
    }
    void setLowPassCount(const size_t value)
    {
        m_lowPassCount = value;
    }
    void setHighPass(const float value)
    {
        m_highPass = value;
    }
    void setHighPassCount(const size_t value)
    {
        m_highPassCount = value;
    }
    void setModulationDepth(const float value)
    {
        m_modulationDepth = value;
    }
    void setModulationSpeed(const float value)
    {
        m_modulationSpeed = value;
    }
    void setReversePitch(const bool value)
    {
        m_reversePitch = value;
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
    size_t m_order{};
    float m_dry{};
    float m_wet{};
    float m_stereoWidth{};
    float m_baseSize{};
    float m_sizeFactor{};
    float m_bulge{};
    bool m_uniqueDelay{};
    float m_decay{};
    float m_allPassUp{};
    float m_allPassDown{};
    float m_lowPass{};
    size_t m_lowPassCount{};
    float m_highPass{};
    size_t m_highPassCount{};
    float m_modulationDepth{};
    float m_modulationSpeed{};
    bool m_reversePitch{};
    float m_pitchStrength{};
    float m_pitch1Inplace{};
    float m_pitch2Inplace{};
};