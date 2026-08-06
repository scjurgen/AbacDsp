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
    }
    void setNumChains(const float value)
    {
        m_numChains = value;
    }
    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }
    void setWet(const float value)
    {
        m_wet = std::pow(10.f, value / 20.f);
    }
    void setLowFreq(const float value)
    {
        m_lowFreq = value;
    }
    void setHighFreq(const float value)
    {
        m_highFreq = value;
    }
    void setDistribution(const size_t value)
    {
        m_distribution = value;
    }
    void setDecayMin(const float value)
    {
        m_decayMin = value;
    }
    void setDecayMax(const float value)
    {
        m_decayMax = value;
    }
    void setGainMin(const float value)
    {
        m_gainMin = std::pow(10.f, value / 20.f);
    }
    void setGainMax(const float value)
    {
        m_gainMax = std::pow(10.f, value / 20.f);
    }
    void setDelayMin(const float value)
    {
        m_delayMin = value;
    }
    void setDelayMax(const float value)
    {
        m_delayMax = value;
    }
    void setQ(const float value)
    {
        m_q = value;
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
    float m_numChains{};
    float m_dry{};
    float m_wet{};
    float m_lowFreq{};
    float m_highFreq{};
    size_t m_distribution{};
    float m_decayMin{};
    float m_decayMax{};
    float m_gainMin{};
    float m_gainMax{};
    float m_delayMin{};
    float m_delayMax{};
    float m_q{};
};