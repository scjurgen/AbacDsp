#pragma once

#include <cstddef>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/KarplusStrongEnsemble.h"
#include "Numbers/Convert.h"
#include "PluckSequencer.h"

template <size_t BlockSize>
class TanpuraImpl final : public EffectBase
{
  public:
    explicit TanpuraImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_ensemble(sampleRate)
        , m_sequencer(sampleRate)
    {
    }

    void setKey(const size_t value)
    {
        m_sequencer.setKey(value);
    }

    void setLevel(const float value)
    {
        m_level = Convert::dbToGain(value);
    }

    void setTuning(const float value)
    {
        m_sequencer.setTuning(value);
    }

    void setDetuneString1(const float value)
    {
        m_sequencer.setDetuneCents(0, value);
    }

    void setDetuneString2(const float value)
    {
        m_sequencer.setDetuneCents(1, value);
    }

    void setDetuneString3(const float value)
    {
        m_sequencer.setDetuneCents(2, value);
    }

    void setDetuneString4(const float value)
    {
        m_sequencer.setDetuneCents(3, value);
    }

    void setDetuneString5(const float value)
    {
        m_sequencer.setDetuneCents(4, value);
    }

    void setPattern(const size_t value)
    {
        m_sequencer.setPattern(value);
    }

    void setSlide(const float value)
    {
        m_sequencer.setSlidePercent(value);
    }

    void setSlideTime(const float value)
    {
        m_sequencer.setSlideTimeMs(value);
    }

    void setHarmonicFirst(const size_t value)
    {
        m_sequencer.setHarmonicFirst(value);
    }

    void setHarmonicSecond(const size_t value)
    {
        m_sequencer.setHarmonicSecond(value);
    }

    void setPlayStop(const bool value)
    {
        m_sequencer.setPlaying(value);
    }

    void setPicksPerMinute(const float value)
    {
        m_sequencer.setPicksPerMinute(value);
    }

    void setPauseLength(const float value)
    {
        m_sequencer.setPauseLengthMs(value);
    }

    void setAttack(const float value)
    {
        forEachVoice([value](auto& voice) { voice.attackTime(value); });
    }

    void setDecay(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setDecayByTime(value); });
    }

    void setLevelSustain(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setConstFeed(value); });
    }

    void setLfoDepth(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setFilterLfoDepthOctaves(value); });
    }

    void setAttackFilter(const float value)
    {
        m_attackFilterMsecs = value;
        updateFilterEnvelope();
    }

    void setDecayFilter(const float value)
    {
        m_decayFilterMsecs = value;
        updateFilterEnvelope();
    }

    void setLevelSustainFilter(const float value)
    {
        m_levelSustainFilter = value;
        updateFilterEnvelope();
    }

    void setFilterCutoff(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setFilterCutoffSemitones(value); });
    }

    void setFilterResonance(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setFilterResonance(value); });
    }

    void setContourFilter(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setKeyTracking(value); });
    }

    void processBlock([[maybe_unused]] const AbacDsp::AudioBuffer<2, BlockSize>& in,
                      AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_sequencer.step(m_ensemble);
            const auto sample = m_ensemble.step() * m_level;
            out(i, 0) = sample;
            out(i, 1) = sample;
        }
    }

  private:
    static constexpr size_t kMaxStringLength{10000};
    static constexpr size_t kNumVoices{5};

    template <typename Fn>
    void forEachVoice(Fn&& fn)
    {
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            fn(m_ensemble.voice(i));
        }
    }

    void updateFilterEnvelope()
    {
        forEachVoice([this](auto& voice)
                     { voice.setFilterEnvelope(m_attackFilterMsecs, m_decayFilterMsecs, m_levelSustainFilter); });
    }

    AbacDsp::KarplusStrongEnsemble<kNumVoices, kMaxStringLength> m_ensemble;
    PluckSequencer<kMaxStringLength> m_sequencer;

    float m_level{1.f};
    float m_attackFilterMsecs{10.f};
    float m_decayFilterMsecs{10.f};
    float m_levelSustainFilter{0.f};
};
