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
        m_visualWavedata.resize(6000);
    }
    void setFeedGain(const float value)
    {
        m_feedGain = std::pow(10.f, value / 20.f);
    }
    void setTapeSpeed(const float value)
    {
        m_tapeSpeed = value;
    }
    void setWow(const float value)
    {
        m_wow = value;
    }
    void setHysteresis(const float value)
    {
        m_hysteresis = value;
    }
    void setSaturation(const float value)
    {
        m_saturation = value;
    }
    void setNoiseFloor(const float value)
    {
        m_noiseFloor = value;
    }
    void setNoiseDistribution(const float value)
    {
        m_noiseDistribution = value;
    }
    void setDelayTime1(const float value)
    {
        m_delayTime1 = value;
    }
    void setDelayTime2(const float value)
    {
        m_delayTime2 = value;
    }
    void setDelayTime3(const float value)
    {
        m_delayTime3 = value;
    }
    void setDelayTime4(const float value)
    {
        m_delayTime4 = value;
    }
    void setDelayLevel1(const float value)
    {
        m_delayLevel1 = std::pow(10.f, value / 20.f);
    }
    void setDelayLevel2(const float value)
    {
        m_delayLevel2 = std::pow(10.f, value / 20.f);
    }
    void setDelayLevel3(const float value)
    {
        m_delayLevel3 = std::pow(10.f, value / 20.f);
    }
    void setDelayLevel4(const float value)
    {
        m_delayLevel4 = std::pow(10.f, value / 20.f);
    }
    void setFeedback1(const float value)
    {
        m_feedback1 = value;
    }
    void setFeedback2(const float value)
    {
        m_feedback2 = value;
    }
    void setFeedback3(const float value)
    {
        m_feedback3 = value;
    }
    void setFeedback4(const float value)
    {
        m_feedback4 = value;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = in(i, 0);
            out(i, 1) = in(i, 1);
        }

        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            m_currentSample++;
            if (m_currentSample >= m_visualWavedata.size())
            {
                m_currentSample = 0;
            }
        }
    }
    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata.resize(m_visualWavedata.size());
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    float m_feedGain{};
    float m_tapeSpeed{};
    float m_wow{};
    float m_hysteresis{};
    float m_saturation{};
    float m_noiseFloor{};
    float m_noiseDistribution{};
    float m_delayTime1{};
    float m_delayTime2{};
    float m_delayTime3{};
    float m_delayTime4{};
    float m_delayLevel1{};
    float m_delayLevel2{};
    float m_delayLevel3{};
    float m_delayLevel4{};
    float m_feedback1{};
    float m_feedback2{};
    float m_feedback3{};
    float m_feedback4{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};