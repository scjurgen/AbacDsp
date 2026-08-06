#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class TanpuraImpl final : public EffectBase
{
  public:
    explicit TanpuraImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setKey(const size_t value)
    {
        m_key = value;
    }
    void setLevel(const float value)
    {
        m_level = std::pow(10.f, value / 20.f);
    }
    void setTuning(const float value)
    {
        m_tuning = value;
    }
    void setDetuneString1(const float value)
    {
        m_detuneString1 = value;
    }
    void setDetuneString2(const float value)
    {
        m_detuneString2 = value;
    }
    void setDetuneString3(const float value)
    {
        m_detuneString3 = value;
    }
    void setDetuneString4(const float value)
    {
        m_detuneString4 = value;
    }
    void setDetuneString5(const float value)
    {
        m_detuneString5 = value;
    }
    void setPattern(const size_t value)
    {
        m_pattern = value;
    }
    void setSlide(const float value)
    {
        m_slide = value;
    }
    void setHarmonicFirst(const size_t value)
    {
        m_harmonicFirst = value;
    }
    void setHarmonicSecond(const size_t value)
    {
        m_harmonicSecond = value;
    }
    void setPlayStop(const bool value)
    {
        m_playStop = value;
    }
    void setPicksPerMinute(const float value)
    {
        m_picksPerMinute = value;
    }
    void setPauseLength(const float value)
    {
        m_pauseLength = value;
    }
    void setAttack(const float value)
    {
        m_attack = value;
    }
    void setDecay(const float value)
    {
        m_decay = value;
    }
    void setLevelSustain(const float value)
    {
        m_levelSustain = value;
    }
    void setLfoDepth(const float value)
    {
        m_lfoDepth = value;
    }
    void setAttackFilter(const float value)
    {
        m_attackFilter = value;
    }
    void setDecayFilter(const float value)
    {
        m_decayFilter = value;
    }
    void setLevelSustainFilter(const float value)
    {
        m_levelSustainFilter = value;
    }
    void setFilterCutoff(const float value)
    {
        m_filterCutoff = value;
    }
    void setFilterResonance(const float value)
    {
        m_filterResonance = value;
    }
    void setContourFilter(const float value)
    {
        m_contourFilter = value;
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
    size_t m_key{};
    float m_level{};
    float m_tuning{};
    float m_detuneString1{};
    float m_detuneString2{};
    float m_detuneString3{};
    float m_detuneString4{};
    float m_detuneString5{};
    size_t m_pattern{};
    float m_slide{};
    size_t m_harmonicFirst{};
    size_t m_harmonicSecond{};
    bool m_playStop{};
    float m_picksPerMinute{};
    float m_pauseLength{};
    float m_attack{};
    float m_decay{};
    float m_levelSustain{};
    float m_lfoDepth{};
    float m_attackFilter{};
    float m_decayFilter{};
    float m_levelSustainFilter{};
    float m_filterCutoff{};
    float m_filterResonance{};
    float m_contourFilter{};
};