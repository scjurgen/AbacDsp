#pragma once

#include <array>
#include <cstddef>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/Biquad.h"
#include "Generators/KarplusStrongEnsemble.h"
#include "Numbers/Convert.h"
#include "PluckSequencer.h"
#include "Reverbs/FdnTankGlide.h"

template <size_t BlockSize>
class TanpuraImpl final : public EffectBase
{
  public:
    explicit TanpuraImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_ensemble(sampleRate)
        , m_sequencer(sampleRate)
        , m_fdn(sampleRate)
    {
        m_fdn.setModulation(kReverbModulationDepth, kReverbModulationSpeedHz);
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

    void setDetune(const float value)
    {
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            m_sequencer.setDetuneCents(i, kDetuneFactors[i] * value);
        }
    }

    void setReverbDry(const float valueDb)
    {
        m_reverbDryGain = Convert::dbToGain(valueDb);
    }

    void setReverbWet(const float valueDb)
    {
        m_reverbWetGain = Convert::dbToGain(valueDb);
    }

    void setReverbSize(const float meters)
    {
        m_fdn.setMinSize(meters / kFdnSizeSpread);
        m_fdn.setMaxSize(meters * kFdnSizeSpread);
    }

    void setReverbDecay(const float msecs)
    {
        m_fdn.setDecay(msecs);
    }

    void setReverbShelfLow(const float valueDb)
    {
        for (auto& f : m_reverbShelfLow)
        {
            f.computeCoefficients(sampleRate(), kReverbShelfLowHz, kEqShelfQ, valueDb);
        }
    }

    void setReverbShelfHigh(const float valueDb)
    {
        for (auto& f : m_reverbShelfHigh)
        {
            f.computeCoefficients(sampleRate(), kReverbShelfHighHz, kEqShelfQ, valueDb);
        }
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
        std::array<float, BlockSize> dry{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_sequencer.step(m_ensemble);
            dry[i] = m_ensemble.step();
        }

        std::array<float, BlockSize> wetLeft{};
        std::array<float, BlockSize> wetRight{};
        m_fdn.processBlockSplit(dry.data(), wetLeft.data(), wetRight.data());
        m_reverbShelfLow[0].processBlock(wetLeft.data(), wetLeft.data(), BlockSize);
        m_reverbShelfLow[1].processBlock(wetRight.data(), wetRight.data(), BlockSize);
        m_reverbShelfHigh[0].processBlock(wetLeft.data(), wetLeft.data(), BlockSize);
        m_reverbShelfHigh[1].processBlock(wetRight.data(), wetRight.data(), BlockSize);

        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = (dry[i] * m_reverbDryGain + wetLeft[i] * m_reverbWetGain) * m_level;
            out(i, 1) = (dry[i] * m_reverbDryGain + wetRight[i] * m_reverbWetGain) * m_level;
        }
    }

  private:
    static constexpr size_t kMaxStringLength{10000};
    static constexpr size_t kNumVoices{5};
    static constexpr std::array<float, kNumVoices> kDetuneFactors{0.f, -1.f, 1.3f, -1.7f, 2.f};
    static constexpr size_t kFdnOrder{32};
    static constexpr size_t kFdnMaxSizePerElement{100000};
    static constexpr float kFdnSizeSpread{2.3f};
    static constexpr float kReverbModulationDepth{0.02f};
    static constexpr float kReverbModulationSpeedHz{0.35f};
    static constexpr float kReverbShelfLowHz{150.f};
    static constexpr float kReverbShelfHighHz{6000.f};
    static constexpr float kEqShelfQ{0.707f};

    using Fdn = AbacDsp::FdnTankGlide<kFdnMaxSizePerElement, kFdnOrder, BlockSize>;
    using LoShelfFilter = AbacDsp::Biquad<AbacDsp::BiquadFilterType::LoShelf>;
    using HiShelfFilter = AbacDsp::Biquad<AbacDsp::BiquadFilterType::HiShelf>;

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
    Fdn m_fdn;
    std::array<LoShelfFilter, 2> m_reverbShelfLow{};
    std::array<HiShelfFilter, 2> m_reverbShelfHigh{};

    float m_level{1.f};
    float m_reverbDryGain{1.f};
    float m_reverbWetGain{0.f};
    float m_attackFilterMsecs{10.f};
    float m_decayFilterMsecs{10.f};
    float m_levelSustainFilter{0.f};
};
