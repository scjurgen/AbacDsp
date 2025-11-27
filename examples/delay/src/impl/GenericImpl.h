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
        m_visualWavedata.resize(6000);
    }
    void setGain(const float value)
    {
        m_gain = std::pow(10.f, value / 20.f);
    }
    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }
    void setWet(const float value)
    {
        m_wet = std::pow(10.f, value / 20.f);
    }
    void setTimeInMs(const float value)
    {
        m_timeInMs = value;
    }
    void setFeedback(const float value)
    {
        m_feedback = value;
    }
    void setLowPass(const float value)
    {
        m_lowPass = value;
    }
    void setHighPass(const float value)
    {
        m_highPass = value;
    }
    void setAllPass(const float value)
    {
        m_allPass = value;
    }
    void setModDepth(const float value)
    {
        m_modDepth = value;
    }
    void setModSpeed(const float value)
    {
        m_modSpeed = value;
    }
    void setModVariance(const float value)
    {
        m_modVariance = value;
    }
    void setModDrift(const float value)
    {
        m_modDrift = value;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = in(i, 0);
            out(i, 1) = in(i, 1);
        }

        m_visualWavedata[m_currentSample] = out(0, 0) * 0.5f;
        m_currentSample++;
        if (m_currentSample >= m_visualWavedata.size())
        {
            m_currentSample = 0;
        }
    }
    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata.resize(m_visualWavedata.size());
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    float m_gain{};
    float m_dry{};
    float m_wet{};
    float m_timeInMs{};
    float m_feedback{};
    float m_lowPass{};
    float m_highPass{};
    float m_allPass{};
    float m_modDepth{};
    float m_modSpeed{};
    float m_modVariance{};
    float m_modDrift{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};