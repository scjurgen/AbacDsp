#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "ChorusConfigurations.h"
#include "Delays/OrganicChorusEngine.h"
#include "EffectBase.h"
#include "Filters/Sinc/sinc_4.h"
#include "OrganicChorusScriptEngine.h"

template <size_t BlockSize>
class OrganicChorusImpl final : public EffectBase
{
  public:
    static constexpr size_t kBufferSize{8192};
    static constexpr size_t kMaxVoices{OrganicChorus::kMaxVoices};
    using Engine = AbacDsp::OrganicChorusEngine<kBufferSize, BlockSize, kMaxVoices>;

    explicit OrganicChorusImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_engine(sampleRate, std::make_shared<AbacDsp::SincFilter>(sinc4))
        , m_sampleRate(sampleRate)
    {
        m_visualWavedata.resize(6000);
        applyConfiguration();
    }

    bool setScript(const std::string_view source)
    {
        return m_scriptEngine.loadScript(source);
    }

    void setImportResolver(OrganicChorusScriptEngine::ImportResolver resolver)
    {
        m_scriptEngine.setImportResolver(std::move(resolver));
    }

    [[nodiscard]] bool hasScriptError() const noexcept
    {
        return m_scriptEngine.hasError();
    }

    [[nodiscard]] const std::string& scriptError() const noexcept
    {
        return m_scriptEngine.lastError();
    }

    [[nodiscard]] static std::string scriptSkeleton()
    {
        return OrganicChorusScriptEngine::kFullSkeletonScript;
    }

    [[nodiscard]] const OrganicChorusScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    void setConfiguration(const int value)
    {
        m_configuration =
            static_cast<size_t>(std::clamp(value, 0, static_cast<int>(OrganicChorus::kConfigurationCount) - 1));
        applyConfiguration();
    }

    void setTone(const float percent)
    {
        m_tone = percent * 0.01f;
        applyConfiguration();
    }

    // Speed is an absolute Hz value shared across configurations, not a per-configuration
    // fraction - applyConfiguration() clamps it into whichever configuration is active.
    void setSpeed(const float hz)
    {
        m_speedHz = hz;
        applyConfiguration();
    }

    // Tape Speed is the transport's own baseline ratio (semitones, 0 = unity), applied to
    // every voice directly - a manual pitch/time offset independent of the Wow wobble.
    void setTapeSpeed(const float semitones)
    {
        const auto ratio = std::pow(2.f, semitones / 12.f);
        for (size_t v = 0; v < OrganicChorus::kMaxVoices; ++v)
        {
            m_engine.setVoiceTapeSpeedBaseRatio(v, ratio);
        }
    }

    void setDepth(const float percent)
    {
        m_depth = percent * 0.01f;
        applyConfiguration();
    }

    void setFeedback(const float percent)
    {
        m_feedback = percent * 0.01f;
        applyConfiguration();
    }

    void setMix(const float percent)
    {
        m_engine.setMix(percent * 0.01f);
    }

    // Extra live-tweak knobs beyond the six macros, claimed via the script's
    // UICreateParameterSet (see OrganicChorusScriptEngine's stub script).
    void setLuaParam1(const float value)
    {
        m_drift = value;
        applyConfiguration();
    }

    void setLuaParam2(const float value)
    {
        m_spread = value;
        applyConfiguration();
    }

    void setLuaParam3(const float) {}
    void setLuaParam4(const float) {}
    void setLuaParam5(const float) {}
    void setLuaParam6(const float) {}
    void setLuaParam7(const float) {}
    void setLuaParam8(const float) {}

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<float, 2 * BlockSize> interleavedIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            interleavedIn[2 * i] = in(i, 0);
            interleavedIn[2 * i + 1] = in(i, 1);
        }

        std::array<float, 2 * BlockSize> interleavedOut{};
        m_engine.processBlock(interleavedIn, interleavedOut);

        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = interleavedOut[2 * i];
            out(i, 1) = interleavedOut[2 * i + 1];
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            m_currentSample = (m_currentSample + 1) % m_visualWavedata.size();
        }
    }

    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    // Re-derives every voice's DSP values from the active configuration's spec and the
    // current macro fractions - cheap enough (a handful of setters per voice) to just
    // rerun in full on any macro change rather than tracking what actually moved.
    void applyConfiguration()
    {
        const auto& spec = OrganicChorus::kConfigurations[m_configuration];
        m_engine.setActiveVoiceCount(spec.voiceCount);

        // Drift is real mechanical speed wander (see OrganicChorusVoice's speed-drift),
        // independent of Depth's sine-LFO wobble - not scaled by it.
        const auto speedDriftSigma = m_drift * spec.speedDriftMaxSigma;

        for (size_t v = 0; v < spec.voiceCount; ++v)
        {
            const auto& voice = spec.voices[v];
            const auto spreadFraction = static_cast<float>(v) * m_spread;

            const auto delayMs = spec.baseDelayMs.at(m_depth) + voice.delayOffsetMs;
            const auto delaySamples = delayMs * 0.001f * m_sampleRate;
            m_engine.setVoiceReadHeadSafetyMargin(v, spec.readHeadSafetyMarginSamples);
            m_engine.setVoiceReadHeadCorrectionThreshold(v, spec.readHeadCorrectionThresholdSamples);
            m_engine.setVoiceCentreDelay(v, delaySamples);
            m_engine.setVoiceSpeedDriftAmplitude(v, speedDriftSigma);

            const auto clampedSpeedHz = std::clamp(m_speedHz, spec.wowRateHz.atZero, spec.wowRateHz.atOne);
            const auto wowRate = clampedSpeedHz * voice.rateMultiplier * (1.f + 0.05f * spreadFraction);
            const auto wowDepth = spec.wowDepth.at(m_depth);
            m_engine.setVoiceWow(v, wowRate, wowDepth, 0.f, 0.f);
            const auto flutterRate = std::max(m_speedHz, spec.flutterRateFloorHz);
            m_engine.setVoiceFlutter(v, flutterRate, spec.flutterDepth.at(m_depth));

            m_engine.setVoiceTone(v, spec.toneHighPassHz.at(m_tone), spec.tonePreLowPassHz.at(m_tone),
                                  spec.tonePostLowPassHz.at(m_tone));
            m_engine.setVoiceSaturation(v, spec.saturation);
            m_engine.setVoiceFeedback(v, spec.feedback.at(0.5f + 0.5f * m_feedback), spec.feedbackDampHz);
            m_engine.setVoicePan(v, voice.pan);
        }
    }

    Engine m_engine;
    const float m_sampleRate;
    size_t m_configuration{0};
    float m_tone{0.5f};
    float m_speedHz{0.8f};
    float m_depth{0.5f};
    float m_feedback{0.f};
    float m_drift{0.5f};
    float m_spread{0.5f};

    OrganicChorusScriptEngine m_scriptEngine;
    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample{0};
};
