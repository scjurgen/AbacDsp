#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/OnePoleFilter.h"
#include "Filters/PoleMixingFilter.h"
#include "Generators/SynthLfo.h"
#include "Helpers/ConstructArray.h"
#include "MorphexsynthScriptEngine.h"
#include "Numbers/Convert.h"
#include "Parameters/SmoothingParameter.h"
#include "Reverbs/FdnTankGlide.h"
#include "Reverbs/ModulationDelayNoFeedback.h"
#include "SynthHandling/SustainPedalHandler.h"
#include "Synthesizer/MorphexsynthVoice.h"

template <size_t BlockSize>
class MorphexsynthImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxVoices{12};

    explicit MorphexsynthImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_voices(AbacDsp::constructArray<AbacDsp::MorphexsynthVoice, kMaxVoices>(sampleRate, m_waveShaperTables,
                                                                                   m_curveMap))
        , m_dcBlocker(
              AbacDsp::constructArray<AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>, 2>(
                  sampleRate, 20.f))
        , m_phaser(AbacDsp::constructArray<AbacDsp::Phaser24Smooth, kPhaserTotalStages>(sampleRate))
        , m_phaserLfo(sampleRate)
        , m_chorusDelay(
              AbacDsp::constructArray<AbacDsp::ModulationDelayNoFeedback<kChorusMaxDelaySamples>, 2>(sampleRate))
        , m_reverb(sampleRate)
    {
        m_sustainPedal.configureCallbacks(
            [this](const int channel, const int note, const int velocity) { allocateVoice(channel, note, velocity); },
            [this](const int channel, const int note, const int) { releaseVoice(channel, note); });
        for (auto& stage : m_phaser)
        {
            stage.setSmoothingSteps(BlockSize);
        }
        m_phaserLfo.setWaveForm(AbacDsp::LfoType::Sine);
        for (auto& delay : m_chorusDelay)
        {
            delay.setWidthInMsecs(kChorusBaseDelayMs);
        }
        setChorus(kDefaultChorusRateHz, kDefaultChorusDepth, m_chorusMix);
        // Stereo width: prime the right channel a half modulation cycle out of phase with
        // the left by running it through that many silent samples once, up front.
        const auto halfPeriodSamples = static_cast<size_t>(sampleRate / (2.f * kDefaultChorusRateHz));
        for (size_t i = 0; i < halfPeriodSamples; ++i)
        {
            (void) m_chorusDelay[1].step(0.f); // priming the phase offset, not producing audio yet
        }
        setPhaser(kDefaultPhaserRateHz, m_phaserDepth, kDefaultPhaserFeedback, m_phaserMix);
        // morphexsynth has no Play switch/Host Sync concept (unlike most OnStart() users -
        // see LuaScriptEngineBase's own skeleton comment): fires exactly once here, when the
        // engine is ready, the usual place for a script to set its patch's static config.
        m_scriptEngine.notifyStart();
    }

    void setVol(const float value) noexcept
    {
        m_vol = Convert::dbToGain(value);
    }

    void setCutoff(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setCutoff(value);
        }
    }

    void setResonance(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setResonance(value * 0.01f);
        }
    }

    void setReverbSize(const float meters) noexcept
    {
        m_reverb.setMinSize(meters / kFdnSizeSpread);
        m_reverb.setMaxSize(meters * kFdnSizeSpread);
    }

    void setReverbDecay(const float milliseconds) noexcept
    {
        m_reverb.setDecay(milliseconds);
    }

    void setReverbMix(const float valueDb) noexcept
    {
        m_reverbMix.newTransition(Convert::dbToGain(valueDb), kFxSmoothingSeconds, sampleRate());
    }

    void setReverbDry(const float valueDb) noexcept
    {
        m_reverbDry.newTransition(Convert::dbToGain(valueDb), kFxSmoothingSeconds, sampleRate());
    }

    // Both stages of both channels share one sweep and one feedback amount - a deliberately
    // mono-swept phaser, not a stereo-width one (see the plan's flagged risk).
    void setPhaser(const float rateHz, const float depth, const float feedback, const float mix) noexcept
    {
        m_phaserDepth = depth;
        m_phaserMix = mix;
        // Stepped once per block rather than per sample (see updatePhaserCutoff()), so its
        // per-call phase advance must cover a whole block's worth of samples at once.
        m_phaserLfo.setFrequency(rateHz * static_cast<float>(BlockSize));
        for (auto& stage : m_phaser)
        {
            stage.setResonance(feedback);
        }
    }

    void setChorus(const float rateHz, const float depth, const float mix) noexcept
    {
        m_chorusMix = mix;
        for (auto& delay : m_chorusDelay)
        {
            delay.setModSpeed(rateHz);
            delay.setModDepth(depth * kChorusMaxModDepth);
        }
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

    bool setScript(const std::string_view source)
    {
        const bool ok = m_scriptEngine.loadScript(source);
        if (ok)
        {
            // Every patch parameter a script can touch (oscillators, envelopes, LFO, filter,
            // distortion, the MPE matrix, the MPE zone) must not carry over from whatever the
            // previous script left behind - a script's behavior must not depend on load order.
            resetVoicesToDefaults();
            resendUiParameters();
            // The constructor fires OnStart() once for the initial stub load; every later
            // setScript() (a new patch, the script editor's Apply) needs its own OnStart()
            // too, or that script's own one-time setup (SetOscillator, ...) never runs.
            m_scriptEngine.notifyStart();
            applyPendingScriptCommands();
        }
        return ok;
    }

    void setImportResolver(MorphexsynthScriptEngine::ImportResolver resolver)
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
        return std::string(MorphexsynthScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const MorphexsynthScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    // 1-indexed, matching MIDI channel numbers as shown to a user.
    void setMpeMasterChannel(const int channel) noexcept
    {
        m_mpeMaster = std::clamp(channel - 1, 0, 15);
    }

    void setMpeRangeLowerChannel(const int channel) noexcept
    {
        m_mpeLower = std::clamp(channel - 1, 0, 15);
    }

    void setMpeRangeUpperChannel(const int channel) noexcept
    {
        m_mpeUpper = std::clamp(channel - 1, 0, 15);
    }

    // Channel Voice messages only, gated to the configured MPE zone (master, or lower..upper
    // member channels); anything outside that is ignored. Z reads Channel Pressure (0xD0), the
    // canonical MPE source, not the original's hardware-specific Poly Pressure (0xA0) reuse.
    void processMidi(const uint8_t* msg) override
    {
        const int channel = msg[0] & 0x0F;
        if (channel != m_mpeMaster && (channel < m_mpeLower || channel > m_mpeUpper))
        {
            return;
        }

        switch (msg[0] & 0xF0)
        {
            case 0x90: // Note On; velocity 0 is a Note Off per MIDI running-status convention
                if (msg[2] == 0)
                {
                    m_sustainPedal.noteOff(channel, msg[1], 0x40);
                    m_scriptEngine.notifyNoteOff(channel, msg[1], 0x40);
                }
                else
                {
                    // Notify (letting a script queue e.g. a velocity-scaled SetFilterEnvelope)
                    // and apply before triggering, so the voice about to sound picks up
                    // whatever this note's OnNoteOn just set, not last block's settings.
                    m_scriptEngine.notifyNoteOn(channel, msg[1], msg[2]);
                    applyPendingScriptCommands();
                    m_sustainPedal.noteOn(channel, msg[1], msg[2]);
                }
                break;
            case 0x80: // Note Off
                m_sustainPedal.noteOff(channel, msg[1], msg[2]);
                m_scriptEngine.notifyNoteOff(channel, msg[1], msg[2]);
                break;
            case 0xB0: // Control Change
                if (msg[1] == kCcSustainPedal)
                {
                    m_sustainPedal.setSustain(msg[2] >= 64);
                }
                else if (msg[1] == kCcTimbre || msg[1] == kCcModWheel)
                {
                    mpeY(channel, msg[2] << 7);
                }
                else if (msg[1] == kCcAllNotesOff)
                {
                    allNotesOff();
                }
                m_scriptEngine.notifyCC(channel, msg[1], msg[2]);
                break;
            case 0xC0: // Program Change - no C++ handling, forwarded to the script only
                m_scriptEngine.notifyProgramChange(channel, msg[1]);
                break;
            case 0xA0: // Polyphonic Key Pressure - no C++ handling, forwarded to the script only
                m_scriptEngine.notifyPolyPressure(channel, msg[1], msg[2]);
                break;
            case 0xD0: // Channel Pressure
                mpeZ(channel, msg[1] << 7);
                m_scriptEngine.notifyAftertouch(channel, msg[1]);
                break;
            case 0xE0: // Pitch Bend: 14-bit 0..16383 (center 8192), re-centered to -8192..8191
            {
                const int bendValue = ((static_cast<int>(msg[2]) << 7) | msg[1]) - 8192;
                mpeX(channel, bendValue);
                m_scriptEngine.notifyPitchBend(channel, bendValue);
                break;
            }
            default:
                break;
        }
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();
        applyPendingScriptCommands();

        std::array<float, BlockSize> left{};
        std::array<float, BlockSize> right{};
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            if (!m_voices[v].isPlaying())
            {
                continue;
            }
            std::array<float, BlockSize> voiceLeft{};
            std::array<float, BlockSize> voiceRight{};
            m_voices[v].processBlock(voiceLeft.data(), voiceRight.data(), BlockSize);
            for (size_t s = 0; s < BlockSize; ++s)
            {
                left[s] += voiceLeft[s];
                right[s] += voiceRight[s];
            }
        }

        m_dcBlocker[0].processBlock(left.data(), BlockSize);
        m_dcBlocker[1].processBlock(right.data(), BlockSize);

        processPhaser(left, right);
        processChorus(left, right);
        processReverb(left, right);

        for (size_t s = 0; s < BlockSize; ++s)
        {
            out(s, 0) = in(s, 0) + left[s] * m_vol;
            out(s, 1) = in(s, 1) + right[s] * m_vol;
        }
    }

  private:
    static constexpr uint8_t kCcSustainPedal{64};
    static constexpr uint8_t kCcModWheel{1};
    static constexpr uint8_t kCcTimbre{74};
    static constexpr uint8_t kCcAllNotesOff{123};

    static constexpr size_t kPhaserStagesPerChannel{2}; // 2 in series = 8 allpass poles total
    static constexpr size_t kPhaserTotalStages{kPhaserStagesPerChannel * 2};
    static constexpr float kPhaserMinHz{200.f};
    static constexpr float kPhaserMaxHz{2000.f};
    static constexpr float kDefaultPhaserRateHz{0.3f};
    static constexpr float kDefaultPhaserFeedback{0.f};

    static constexpr size_t kChorusMaxDelaySamples{4096}; // covers kChorusBaseDelayMs at 48 kHz with headroom
    static constexpr float kChorusBaseDelayMs{18.f};
    static constexpr float kChorusMaxModDepth{0.6f}; // fraction of ModulationDelayNoFeedback's own depth scale
    static constexpr float kDefaultChorusRateHz{0.6f};
    static constexpr float kDefaultChorusDepth{0.5f};

    static constexpr size_t kFdnOrder{32};
    static constexpr size_t kFdnMaxSizePerElement{100000};
    static constexpr float kFdnSizeSpread{2.3f}; // Size dial value / *this .. Size dial value * this
    static constexpr float kFxSmoothingSeconds{0.01f};

    using Fdn = AbacDsp::FdnTankGlide<kFdnMaxSizePerElement, kFdnOrder, BlockSize>;

    // One sweep per block (not per sample) into every phaser stage's own ramped setCutoff();
    // see setPhaser()'s comment on why the LFO's frequency is pre-scaled for this.
    void updatePhaserCutoff() noexcept
    {
        const float lfo = m_phaserLfo.step();
        const float logMin = std::log(kPhaserMinHz);
        const float logMax = std::log(kPhaserMaxHz);
        const float center = 0.5f * (logMin + logMax);
        const float halfRange = 0.5f * (logMax - logMin) * m_phaserDepth;
        const float cutoffHz = std::exp(center + halfRange * lfo);
        for (auto& stage : m_phaser)
        {
            stage.setCutoff(cutoffHz);
        }
    }

    void processPhaser(std::array<float, BlockSize>& left, std::array<float, BlockSize>& right) noexcept
    {
        updatePhaserCutoff();
        const std::array<float, BlockSize> dryLeft{left};
        const std::array<float, BlockSize> dryRight{right};
        std::array<float, BlockSize> tmp{};
        m_phaser[0].processBlock(left.data(), tmp.data(), BlockSize);
        m_phaser[1].processBlock(tmp.data(), left.data(), BlockSize);
        m_phaser[2].processBlock(right.data(), tmp.data(), BlockSize);
        m_phaser[3].processBlock(tmp.data(), right.data(), BlockSize);
        for (size_t s = 0; s < BlockSize; ++s)
        {
            left[s] = dryLeft[s] * (1.f - m_phaserMix) + left[s] * m_phaserMix;
            right[s] = dryRight[s] * (1.f - m_phaserMix) + right[s] * m_phaserMix;
        }
    }

    void processChorus(std::array<float, BlockSize>& left, std::array<float, BlockSize>& right) noexcept
    {
        for (size_t s = 0; s < BlockSize; ++s)
        {
            const float wetLeft = m_chorusDelay[0].step(left[s]);
            const float wetRight = m_chorusDelay[1].step(right[s]);
            left[s] = left[s] * (1.f - m_chorusMix) + wetLeft * m_chorusMix;
            right[s] = right[s] * (1.f - m_chorusMix) + wetRight * m_chorusMix;
        }
    }

    void processReverb(std::array<float, BlockSize>& left, std::array<float, BlockSize>& right) noexcept
    {
        std::array<float, BlockSize> mono{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            mono[s] = 0.5f * (left[s] + right[s]);
        }
        std::array<float, BlockSize> wetLeft{};
        std::array<float, BlockSize> wetRight{};
        m_reverb.processBlockSplit(mono.data(), wetLeft.data(), wetRight.data());
        for (size_t s = 0; s < BlockSize; ++s)
        {
            const float dry = m_reverbDry.getValue();
            const float wet = m_reverbMix.getValue();
            left[s] = left[s] * dry + wetLeft[s] * wet;
            right[s] = right[s] * dry + wetRight[s] * wet;
        }
    }

    struct VoiceState
    {
        int channel{-1};
        int note{-1};
        size_t assignedAt{0};
    };

    // Reuses any silent voice first, else steals the one assigned longest ago (no frozen-voice
    // exclusion, no overflow queue, per the plan's Phase 0 trim). Envelope<N>::trigger() ramps
    // from a stolen voice's current gain rather than a hard jump, its only declick for v1.
    void allocateVoice(const int channel, const int note, const int velocity) noexcept
    {
        size_t target = 0;
        size_t oldestAssignedAt = m_noteCounter;
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (!m_voices[i].isPlaying())
            {
                target = i;
                break;
            }
            if (m_voiceState[i].assignedAt < oldestAssignedAt)
            {
                oldestAssignedAt = m_voiceState[i].assignedAt;
                target = i;
            }
        }

        m_voices[target].triggerVoice(note, velocity, m_lastNote);
        m_voiceState[target] = VoiceState{channel, note, m_noteCounter++};
        m_lastNote = note;
    }

    void releaseVoice(const int channel, const int note) noexcept
    {
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (m_voiceState[i].channel == channel && m_voiceState[i].note == note)
            {
                m_voices[i].stopVoice();
                return;
            }
        }
    }

    void allNotesOff() noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.stopVoice();
        }
    }

    void mpeX(const int channel, const int bendValue14Bit) noexcept
    {
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (m_voiceState[i].channel == channel && m_voices[i].isPlaying())
            {
                m_voices[i].mpeX(bendValue14Bit);
            }
        }
    }

    void mpeY(const int channel, const int value14Bit) noexcept
    {
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (m_voiceState[i].channel == channel && m_voices[i].isPlaying())
            {
                m_voices[i].mpeY(value14Bit);
            }
        }
    }

    void mpeZ(const int channel, const int value14Bit) noexcept
    {
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (m_voiceState[i].channel == channel && m_voices[i].isPlaying())
            {
                m_voices[i].mpeZ(value14Bit);
            }
        }
    }

    // A reload resets the script's Lua globals, so resendUiParameters() re-syncs it to
    // each claimed slot's current value - otherwise it stays believing coded defaults.
    void resendUiParameters() noexcept
    {
        for (size_t i = 0; i < MorphexsynthScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

    // Same exact-equality reasoning as Pingsynth's own version: a stored float either
    // stays bit-identical or is genuinely a new host/UI value.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < MorphexsynthScriptEngine::kMaxLuaParams; ++i)
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

    // Destroys and reconstructs every voice in place, guaranteeing every patch parameter
    // lands back at MorphexsynthVoice's own as-constructed defaults - by construction, not
    // by a hand-maintained list that could silently miss a future SetXxx() addition.
    void resetVoicesToDefaults()
    {
        for (auto& voice : m_voices)
        {
            std::destroy_at(&voice);
            std::construct_at(&voice, sampleRate(), m_waveShaperTables, m_curveMap);
        }
        m_voiceState = std::array<VoiceState, kMaxVoices>{};
        m_noteCounter = 0;
        m_lastNote = 60;
        m_mpeMaster = 0;
        m_mpeLower = 1;
        m_mpeUpper = 15;
        m_sustainPedal.allNotesOff();
    }

    // Drains every SetXxx() command a script queued since the last block and, if present,
    // broadcasts it to every voice - the same all-voice-broadcast pattern setCutoff()/
    // setResonance() already use, since morphexsynth has no per-note patch parameters.
    void applyPendingScriptCommands() noexcept
    {
        for (size_t i = 0; i < AbacDsp::MorphexsynthVoice::kNumOscillators; ++i)
        {
            if (const auto osc = m_scriptEngine.drainOscillatorCommand(i))
            {
                for (auto& voice : m_voices)
                {
                    voice.setWaveForm(i, osc->waveform);
                    voice.setDetune(i, osc->detune);
                    voice.setLevelOscillator(i, osc->level);
                    voice.setPwmOscillator(i, osc->pwm);
                    voice.setPitchFactor(i, osc->pitchFactor);
                }
            }
        }
        if (const auto amp = m_scriptEngine.drainAmpEnvelopeCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setEnvelopeAttack(amp->attackMs);
                voice.setEnvelopeDecay(amp->decayMs);
                voice.setEnvelopeSustainLevel(amp->sustainLevel);
                voice.setEnvelopeRelease(amp->releaseMs);
            }
        }
        if (const auto filterEnv = m_scriptEngine.drainFilterEnvelopeCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setEnvelopeAttackFilter(filterEnv->attackMs);
                voice.setEnvelopeDecayFilter(filterEnv->decayMs);
                voice.setEnvelopeSustainLevelFilter(filterEnv->sustainLevel);
                voice.setEnvelopeReleaseFilter(filterEnv->releaseMs);
            }
        }
        if (const auto pitchEnv = m_scriptEngine.drainPitchEnvelopeCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setPitchAttack(pitchEnv->attackMs);
                voice.setPitchDecay(pitchEnv->decayMs);
                voice.setPitchEnvelopeDepth(pitchEnv->depthSemitones);
                voice.setGlide(pitchEnv->glideMsPerOctave);
            }
        }
        if (const auto lfo = m_scriptEngine.drainLfoCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setLfoWaveForm(static_cast<AbacDsp::LfoType>(lfo->waveform));
                voice.setLfoPitchFactor(lfo->speedHz);
                voice.setLfoFilterModulationDepth(lfo->filterDepth);
                voice.setLfoOscModulationDepth(lfo->oscDepth);
                voice.setLfoKeyFollow(lfo->keyFollow);
            }
        }
        if (const auto filter = m_scriptEngine.drainFilterCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setCutoff(filter->cutoff);
                voice.setResonance(filter->resonance);
                voice.setFilterType(filter->type);
            }
        }
        if (const auto distortion = m_scriptEngine.drainDistortionCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setPresetWaveTable(*distortion);
            }
        }
        for (size_t slot = 0; slot < AbacDsp::MorphexsynthVoice::kNumMpeSlots; ++slot)
        {
            if (const auto ctrl = m_scriptEngine.drainCtrlSlotCommand(slot))
            {
                for (auto& voice : m_voices)
                {
                    voice.setCtrlDimension(slot, static_cast<AbacDsp::CtrlDimension>(ctrl->source));
                    voice.setCtrlCurve(slot, static_cast<AbacDsp::CtrlCurve>(ctrl->curve));
                    voice.setCtrlTarget(slot, static_cast<AbacDsp::CtrlTarget>(ctrl->target));
                    voice.setCtrlType(slot, static_cast<AbacDsp::CtrlValueType>(ctrl->valueType));
                    voice.setCtrlDepth(slot, ctrl->depth);
                }
            }
        }
        if (const auto zone = m_scriptEngine.drainMpeZoneCommand())
        {
            setMpeMasterChannel(zone->master);
            setMpeRangeLowerChannel(zone->lower);
            setMpeRangeUpperChannel(zone->upper);
        }
        if (const auto phaser = m_scriptEngine.drainPhaserCommand())
        {
            setPhaser(phaser->rateHz, phaser->depth, phaser->feedback, phaser->mix);
        }
        if (const auto chorus = m_scriptEngine.drainChorusCommand())
        {
            setChorus(chorus->rateHz, chorus->depth, chorus->mix);
        }
    }

    AbacDsp::WaveShaperTableStore m_waveShaperTables{};
    AbacDsp::MpeCurveMap m_curveMap{};
    std::array<AbacDsp::MorphexsynthVoice, kMaxVoices> m_voices;
    std::array<AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>, 2> m_dcBlocker;
    std::array<VoiceState, kMaxVoices> m_voiceState{};

    // [channel * kPhaserStagesPerChannel + stage]
    std::array<AbacDsp::Phaser24Smooth, kPhaserTotalStages> m_phaser;
    AbacDsp::LfoGenerators m_phaserLfo;
    float m_phaserDepth{0.5f};
    float m_phaserMix{0.f};

    std::array<AbacDsp::ModulationDelayNoFeedback<kChorusMaxDelaySamples>, 2> m_chorusDelay;
    float m_chorusMix{0.f};

    Fdn m_reverb;
    AbacDsp::LinearSmoothing m_reverbDry{1.f};
    AbacDsp::LinearSmoothing m_reverbMix{0.f};

    AbacDsp::SustainPedalHandler m_sustainPedal{kMaxVoices};
    size_t m_noteCounter{0};
    int m_lastNote{60};

    int m_mpeMaster{0};
    int m_mpeLower{1};
    int m_mpeUpper{15};

    float m_vol{1.f};

    MorphexsynthScriptEngine m_scriptEngine{};
    std::array<float, MorphexsynthScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, MorphexsynthScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{};
};
