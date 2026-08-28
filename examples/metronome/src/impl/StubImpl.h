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
        m_visualWavedata.resize(6000);
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setDropBars(const size_t value)
    {
        m_dropBars = value;
    }
    void setMetroVolume(const float value)
    {
        m_metroVolume = std::pow(10.f, value / 20.f);
    }
    void setInputVolume(const float value)
    {
        m_inputVolume = std::pow(10.f, value / 20.f);
    }
    void setSubVolume(const float value)
    {
        m_subVolume = std::pow(10.f, value / 20.f);
    }
    void setOnOff(const bool value)
    {
        m_onOff = value;
    }
    void setHostSync(const bool value)
    {
        m_hostSync = value;
    }
    void setAnalysisMode(const bool value)
    {
        m_analysisMode = value;
    }
    void setPreset(const size_t value)
    {
        m_preset = value;
    }
    void setSwingRatio(const float value)
    {
        m_swingRatio = value;
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
    float m_bpm{};
    size_t m_dropBars{};
    float m_metroVolume{};
    float m_inputVolume{};
    float m_subVolume{};
    bool m_onOff{};
    bool m_hostSync{};
    bool m_analysisMode{};
    size_t m_preset{};
    float m_swingRatio{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};