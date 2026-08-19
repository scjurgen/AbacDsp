#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

#include "Audio/AudioBuffer.h"
#include "Delays/MultiTapDelay.h"
#include "EffectBase.h"
#include "Filters/BiquadResoBP.h"
#include "Filters/SvfResoBP.h"
#include "Parameters/SmoothingParameter.h"
#include "ResonikScriptEngine.h"

template <size_t BlockSize>
class ResonikImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxChains{100};
    // One shared buffer sized for the max delay time, independent of kMaxChains. Assumes
    // the engine's fixed 48 kHz internal rate (RateNormalizer::kInternalSampleRate).
    static constexpr float kMaxDelayMs{60000.f};
    static constexpr float kAssumedSampleRate{48000.f};
    static constexpr size_t kMaxDelaySamples{static_cast<size_t>(kAssumedSampleRate * kMaxDelayMs / 1000.f) + 4};
    static constexpr float kPitchAnalysisGranularityMs{100.f};
    static_assert(ResonikScriptEngine::kMaxBodies >= kMaxChains,
                  "ResonikScriptEngine's body-override pool must cover every chain");

    explicit ResonikImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
        for (auto& biquad : m_biquad)
        {
            biquad.setSampleRate(sampleRate);
        }
        for (auto& svf : m_svf)
        {
            svf.setSampleRate(sampleRate);
        }
        m_chainQ.fill(m_q);
        recomputeFrequencies();
        recomputeDecay();
        recomputeGain();
        recomputeDelay();
        m_scriptEngine.setSampleRate(sampleRate);
        m_scriptEngine.setPitchAnalysisGranularity(kPitchAnalysisGranularityMs);
    }

    // A reload resets the script's Lua globals, so resendUiParameters() re-syncs it to
    // each claimed slot's current value - otherwise it stays believing coded defaults.
    bool setScript(const std::string_view source)
    {
        const bool ok = m_scriptEngine.loadScript(source);
        if (ok)
        {
            resendUiParameters();
        }
        return ok;
    }

    void setImportResolver(ResonikScriptEngine::ImportResolver resolver)
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

    // Shown by the popup editor's Reset button, not the engine's own default script.
    [[nodiscard]] static std::string scriptSkeleton()
    {
        return std::string(ResonikScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const ResonikScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    void setLuaParam1(const float value) noexcept
    {
        m_luaParamValues[0] = value;
    }

    void setLuaParam2(const float value) noexcept
    {
        m_luaParamValues[1] = value;
    }

    void setLuaParam3(const float value) noexcept
    {
        m_luaParamValues[2] = value;
    }

    void setLuaParam4(const float value) noexcept
    {
        m_luaParamValues[3] = value;
    }

    void setLuaParam5(const float value) noexcept
    {
        m_luaParamValues[4] = value;
    }

    void setLuaParam6(const float value) noexcept
    {
        m_luaParamValues[5] = value;
    }

    void setLuaParam7(const float value) noexcept
    {
        m_luaParamValues[6] = value;
    }

    void setLuaParam8(const float value) noexcept
    {
        m_luaParamValues[7] = value;
    }

    void setNumChains(const float value)
    {
        m_activeChains = static_cast<size_t>(std::clamp(value, 1.f, static_cast<float>(kMaxChains)) + 0.5f);
        recomputeFrequencies();
        recomputeDecay();
        recomputeGain();
        recomputeDelay();
    }

    void setDry(const float value)
    {
        m_dry.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setWet(const float value)
    {
        constexpr float kWetBoost = 10.f;
        m_wet.newTransition(std::pow(10.f, value / 20.f) * kWetBoost, kParamSmoothingSeconds, sampleRate());
    }

    void setLowFreq(const float value)
    {
        m_lowFreq = value;
        recomputeFrequencies();
    }

    void setHighFreq(const float value)
    {
        m_highFreq = value;
        recomputeFrequencies();
    }

    void setDistribution(const size_t value)
    {
        m_distribution = value;
        recomputeFrequencies();
    }

    void setDecayMin(const float value)
    {
        m_decayMin = value;
        recomputeDecay();
    }

    void setDecayMax(const float value)
    {
        m_decayMax = value;
        recomputeDecay();
    }

    void setGainMin(const float value)
    {
        m_gainMinDb = value;
        recomputeGain();
    }

    void setGainMax(const float value)
    {
        m_gainMaxDb = value;
        recomputeGain();
    }

    void setQ(const float value)
    {
        m_q = value;
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            m_chainQ[c] = value;
        }
        recomputeBiquad();
    }

    void setDelayMin(const float value)
    {
        m_delayMinMs = value;
        recomputeDelay();
    }

    void setDelayMax(const float value)
    {
        m_delayMaxMs = value;
        recomputeDelay();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();

        std::array<float, BlockSize> monoIn{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            monoIn[s] = 0.5f * (in(s, 0) + in(s, 1));
        }
        m_scriptEngine.feedPitchAnalysis(monoIn);
        applyPendingResonanceCommands();

        // Sample-major, not chain-major: every chain taps the same shared delay buffer, so
        // it must be written exactly once per sample before any chain reads that sample.
        std::array<float, BlockSize> wetSum{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            m_delay.write(monoIn[s]);
            for (size_t c = 0; c < m_activeChains; ++c)
            {
                wetSum[s] += m_svf[c].step(m_biquad[c].step(m_delay.readTap(c))) * m_gainLin[c];
            }
        }

        for (size_t s = 0; s < BlockSize; ++s)
        {
            const float dry = m_dry.getValue();
            const float wet = m_wet.getValue();
            for (size_t c = 0; c < 2; ++c)
            {
                out(s, c) = dry * in(s, c) + wet * wetSum[s];
            }
        }
    }

  private:
    // Same exact-equality reasoning as DroneScriptEngine's notifyTimingIfChanged(): a
    // stored float either stays bit-identical or is genuinely a new host/UI value.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < ResonikScriptEngine::kMaxLuaParams; ++i)
        {
            if (m_luaParamValues[i] == m_lastNotifiedLuaParamValues[i])
            {
                continue;
            }
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }
#pragma GCC diagnostic pop

    // Notifies every slot's current value unconditionally, unlike
    // notifyUiParametersIfChanged() - see setScript()'s comment for why.
    void resendUiParameters() noexcept
    {
        for (size_t i = 0; i < ResonikScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

    void applyPendingResonanceCommands()
    {
        if (const auto freq = m_scriptEngine.drainFreqRangeCommand())
        {
            setLowFreq(freq->low);
            setHighFreq(freq->high);
            if (freq->distribution)
            {
                setDistribution(*freq->distribution);
            }
        }
        if (const auto decay = m_scriptEngine.drainDecayRangeCommand())
        {
            setDecayMin(decay->low);
            setDecayMax(decay->high);
        }
        if (const auto gain = m_scriptEngine.drainGainRangeCommand())
        {
            setGainMin(gain->low);
            setGainMax(gain->high);
        }
        if (const auto delay = m_scriptEngine.drainDelayRangeCommand())
        {
            setDelayMin(delay->low);
            setDelayMax(delay->high);
        }
        if (const auto q = m_scriptEngine.drainQCommand())
        {
            setQ(*q);
        }
        applyResonanceBodyOverrides();
    }

    // Applied after every aggregate command above, every block, so a per-body override
    // always wins for whichever fields it sets - see ResonikScriptEngine's class doc.
    void applyResonanceBodyOverrides()
    {
        const auto& overrides = m_scriptEngine.bodyOverrides();
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto& body = overrides[c];
            if (!body)
            {
                continue;
            }
            bool freqOrQChanged = false;
            if (body->freq)
            {
                m_chainFreq[c] = *body->freq;
                freqOrQChanged = true;
            }
            if (body->q)
            {
                m_chainQ[c] = *body->q;
                freqOrQChanged = true;
            }
            if (freqOrQChanged)
            {
                recomputeBiquadForChain(c);
            }
            if (body->freq || body->decay)
            {
                if (body->decay)
                {
                    m_chainDecay[c] = *body->decay;
                }
                recomputeSvfForChain(c);
            }
            if (body->gainDb)
            {
                m_gainLin[c] = std::pow(10.f, *body->gainDb / 20.f);
            }
            if (body->delayMs)
            {
                const auto delaySamples = static_cast<size_t>(
                    std::clamp(*body->delayMs, 0.f, kMaxDelayMs) * 0.001f * kAssumedSampleRate + 0.5f);
                m_delay.setTapDelay(c, delaySamples);
            }
        }
    }

    [[nodiscard]] float chainFraction(const size_t index) const noexcept
    {
        return m_activeChains > 1 ? static_cast<float>(index) / static_cast<float>(m_activeChains - 1) : 0.f;
    }

    void recomputeFrequencies()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            m_chainFreq[c] = m_distribution == 0 ? m_lowFreq + fraction * (m_highFreq - m_lowFreq)
                                                 : m_lowFreq * std::pow(m_highFreq / m_lowFreq, fraction);
        }
        recomputeBiquad();
        recomputeSvf();
    }

    void recomputeDecay()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            m_chainDecay[c] = m_decayMin + fraction * (m_decayMax - m_decayMin);
        }
        recomputeSvf();
    }

    void recomputeGain()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            const auto gainDb = m_gainMinDb + fraction * (m_gainMaxDb - m_gainMinDb);
            m_gainLin[c] = std::pow(10.f, gainDb / 20.f);
        }
    }

    void recomputeDelay()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            const auto delayMs = m_delayMinMs + fraction * (m_delayMaxMs - m_delayMinMs);
            const auto delaySamples =
                static_cast<size_t>(std::clamp(delayMs, 0.f, kMaxDelayMs) * 0.001f * kAssumedSampleRate + 0.5f);
            m_delay.setTapDelay(c, delaySamples);
        }
    }

    void recomputeBiquadForChain(const size_t c)
    {
        m_biquad[c].computeCoefficients(0, m_chainFreq[c], m_chainQ[c]);
    }

    void recomputeSvfForChain(const size_t c)
    {
        m_svf[c].setByDecay(0, m_chainFreq[c], m_chainDecay[c]);
    }

    void recomputeBiquad()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            recomputeBiquadForChain(c);
        }
    }

    void recomputeSvf()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            recomputeSvfForChain(c);
        }
    }

    static constexpr float kParamSmoothingSeconds{0.01f};
    size_t m_activeChains{48};
    AbacDsp::LinearSmoothing m_dry{1.f};
    AbacDsp::LinearSmoothing m_wet{1.f};
    float m_lowFreq{80.f};
    float m_highFreq{6000.f};
    size_t m_distribution{1};
    float m_decayMin{0.3f};
    float m_decayMax{4.f};
    float m_gainMinDb{-18.f};
    float m_gainMaxDb{0.f};
    float m_q{6.f};
    float m_delayMinMs{0.f};
    float m_delayMaxMs{300.f};

    std::array<AbacDsp::BiquadResoBP, kMaxChains> m_biquad{};
    std::array<AbacDsp::SvfResoBP, kMaxChains> m_svf{};
    AbacDsp::MultiTapDelay<kMaxDelaySamples, kMaxChains> m_delay{};
    std::array<float, kMaxChains> m_chainFreq{};
    std::array<float, kMaxChains> m_chainDecay{};
    std::array<float, kMaxChains> m_chainQ{};
    std::array<float, kMaxChains> m_gainLin{};

    ResonikScriptEngine m_scriptEngine;
    std::array<float, ResonikScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, ResonikScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{-1.f, -1.f, -1.f, -1.f,
                                                                                       -1.f, -1.f, -1.f, -1.f};
};