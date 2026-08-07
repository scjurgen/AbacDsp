#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>
#include <string_view>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/Biquad.h"
#include "Generators/KarplusStrongEnsemble.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Helpers/ConstructArray.h"
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
        , m_lfoWander(AbacDsp::constructArray<AbacDsp::OrnsteinUhlenbeckProcess, kNumVoices>(
              sampleRate / static_cast<float>(BlockSize)))
    {
        m_fdn.setModulation(kReverbModulationDepth, kReverbModulationSpeedHz);
        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist{-1.f, 1.f};
        for (auto& offset : m_voiceLfoStaticOffset)
        {
            offset = dist(rng);
        }
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

    void setPlayStop(const bool value) noexcept
    {
        m_manualPlaying = value;
    }

    void setBpm(const float value) noexcept
    {
        m_manualBpm = value;
    }

    void setHostSync(const bool value) noexcept
    {
        m_hostSync = value;
    }

    void setPluckDivision(const int index) noexcept
    {
        m_pluckDivisionIndex = clampDivisionIndex(index);
    }

    void setPauseDivision(const int index) noexcept
    {
        m_pauseDivisionIndex = clampDivisionIndex(index);
    }

    [[nodiscard]] float currentBpm() const noexcept
    {
        return m_hostSync ? std::clamp(static_cast<float>(hostTransport().bpm), 20.f, 300.f) : m_manualBpm;
    }

    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return m_hostSync;
    }

    [[nodiscard]] bool effectivePlaying() const noexcept
    {
        return m_hostSync ? hostTransport().isPlaying : m_manualPlaying;
    }

    [[nodiscard]] float voiceLfoSpeed(const size_t index) const noexcept
    {
        return m_lastVoiceLfoSpeed[index];
    }

    [[nodiscard]] static float intervalMsForDivision(const float bpm, const int divisionIndex) noexcept
    {
        return kSyncDivisions[clampDivisionIndex(divisionIndex)].quarterNotes * (60000.f / bpm);
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

    void setLfoSpeed(const float value) noexcept
    {
        m_lfoSpeed = value;
    }

    void setLfoSpeedVariation(const float percent) noexcept
    {
        m_lfoSpeedVariationPercent = std::clamp(percent, 0.f, 100.f);
    }

    void setHumanizeTiming(const float percent)
    {
        m_sequencer.setHumanizeTiming(percent);
    }

    void setHumanizeLevel(const float percent)
    {
        m_sequencer.setHumanizeLevel(percent);
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
        forEachVoice([value](auto& voice) { voice.setFilterResonance(value * 2.1f); });
    }

    void setContourFilter(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setKeyTracking(value); });
    }

    void processBlock([[maybe_unused]] const AbacDsp::AudioBuffer<2, BlockSize>& in,
                      AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        updateTiming();
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
    static constexpr float kMaxVoiceLfoSpreadFraction{0.15f}; // per-voice static offset bound at 100% variation
    static constexpr float kMaxLfoWanderSigma{0.1f};          // shared wander bound at 100% variation

    struct SyncDivision
    {
        std::string_view name;
        float quarterNotes;
    };

    // clang-format off
    static constexpr auto kSyncDivisions = std::to_array<SyncDivision>({
        {"1/1",   4.f},      {"1/2",   2.f},      {"1/2.",  3.f},      {"1/2T",  4.f / 3.f},
        {"1/4",   1.f},      {"1/4.",  1.5f},     {"1/4T",  2.f / 3.f},
        {"1/8",   0.5f},     {"1/8.",  0.75f},    {"1/8T",  1.f / 3.f},
        {"1/16",  0.25f},    {"1/16.", 0.375f},   {"1/16T", 1.f / 6.f},
    });
    // clang-format on

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

    [[nodiscard]] static size_t clampDivisionIndex(const int index) noexcept
    {
        return static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kSyncDivisions.size()) - 1));
    }

    void updateTiming() noexcept
    {
        const float bpm = currentBpm();
        m_sequencer.setIntervalMs(intervalMsForDivision(bpm, static_cast<int>(m_pluckDivisionIndex)));
        m_sequencer.setPauseGapMs(intervalMsForDivision(bpm, static_cast<int>(m_pauseDivisionIndex)));
        m_sequencer.setPlaying(effectivePlaying());
        updateLfoSpeeds();
    }

    // At 0% variation, every voice's wander sigma/mu settle to 0 (mu = sigma in
    // OrnsteinUhlenbeckProcess), so this reduces exactly to the old shared-speed behavior.
    void updateLfoSpeeds() noexcept
    {
        const auto amount = m_lfoSpeedVariationPercent * 0.01f;
        const auto sigma = amount * kMaxLfoWanderSigma;
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            m_lfoWander[i].setSigma(sigma);
            const auto wander = m_lfoWander[i].step() - sigma;
            const auto speed =
                m_lfoSpeed * (1.f + m_voiceLfoStaticOffset[i] * kMaxVoiceLfoSpreadFraction * amount) * (1.f + wander);
            m_lastVoiceLfoSpeed[i] = std::max(0.01f, speed);
            m_ensemble.voice(i).setFilterLfoSpeed(m_lastVoiceLfoSpeed[i]);
        }
    }

    AbacDsp::KarplusStrongEnsemble<kNumVoices, kMaxStringLength> m_ensemble;
    PluckSequencer<kMaxStringLength> m_sequencer;
    Fdn m_fdn;
    std::array<LoShelfFilter, 2> m_reverbShelfLow{};
    std::array<HiShelfFilter, 2> m_reverbShelfHigh{};
    std::array<AbacDsp::OrnsteinUhlenbeckProcess, kNumVoices> m_lfoWander;
    std::array<float, kNumVoices> m_voiceLfoStaticOffset{};
    std::array<float, kNumVoices> m_lastVoiceLfoSpeed{};

    float m_level{1.f};
    float m_reverbDryGain{1.f};
    float m_reverbWetGain{0.f};
    float m_manualBpm{120.f};
    bool m_hostSync{false};
    bool m_manualPlaying{false};
    size_t m_pluckDivisionIndex{4};
    size_t m_pauseDivisionIndex{4};
    float m_lfoSpeed{0.5f};
    float m_lfoSpeedVariationPercent{0.f};
    float m_attackFilterMsecs{10.f};
    float m_decayFilterMsecs{10.f};
    float m_levelSustainFilter{0.f};
};
