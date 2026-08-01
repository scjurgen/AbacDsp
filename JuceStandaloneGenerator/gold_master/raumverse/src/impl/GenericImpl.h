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
    void setLowSize(const float value)
    {
        m_lowSize = value;
    }
    void setHighSize(const float value)
    {
        m_highSize = value;
    }
    void setUniqueDelay(const bool value)
    {
        m_uniqueDelay = value;
    }
    void setBulge(const float value)
    {
        m_bulge = value;
    }
    void setDecayLow(const float value)
    {
        m_decayLow = value;
    }
    void setCrossOver(const float value)
    {
        m_crossOver = value;
    }
    void setDecayHigh(const float value)
    {
        m_decayHigh = value;
    }
    void setAllPassUp(const float value)
    {
        m_allPassUp = value;
    }
    void setAllPassDown(const float value)
    {
        m_allPassDown = value;
    }
    void setAllPassCount(const size_t value)
    {
        m_allPassCount = value;
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
    void setModulationCount(const size_t value)
    {
        m_modulationCount = value;
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
    void setPitchSize(const float value)
    {
        m_pitchSize = value;
    }
    void setPitch1(const float value)
    {
        m_pitch1 = value;
    }
    void setPitch2(const float value)
    {
        m_pitch2 = value;
    }
    void setPitch3(const float value)
    {
        m_pitch3 = value;
    }
    void setPitch4(const float value)
    {
        m_pitch4 = value;
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
    float m_lowSize{};
    float m_highSize{};
    bool m_uniqueDelay{};
    float m_bulge{};
    float m_decayLow{};
    float m_crossOver{};
    float m_decayHigh{};
    float m_allPassUp{};
    float m_allPassDown{};
    size_t m_allPassCount{};
    float m_lowPass{};
    size_t m_lowPassCount{};
    float m_highPass{};
    size_t m_highPassCount{};
    float m_modulationDepth{};
    float m_modulationSpeed{};
    size_t m_modulationCount{};
    bool m_reversePitch{};
    float m_pitchStrength{};
    float m_pitch1Inplace{};
    float m_pitch2Inplace{};
    float m_pitchSize{};
    float m_pitch1{};
    float m_pitch2{};
    float m_pitch3{};
    float m_pitch4{};
};