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
    void setLevel(const float value)
    {
        m_level = std::pow(10.f, value / 20.f);
    }
    void setTuning(const float value)
    {
        m_tuning = value;
    }
    void setTranspose(const float value)
    {
        m_transpose = value;
    }
    void setDetune(const float value)
    {
        m_detune = value;
    }
    void setReverbDry(const float value)
    {
        m_reverbDry = std::pow(10.f, value / 20.f);
    }
    void setReverbWet(const float value)
    {
        m_reverbWet = std::pow(10.f, value / 20.f);
    }
    void setReverbSize(const float value)
    {
        m_reverbSize = value;
    }
    void setReverbDecay(const float value)
    {
        m_reverbDecay = value;
    }
    void setReverbShelfLow(const float value)
    {
        m_reverbShelfLow = std::pow(10.f, value / 20.f);
    }
    void setReverbShelfHigh(const float value)
    {
        m_reverbShelfHigh = std::pow(10.f, value / 20.f);
    }
    void setPlayStop(const bool value)
    {
        m_playStop = value;
    }
    void setHumanizeTiming(const float value)
    {
        m_humanizeTiming = value;
    }
    void setHumanizeLevel(const float value)
    {
        m_humanizeLevel = value;
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setHostSync(const bool value)
    {
        m_hostSync = value;
    }
    void setDivision(const size_t value)
    {
        m_division = value;
    }
    void setAttack(const float value)
    {
        m_attack = value;
    }
    void setDecay(const float value)
    {
        m_decay = value;
    }
    void setDecayOctave(const float value)
    {
        m_decayOctave = value;
    }
    void setDamper(const float value)
    {
        m_damper = value;
    }
    void setLevelSustain(const float value)
    {
        m_levelSustain = value;
    }
    void setSustainHumanize(const float value)
    {
        m_sustainHumanize = value;
    }
    void setLfoDepth(const float value)
    {
        m_lfoDepth = value;
    }
    void setLfoSpeed(const float value)
    {
        m_lfoSpeed = value;
    }
    void setLfoSpeedVariation(const float value)
    {
        m_lfoSpeedVariation = value;
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
    float m_level{};
    float m_tuning{};
    float m_transpose{};
    float m_detune{};
    float m_reverbDry{};
    float m_reverbWet{};
    float m_reverbSize{};
    float m_reverbDecay{};
    float m_reverbShelfLow{};
    float m_reverbShelfHigh{};
    bool m_playStop{};
    float m_humanizeTiming{};
    float m_humanizeLevel{};
    float m_bpm{};
    bool m_hostSync{};
    size_t m_division{};
    float m_attack{};
    float m_decay{};
    float m_decayOctave{};
    float m_damper{};
    float m_levelSustain{};
    float m_sustainHumanize{};
    float m_lfoDepth{};
    float m_lfoSpeed{};
    float m_lfoSpeedVariation{};
    float m_attackFilter{};
    float m_decayFilter{};
    float m_levelSustainFilter{};
    float m_filterCutoff{};
    float m_filterResonance{};
    float m_contourFilter{};
};