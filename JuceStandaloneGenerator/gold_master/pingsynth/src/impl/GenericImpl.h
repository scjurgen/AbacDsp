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
    void setMode(const size_t value)
    {
        m_mode = value;
    }
    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }
    void setReverbLevel(const float value)
    {
        m_reverbLevel = std::pow(10.f, value / 20.f);
    }
    void setX(const float value)
    {
        m_x = value;
    }
    void setY(const float value)
    {
        m_y = value;
    }
    void setAttack(const float value)
    {
        m_attack = value;
    }
    void setDecay(const float value)
    {
        m_decay = value;
    }
    void setDecaySkew(const float value)
    {
        m_decaySkew = value;
    }
    void setSpread(const float value)
    {
        m_spread = value;
    }
    void setType(const size_t value)
    {
        m_type = value;
    }
    void setSkew(const float value)
    {
        m_skew = value;
    }
    void setSpread(const float value)
    {
        m_Spread = value;
    }
    void setRandPower(const float value)
    {
        m_randPower = value;
    }
    void setRandExcitation(const float value)
    {
        m_randExcitation = value;
    }
    void setSoftExcitation(const float value)
    {
        m_softExcitation = value;
    }
    void setSparkleTime(const float value)
    {
        m_sparkleTime = value;
    }
    void setSparkleRand(const float value)
    {
        m_sparkleRand = value;
    }
    void setMinHarmonics(const float value)
    {
        m_minHarmonics = value;
    }
    void setMaxHarmonics(const float value)
    {
        m_maxHarmonics = value;
    }
    void setMinPbNote(const float value)
    {
        m_minPbNote = value;
    }
    void setMaxPbNote(const float value)
    {
        m_maxPbNote = value;
    }
    void setRangePb(const float value)
    {
        m_rangePb = value;
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
    size_t m_mode{};
    float m_vol{};
    float m_reverbLevel{};
    float m_x{};
    float m_y{};
    float m_attack{};
    float m_decay{};
    float m_decaySkew{};
    float m_spread{};
    size_t m_type{};
    float m_skew{};
    float m_Spread{};
    float m_randPower{};
    float m_randExcitation{};
    float m_softExcitation{};
    float m_sparkleTime{};
    float m_sparkleRand{};
    float m_minHarmonics{};
    float m_maxHarmonics{};
    float m_minPbNote{};
    float m_maxPbNote{};
    float m_rangePb{};
};