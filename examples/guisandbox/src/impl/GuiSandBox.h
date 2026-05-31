#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class GuiSandBox final : public EffectBase
{
  public:
    GuiSandBox(const float sampleRate)
        : EffectBase(sampleRate)
        , m_visualWavedata(6000, 0)
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
    size_t m_onOff{};
    float m_input{};
    float m_modulationDepth{};
    float m_mix{};
    float m_density{};
    float m_threshold{};
    float m_knee{};
    bool m_dropIt{};
    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};