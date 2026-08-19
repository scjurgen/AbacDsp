#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "Audio/AudioBuffer.h"
#include "Delays/MultiTapDelay.h"
#include "EffectBase.h"
#include "Filters/CombResonator.h"
#include "Filters/SvfMultiMode.h"
#include "Filters/SvfResoBP.h"
#include "Helpers/ConstructArray.h"
#include "Parameters/SmoothingParameter.h"
#include "Reverbs/FdnTankGlide.h"
#include "SpectraltapScriptEngine.h"

namespace SpectraltapVoices
{

template <class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

struct BypassVoice
{
};

/// @brief Covers LowPass/HighPass/BandPass/Notch: one SvfMultiMode, mode picks the output tap.
struct FilterVoice
{
    AbacDsp::SvfMultiMode svf{};
    TapType mode{TapType::BandPass};
};

/// @brief gainBoost cancels SvfResoBP::step()'s built-in 1/Q normalization (meant for
/// impulse-triggered use elsewhere) so a continuously-driven Resonator peaks like BandPass.
struct ResonatorVoice
{
    AbacDsp::SvfResoBP svf{};
    float gainBoost{1.f};
};

/// @brief F0/F1/F2 as three parallel SvfResoBP sections, summed and gain-normalized.
struct FormantVoice
{
    std::array<AbacDsp::SvfResoBP, 3> sections{};
    float f1Gain{0.f};
    float f2Gain{0.f};
};

/// @brief Trivial tag, same role BypassVoice plays: the actual AbacDsp::CombResonator
/// lives in SpectraltapImpl::m_combs, constructed once and reused by index, so switching
/// a tap to/from CombResonator never (re)allocates its buffer on the audio thread. Each
/// logical tap owns two slots (see SpectraltapImpl::slotIndex()) so a retune can crossfade
/// between them instead of resetting state in place.
struct CombVoice
{
};

/// @brief Sine carrier at the tap's set frequency, ring-multiplied with the delayed input.
/// No decay/Q - a bounded [-1,1] carrier can never amplify.
struct RingModVoice
{
    float phase{0.f};
    float phaseIncrement{0.f};
};

using TapVoice = std::variant<BypassVoice, FilterVoice, ResonatorVoice, FormantVoice, CombVoice, RingModVoice>;

}

