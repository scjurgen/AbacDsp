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
    void setWave(const size_t value)
    {
        m_wave = value;
    }
    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }
    void setFrequency(const float value)
    {
        m_frequency = value;
    }
    void setDeformType(const size_t value)
    {
        m_deformType = value;
    }
    void setDeform(const float value)
    {
        m_deform = value;
    }
    void setGain(const float value)
    {
        m_gain = value;
    }
    void setOffset(const float value)
    {
        m_offset = value;
    }
    void setClip(const float value)
    {
        m_clip = value;
    }
    void setPhase(const float value)
    {
        m_phase = value;
    }
    void setSmooth(const float value)
    {
        m_smooth = value;
    }
    void setOutputType(const size_t value)
    {
        m_outputType = value;
    }
    void setDiscrete(const float value)
    {
        m_discrete = value;
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
    size_t m_wave{};
    float m_vol{};
    float m_frequency{};
    size_t m_deformType{};
    float m_deform{};
    float m_gain{};
    float m_offset{};
    float m_clip{};
    float m_phase{};
    float m_smooth{};
    size_t m_outputType{};
    float m_discrete{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};