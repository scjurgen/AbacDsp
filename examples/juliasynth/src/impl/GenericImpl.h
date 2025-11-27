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
    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }
    void setPower(const float value)
    {
        m_power = value;
    }
    void setCx(const float value)
    {
        m_cx = value;
    }
    void setCy(const float value)
    {
        m_cy = value;
    }
    void setJx(const float value)
    {
        m_jx = value;
    }
    void setJy(const float value)
    {
        m_jy = value;
    }
    void setJrx(const float value)
    {
        m_jrx = value;
    }
    void setJry(const float value)
    {
        m_jry = value;
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
    float m_vol{};
    float m_power{};
    float m_cx{};
    float m_cy{};
    float m_jx{};
    float m_jy{};
    float m_jrx{};
    float m_jry{};
};