template <size_t BlockSize>
class SpectraltapImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxTaps{SpectraltapScriptEngine::kMaxTaps};
    static constexpr float kMaxDelayMs{SpectraltapScriptEngine::kMaxDelayMs};
    // One shared buffer sized for the max delay time, independent of kMaxTaps. Assumes
    // the engine's fixed 48 kHz internal rate (RateNormalizer::kInternalSampleRate).
    static constexpr float kAssumedSampleRate{48000.f};
    static constexpr float kMinFeedbackBeats{1.f};
    static constexpr float kMaxFeedbackBeats{32.f};
    // The lowest tempo the feedback delay line is sized for; below this (only reachable
    // via Host Sync) a long feedback time is capped by the buffer, not accommodated.
    static constexpr float kMinFeedbackBpm{40.f};
    static constexpr float kMaxFeedbackDelayMs{kMaxFeedbackBeats * 60000.f / kMinFeedbackBpm};
    static constexpr size_t kMaxDelaySamples{
        static_cast<size_t>(kAssumedSampleRate * std::max(kMaxDelayMs, kMaxFeedbackDelayMs) / 1000.f) + 4};
    static constexpr float kMaxTapFreqHz{SpectraltapScriptEngine::kMaxFreqHz};
    static constexpr float kCombMinFreqHz{20.f};
    static constexpr size_t kCombBufferSize{static_cast<size_t>(kAssumedSampleRate / kCombMinFreqHz) + 8};
    // Fixed, not decay-derived: formant sections are meant to be broad/vowel-like, not
    // razor-sharp, independent of whatever "decay" a script requests for other taps.
    static constexpr float kFormantSectionQ{10.f};
    // Its own (much smaller) constant than SvfResoBP's native decay-to-Q relation, which
    // is tuned for impulse ring-down time, not continuous drive - see filterQFromDecay().
    static constexpr float kFilterDecayToQ{0.005f};
    static constexpr float kMaxFilterQ{20.f};
    // Matches the Division dropdown's 13 items (1/1..1/16T); the Lua script owns the
    // actual beat-multiplier lookup (see kStubScript), this only bounds the dropdown's index.
    static constexpr size_t kNumDivisions{13};
    // Two physical slots per logical tap (delay-read + voice + comb) so applyTapTopology()
    // can crossfade a retune instead of resetting state in place.
    static constexpr size_t kMaxSlots{kMaxTaps * 2};
    static constexpr float kCrossfadeSeconds{0.03f};
    // One more shared-buffer read position, beyond every tap's own two slots, for the
    // global tempo-synced feedback loop.
    static constexpr size_t kGlobalFeedbackSlot{kMaxSlots};
    // Same headroom-scaled tanh limiter CombResonator uses (kLimiterHeadroom, same value):
    // bounds the write signal, never clamps a user-facing parameter.
    static constexpr float kFeedbackHeadroom{16.f};
    // Same tank maxdiffuser uses (order 32, for the closest sonic match); MaxSizePerElement
    // sized for this class's own [1, 60] m Reverb Size range, not maxdiffuser's larger one.
    static constexpr size_t kFdnOrder{32};
    static constexpr size_t kFdnMaxSizePerElement{24000};
    static constexpr float kFdnSizeSpread{2.3f};
    static constexpr float kFdnPresetBulge{-0.4f};
    using Fdn = AbacDsp::FdnTankGlide<kFdnMaxSizePerElement, kFdnOrder, BlockSize>;

    explicit SpectraltapImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_combs(AbacDsp::constructArray<AbacDsp::CombResonator<kCombBufferSize>, kMaxSlots>(sampleRate))
        , m_fdn{sampleRate}
    {
        m_scriptEngine.setSampleRate(sampleRate);
        m_fdn.setSpreadBulge(kFdnPresetBulge);
        setReverbSize(15.f);
        setReverbDecay(2000.f);
    }

    // A reload resets the script's Lua globals, so resendUiParameters()/resendTiming()
    // re-sync it to each claimed slot's value and the current bpm/division - otherwise a
    // script whose topology is built entirely from OnTiming would never configure a tap.
    bool setScript(const std::string_view source)
    {
        const bool ok = m_scriptEngine.loadScript(source);
        if (ok)
        {
            resendUiParameters();
            resendTiming();
        }
        return ok;
    }

    void setImportResolver(SpectraltapScriptEngine::ImportResolver resolver)
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
        return std::string(SpectraltapScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const SpectraltapScriptEngine::UiParamSlots& uiParamSlots() const noexcept
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

    void setDry(const float value)
    {
        m_dry.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setWet(const float value)
    {
        m_wet.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setBpm(const float value) noexcept
    {
        m_manualBpm = value;
    }

    void setHostSync(const bool value) noexcept
    {
        m_hostSync = value;
    }

    void setDivision(const int index) noexcept
    {
        m_divisionIndex = clampDivisionIndex(index);
    }

    void setFeedback(const float value) noexcept
    {
        m_feedback.newTransition(value * 0.01f, kParamSmoothingSeconds, sampleRate());
    }

    void setFeedbackBeats(const float value) noexcept
    {
        m_feedbackBeats = std::clamp(value, kMinFeedbackBeats, kMaxFeedbackBeats);
    }

    void setReverbWet(const float value)
    {
        m_reverbWet.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setReverbSize(const float meters) noexcept
    {
        m_fdn.setMinSize(meters / kFdnSizeSpread);
        m_fdn.setMaxSize(meters * kFdnSizeSpread);
    }

    void setReverbDecay(const float msecs) noexcept
    {
        m_fdn.setDecay(msecs);
    }

    // The manual BPM dial while free-running, or the host's tempo while Host Sync is on -
    // what OnTiming() actually times taps against, distinct from the shared
    // Transport.Tempo() every Lua example gets (always the raw host tempo).
    [[nodiscard]] float currentBpm() const noexcept
    {
        return m_hostSync ? std::clamp(static_cast<float>(hostTransport().bpm), 20.f, 300.f) : m_manualBpm;
    }

    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return m_hostSync;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();
        notifyTimingIfChanged(currentBpm());
        applyPendingCommands();
        updateTapCoefficients();
        updateFeedbackDelay();

        std::array<float, BlockSize> monoIn{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            monoIn[s] = 0.5f * (in(s, 0) + in(s, 1));
        }

        std::array<float, BlockSize> wetL{};
        std::array<float, BlockSize> wetR{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            // Feedback is staged one sample late (not read-before-write) so a delayMs = 0
            // tap still reads exactly what this same sample just wrote.
            const float combined = monoIn[s] + m_pendingFeedback;
            m_delay.write(kFeedbackHeadroom * std::tanh(combined / kFeedbackHeadroom));
            float sampleFeedback = 0.f;
            for (size_t t = 0; t < m_activeTaps; ++t)
            {
                float tapOut = 0.f;
                for (size_t slot = 0; slot < 2; ++slot)
                {
                    const size_t idx = slotIndex(t, slot);
                    const float slotGainValue = slotGain(t, slot).getValue();
                    tapOut += stepVoice(idx, m_delay.readTap(idx)) * slotGainValue;
                }
                const float gain = m_gain[t].getValue();
                wetL[s] += tapOut * gain * m_panL[t].getValue();
                wetR[s] += tapOut * gain * m_panR[t].getValue();
                sampleFeedback += tapOut * m_tapFeedback[t].getValue();
            }
            sampleFeedback += m_delay.readTap(kGlobalFeedbackSlot) * m_feedback.getValue();
            m_pendingFeedback = sampleFeedback;
        }

        // The reverb tails the dry/wet mix the listener actually hears, not the tap
        // bank's own output alone, so it picks up the dry signal too.
        std::array<float, BlockSize> fdnIn{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            const float dry = m_dry.getValue();
            const float wet = m_wet.getValue();
            out(s, 0) = dry * in(s, 0) + wet * wetL[s];
            out(s, 1) = dry * in(s, 1) + wet * wetR[s];
            fdnIn[s] = 0.5f * (out(s, 0) + out(s, 1));
        }

        std::array<float, BlockSize> reverbL{};
        std::array<float, BlockSize> reverbR{};
        m_fdn.processBlockSplit(fdnIn.data(), reverbL.data(), reverbR.data());

        for (size_t s = 0; s < BlockSize; ++s)
        {
            const float reverbWet = m_reverbWet.getValue();
            out(s, 0) += reverbWet * reverbL[s];
            out(s, 1) += reverbWet * reverbR[s];
        }
    }

  private:
    // Same exact-equality reasoning as DroneScriptEngine's notifyTimingIfChanged(): a
    // stored float either stays bit-identical or is genuinely a new host/UI value.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < SpectraltapScriptEngine::kMaxLuaParams; ++i)
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
        for (size_t i = 0; i < SpectraltapScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

    [[nodiscard]] static size_t clampDivisionIndex(const int index) noexcept
    {
        return static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kNumDivisions) - 1));
    }

    // OnTiming() is user Lua code, so it only runs when bpm/division actually change, not
    // every block - otherwise a host bpm-automation ramp would run it constantly. Exact
    // equality is safe here: currentBpm() either repeats its last value bit-for-bit or not.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyTimingIfChanged(const float bpm) noexcept
    {
        if (bpm == m_lastNotifiedBpm && m_divisionIndex == m_lastNotifiedDivisionIndex)
        {
            return;
        }
        m_scriptEngine.notifyTiming(bpm, static_cast<int>(m_divisionIndex));
        m_lastNotifiedBpm = bpm;
        m_lastNotifiedDivisionIndex = m_divisionIndex;
    }
#pragma GCC diagnostic pop

    // Notifies the current bpm/division unconditionally, unlike notifyTimingIfChanged() -
    // see setScript()'s comment for why.
    void resendTiming() noexcept
    {
        const float bpm = currentBpm();
        m_scriptEngine.notifyTiming(bpm, static_cast<int>(m_divisionIndex));
        m_lastNotifiedBpm = bpm;
        m_lastNotifiedDivisionIndex = m_divisionIndex;
    }

    [[nodiscard]] size_t msToDelaySamples(const float delayMs) const noexcept
    {
        return static_cast<size_t>(std::clamp(delayMs, 0.f, kMaxDelayMs) * 0.001f * kAssumedSampleRate + 0.5f);
    }

    [[nodiscard]] size_t feedbackMsToDelaySamples(const float delayMs) const noexcept
    {
        return static_cast<size_t>(std::clamp(delayMs, 0.f, kMaxFeedbackDelayMs) * 0.001f * kAssumedSampleRate + 0.5f);
    }

    // Frequency/decay/formant targets are smoothed once per block (not per sample) in log
    // domain, per SpectraltapScriptEngine's parameter-semantics contract - so this is the
    // "sample rate" getValue() advances against when called once per processBlock().
    [[nodiscard]] float blockRate() const noexcept
    {
        return sampleRate() / static_cast<float>(BlockSize);
    }

    void applyPendingCommands()
    {
        if (const auto maxTaps = m_scriptEngine.drainMaxTapsCommand())
        {
            m_activeTaps = std::min(*maxTaps, kMaxTaps);
        }
        for (size_t i = 0; i < kMaxTaps; ++i)
        {
            if (const auto tap = m_scriptEngine.drainTapCommand(i))
            {
                applyTapTopology(i, *tap);
            }
            if (const auto freq = m_scriptEngine.drainFrequencyCommand(i))
            {
                applyFrequency(i, *freq);
            }
            if (const auto resonance = m_scriptEngine.drainResonanceCommand(i))
            {
                applyResonance(i, *resonance);
            }
            if (const auto formant = m_scriptEngine.drainFormantCommand(i))
            {
                applyFormant(i, *formant);
            }
            if (const auto gain = m_scriptEngine.drainGainCommand(i))
            {
                applyGain(i, *gain);
            }
            if (const auto pan = m_scriptEngine.drainPanCommand(i))
            {
                applyPan(i, *pan);
            }
            if (const auto feedback = m_scriptEngine.drainTapFeedbackCommand(i))
            {
                m_tapFeedback[i].newTransition(*feedback, kParamSmoothingSeconds, sampleRate());
            }
        }
    }

    [[nodiscard]] static constexpr size_t slotIndex(const size_t tap, const size_t slot) noexcept
    {
        return tap * 2 + slot;
    }

    [[nodiscard]] AbacDsp::LinearSmoothing& slotGain(const size_t tap, const size_t slot) noexcept
    {
        return slot == 0 ? m_slot0Gain[tap] : m_slot1Gain[tap];
    }

    // Topology tier, click-free: a tap's first-ever SetTap applies directly (slot 0, gain
    // forced to 1). Every later call configures the inactive slot fresh and crossfades it
    // in over kCrossfadeSeconds while the previously-active slot fades out.
    void applyTapTopology(const size_t i, const TapTopology& tap)
    {
        if (!m_tapConfigured[i])
        {
            m_tapConfigured[i] = true;
            m_activeSlot[i] = 0;
            configureSlot(i, 0, tap);
            slotGain(i, 0).forceCurrentValue(1.f);
            slotGain(i, 1).forceCurrentValue(0.f);
        }
        else
        {
            const size_t oldSlot = m_activeSlot[i];
            const size_t newSlot = 1 - oldSlot;
            configureSlot(i, newSlot, tap);
            slotGain(i, newSlot).newTransition(1.f, kCrossfadeSeconds, sampleRate());
            slotGain(i, oldSlot).newTransition(0.f, kCrossfadeSeconds, sampleRate());
            m_activeSlot[i] = newSlot;
        }
        applyGain(i, tap.level);
        applyPan(i, tap.pan);
    }

    void configureSlot(const size_t tap, const size_t slot, const TapTopology& topology)
    {
        const size_t idx = slotIndex(tap, slot);
        m_delay.setTapDelay(idx, msToDelaySamples(topology.delayMs));
        resetVoiceForType(idx, topology.type);
    }

    void resetVoiceForType(const size_t idx, const TapType type)
    {
        using namespace SpectraltapVoices;
        switch (type)
        {
            case TapType::Bypass:
                m_voices[idx] = BypassVoice{};
                break;
            case TapType::LowPass:
            case TapType::HighPass:
            case TapType::BandPass:
            case TapType::Notch:
            {
                FilterVoice voice{};
                voice.mode = type;
                voice.svf.setSampleRate(sampleRate());
                m_voices[idx] = voice;
                break;
            }
            case TapType::Resonator:
            {
                ResonatorVoice voice{};
                voice.svf.setSampleRate(sampleRate());
                m_voices[idx] = voice;
                break;
            }
            case TapType::Formant:
            {
                FormantVoice voice{};
                for (auto& section : voice.sections)
                {
                    section.setSampleRate(sampleRate());
                }
                m_voices[idx] = voice;
                break;
            }
            case TapType::CombResonator:
                m_voices[idx] = CombVoice{};
                m_combs[idx].reset();
                break;
            case TapType::RingModulator:
                m_voices[idx] = RingModVoice{};
                break;
        }
    }

    void applyGain(const size_t i, const float value) noexcept
    {
        m_gain[i].newTransition(value, kParamSmoothingSeconds, sampleRate());
    }

    // Smooths the constant-power L/R gains themselves, not the raw pan value - see
    // SpectraltapScriptEngine's class doc / the example README's pan-law section.
    void applyPan(const size_t i, const float pan) noexcept
    {
        const float angle = (pan + 1.f) * (std::numbers::pi_v<float> / 4.f);
        m_panL[i].newTransition(std::cos(angle), kParamSmoothingSeconds, sampleRate());
        m_panR[i].newTransition(std::sin(angle), kParamSmoothingSeconds, sampleRate());
    }

    void applyFrequency(const size_t i, const float freqHz) noexcept
    {
        m_freqLog2[i].newTransition(std::log2(freqHz), kBlockSmoothingSeconds, blockRate());
    }

    void applyResonance(const size_t i, const ResonanceTarget& resonance) noexcept
    {
        m_combNegative[i] = resonance.negative;
        m_freqLog2[i].newTransition(std::log2(resonance.freqHz), kBlockSmoothingSeconds, blockRate());
        m_decayLog2[i].newTransition(std::log2(resonance.decaySeconds), kBlockSmoothingSeconds, blockRate());
    }

    void applyFormant(const size_t i, const FormantTarget& formant) noexcept
    {
        constexpr float kMinSectionFreqHz{0.1f};
        m_freqLog2[i].newTransition(std::log2(formant.freqHz), kBlockSmoothingSeconds, blockRate());
        m_formantF1Log2[i].newTransition(std::log2(std::max(kMinSectionFreqHz, formant.f1Factor * formant.freqHz)),
                                         kBlockSmoothingSeconds, blockRate());
        m_formantF2Log2[i].newTransition(std::log2(std::max(kMinSectionFreqHz, formant.f2Factor * formant.freqHz)),
                                         kBlockSmoothingSeconds, blockRate());
        m_formantF1Gain[i].newTransition(formant.f1Gain, kBlockSmoothingSeconds, blockRate());
        m_formantF2Gain[i].newTransition(formant.f2Gain, kBlockSmoothingSeconds, blockRate());
    }

    // Own, much smaller-scaled constant than SvfResoBP's native decay-to-Q relation
    // (tuned for impulse ring-down time, not continuous drive) - see kFilterDecayToQ.
    [[nodiscard]] static float filterQFromDecay(const float freqHz, const float decaySeconds) noexcept
    {
        return std::clamp(std::numbers::pi_v<float> * freqHz * decaySeconds * kFilterDecayToQ, 0.05f, kMaxFilterQ);
    }

    // Recomputes every active tap's filter/comb coefficients from its (block-rate
    // smoothed) frequency/decay targets - once per block, per the same "static or slowly
    // changing parameters" allowance SpectraltapScriptEngine's class doc documents.
    void updateTapCoefficients() noexcept
    {
        using namespace SpectraltapVoices;
        for (size_t i = 0; i < m_activeTaps; ++i)
        {
            const size_t idx = slotIndex(i, m_activeSlot[i]);
            const float freqHz = std::min(std::exp2(m_freqLog2[i].getValue()), kMaxTapFreqHz);
            const float decaySeconds = std::exp2(m_decayLog2[i].getValue());
            std::visit(Overloaded{[&](BypassVoice&) {}, [&](FilterVoice& voice)
                                  { voice.svf.computeCoefficients(freqHz, filterQFromDecay(freqHz, decaySeconds)); },
                                  [&](ResonatorVoice& voice)
                                  {
                                      const float q = filterQFromDecay(freqHz, decaySeconds);
                                      voice.svf.computeCoefficients(0, freqHz, q);
                                      voice.gainBoost = q;
                                  },
                                  [&](FormantVoice& voice) { updateFormantVoice(i, voice); },
                                  [&](CombVoice&) { m_combs[idx].setByDecay(freqHz, decaySeconds, m_combNegative[i]); },
                                  [&](RingModVoice& voice)
                                  { voice.phaseIncrement = 2.f * std::numbers::pi_v<float> * freqHz / sampleRate(); }},
                       m_voices[idx]);
        }
    }

    // Global feedback tap's read position, as a beat count converted from currentBpm() -
    // native counterpart to Rhythm.BeatsToMs, since this dial isn't Lua-driven.
    void updateFeedbackDelay() noexcept
    {
        const float delayMs = m_feedbackBeats * 60000.f / std::max(currentBpm(), 1.f);
        m_delay.setTapDelay(kGlobalFeedbackSlot, feedbackMsToDelaySamples(delayMs));
    }

    void updateFormantVoice(const size_t i, SpectraltapVoices::FormantVoice& voice) noexcept
    {
        const float f1 = std::min(std::exp2(m_formantF1Log2[i].getValue()), kMaxTapFreqHz);
        const float f2 = std::min(std::exp2(m_formantF2Log2[i].getValue()), kMaxTapFreqHz);
        const float f0 = std::min(std::exp2(m_freqLog2[i].getValue()), kMaxTapFreqHz);
        voice.sections[0].computeCoefficients(0, f0, kFormantSectionQ);
        voice.sections[1].computeCoefficients(0, f1, kFormantSectionQ);
        voice.sections[2].computeCoefficients(0, f2, kFormantSectionQ);
        voice.f1Gain = m_formantF1Gain[i].getValue();
        voice.f2Gain = m_formantF2Gain[i].getValue();
    }

    [[nodiscard]] float stepVoice(const size_t i, const float in) noexcept
    {
        using namespace SpectraltapVoices;
        return std::visit(Overloaded{[&](BypassVoice&) { return in; },
                                     [&](FilterVoice& voice) -> float
                                     {
                                         const auto out = voice.svf.step(in);
// voice.mode is only ever one of these four (see resetVoiceForType); the other TapType
// values never reach a FilterVoice.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
                                         switch (voice.mode)
                                         {
                                             case TapType::LowPass:
                                                 return out.low;
                                             case TapType::HighPass:
                                                 return out.high;
                                             case TapType::BandPass:
                                                 return out.band;
                                             default: // Notch
                                                 return out.notch;
                                         }
#pragma GCC diagnostic pop
                                     },
                                     [&](ResonatorVoice& voice) { return voice.svf.step(in) * voice.gainBoost; },
                                     [&](FormantVoice& voice) -> float
                                     {
                                         const float s0 = voice.sections[0].step(in) * kFormantSectionQ;
                                         const float s1 = voice.sections[1].step(in) * kFormantSectionQ;
                                         const float s2 = voice.sections[2].step(in) * kFormantSectionQ;
                                         const float norm =
                                             1.f / (1.f + std::abs(voice.f1Gain) + std::abs(voice.f2Gain));
                                         return (s0 + s1 * voice.f1Gain + s2 * voice.f2Gain) * norm;
                                     },
                                     [&](CombVoice&) { return m_combs[i].step(in); },
                                     [&](RingModVoice& voice) -> float
                                     {
                                         const float carrier = std::sin(voice.phase);
                                         voice.phase += voice.phaseIncrement;
                                         constexpr float kTwoPi = 2.f * std::numbers::pi_v<float>;
                                         if (voice.phase >= kTwoPi)
                                         {
                                             voice.phase -= kTwoPi;
                                         }
                                         return in * carrier;
                                     }},
                          m_voices[i]);
    }

    static constexpr float kParamSmoothingSeconds{0.01f};
    static constexpr float kBlockSmoothingSeconds{0.03f};

    size_t m_activeTaps{0};
    float m_manualBpm{120.f};
    bool m_hostSync{false};
    size_t m_divisionIndex{4}; // "1/4", matching the Division dropdown's default
    float m_lastNotifiedBpm{-1.f};
    size_t m_lastNotifiedDivisionIndex{static_cast<size_t>(-1)};
    AbacDsp::LinearSmoothing m_dry{1.f};
    AbacDsp::LinearSmoothing m_wet{1.f};
    AbacDsp::LinearSmoothing m_reverbWet{0.f};
    AbacDsp::LinearSmoothing m_feedback{0.f};
    float m_feedbackBeats{1.f}; // matching the Feedback Time dial's default
    // Carries this sample's tap/global feedback sum into the *next* sample's write, so a
    // delayMs = 0 tap still reads exactly what its own sample just wrote.
    float m_pendingFeedback{0.f};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_tapFeedback{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>(0.f)};

    // Per logical tap: which of its two slots (see slotIndex()) is currently live, and
    // whether it has ever been configured at all (a first-ever SetTap skips the fade).
    std::array<size_t, kMaxTaps> m_activeSlot{};
    std::array<bool, kMaxTaps> m_tapConfigured{};
    std::array<bool, kMaxTaps> m_combNegative{}; // CombResonator only; SetResonance's negative
    // Both start silent (0) so an active-but-never-configured tap (SetMaxTaps raised
    // before its own SetTap arrived) contributes nothing instead of double-counting.
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_slot0Gain{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>(0.f)};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_slot1Gain{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>(0.f)};

    AbacDsp::MultiTapDelay<kMaxDelaySamples, kMaxSlots + 1> m_delay{};
    std::array<SpectraltapVoices::TapVoice, kMaxSlots> m_voices{};
    // Constructed once (see the constructor's init list) and reused by index for the
    // plugin's lifetime - see SpectraltapVoices::CombVoice's class doc for why.
    std::array<AbacDsp::CombResonator<kCombBufferSize>, kMaxSlots> m_combs;
    Fdn m_fdn;

    // LinearSmoothing's ctor is explicit, which trips up aggregate-init on array elements
    // past the first - constructArray() direct-initializes every element instead.
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_gain{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_panL{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_panR{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_freqLog2{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_decayLog2{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_formantF1Log2{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_formantF2Log2{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_formantF1Gain{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};
    std::array<AbacDsp::LinearSmoothing, kMaxTaps> m_formantF2Gain{
        AbacDsp::constructArray<AbacDsp::LinearSmoothing, kMaxTaps>()};

    SpectraltapScriptEngine m_scriptEngine;
    std::array<float, SpectraltapScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, SpectraltapScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{-1.f, -1.f, -1.f, -1.f,
                                                                                           -1.f, -1.f, -1.f, -1.f};
};
