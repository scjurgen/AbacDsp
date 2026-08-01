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
    void setLevel(const float value)
    {
        m_level = std::pow(10.f, value / 20.f);
    }
    void setType(const size_t value)
    {
        m_type = value;
    }
    void setPreboostLow(const float value)
    {
        m_preboostLow = std::pow(10.f, value / 20.f);
    }
    void setPreboostHigh(const float value)
    {
        m_preboostHigh = std::pow(10.f, value / 20.f);
    }
    void setCrossOver(const float value)
    {
        m_crossOver = value;
    }
    void setCut(const float value)
    {
        m_cut = value;
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
    float m_level{};
    size_t m_type{};
    float m_preboostLow{};
    float m_preboostHigh{};
    float m_crossOver{};
    float m_cut{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};