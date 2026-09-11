#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

#include "AmbientSynthScriptEngine.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/OnePoleFilter.h"
#include "Filters/PoleMixingFilter.h"
#include "Generators/SynthLfo.h"
#include "Harmony/PitchClassSet.h"
#include "Helpers/ConstructArray.h"
#include "NonLinear/WaveShaperTables.h"
#include "Numbers/Convert.h"
#include "Parameters/SmoothingParameter.h"
#include "Reverbs/FdnTankGlide.h"
#include "Reverbs/ModulationDelayNoFeedback.h"
#include "Synthesizer/AmbientSynthVoice.h"

template <size_t BlockSize>
class AmbientSynthImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxVoices{16}; ///< must match AmbientSynthScriptEngine::kMaxChannels

    explicit AmbientSynthImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_voices(AbacDsp::constructArray<AbacDsp::AmbientSynthVoice, kMaxVoices>(sampleRate, m_waveShaperTables))
        , m_dcBlocker(
              AbacDsp::constructArray<AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>, 2>(
                  sampleRate, 20.f))
        , m_phaser(AbacDsp::constructArray<AbacDsp::Phaser24Smooth, kPhaserTotalStages>(sampleRate))
        , m_phaserLfo(sampleRate)
        , m_chorusDelay(
              AbacDsp::constructArray<AbacDsp::ModulationDelayNoFeedback<kChorusMaxDelaySamples>, 2>(sampleRate))
        , m_reverb(sampleRate)
    {
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
        // the left, same trick ambientpad/morphexsynth use - the voices themselves stay mono.
        const auto halfPeriodSamples = static_cast<size_t>(sampleRate / (2.f * kDefaultChorusRateHz));
        for (size_t i = 0; i < halfPeriodSamples; ++i)
        {
            (void) m_chorusDelay[1].step(0.f);
        }
        setPhaser(kDefaultPhaserRateHz, m_phaserDepth, kDefaultPhaserFeedback, m_phaserMix);
        setReverbSize(kDefaultReverbSizeMeters);
        setReverbDecay(kDefaultReverbDecayMs);
        setReverbDry(kDefaultReverbDryDb);
        setReverbMix(kDefaultReverbMixDb);
        m_level.newTransition(Convert::dbToGain(kDefaultLevelDb), 0.f, sampleRate);
        m_scriptEngine.notifyStart();
    }

    void setLevel(const float valueDb) noexcept
    {
        m_level.newTransition(Convert::dbToGain(valueDb), kFxSmoothingSeconds, sampleRate());
    }

    void setMaterial(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setMaterial(value);
        }
    }

    void setMaterialRange(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setMaterialRange(value);
        }
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
            voice.setResonance(value);
        }
    }

    /// @brief index is a FilterType.* value (see AmbientSynthScriptEngine's Lua table);
    /// out-of-range values are ignored.
    void setFilterType(const int index) noexcept
    {
        if (index < 0 || index >= static_cast<int>(AmbientSynthScriptEngine::kNumFilterTypes))
        {
            return;
        }
        const auto type = static_cast<AbacDsp::FilterType>(index);
        for (auto& voice : m_voices)
        {
            voice.setFilterType(type);
        }
    }

    void setBloom(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setBloom(value);
        }
    }

    void setCutoffOuRange(const float semitones) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setCutoffOuRange(semitones);
        }
    }

    void setResonanceRange(const float amount) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setResonanceRange(amount);
        }
    }

    void setBreathOuRange(const float amount) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setBreathOuRange(amount);
        }
    }

    void setPitchOuRange(const float cents) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setPitchOuRange(cents);
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

    // Both stages of both channels share one sweep and one feedback amount, same as
    // ambientpad's own deliberately mono-swept phaser.
    void setPhaser(const float rateHz, const float depth, const float feedback, const float mix) noexcept
    {
        m_phaserDepth = depth;
        m_phaserMix = mix;
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

    /// @brief The register PlayHarmony's home-relative semitones are voiced around - always
    /// the octave at and above MIDI 60, regardless of which pitch class home is.
    void setHarmonyHome(const int pitchClass) noexcept
    {
        m_harmonyHomeNote = kHarmonyHomeOctaveBase + (((pitchClass % 12) + 12) % 12);
    }

    /// @brief How long a glided channel takes to reach a PlayHarmony target note; a
    /// cross-faded channel (see realizeHarmony()) is unaffected.
    void setHarmonyGlideTime(const float seconds) noexcept
    {
        m_transitionGlideSeconds = std::clamp(seconds, 0.f, 60.f);
    }

    /// @brief The last channel is a dedicated bass pedal, excluded from PlayHarmony's own
    /// voice-leading. 0 is off; 1..127 is a literal MIDI note (fresh trigger from off,
    /// instant repitch otherwise).
    void setPedalNote(const float value) noexcept
    {
        const auto note = static_cast<int>(std::lround(value));
        auto& voice = m_voices[kMaxVoices - 1];
        if (note <= 0)
        {
            voice.stopVoice();
            return;
        }
        if (voice.isPlaying())
        {
            voice.setPitch(note, 0.f, 0.f);
        }
        else
        {
            voice.triggerVoice(note, kManualPlayVelocity);
        }
    }

    /// @brief channel is 1-indexed, matching every other per-channel call - diagnostics/testing only.
    [[nodiscard]] bool voiceIsPlaying(const size_t channel) const noexcept
    {
        return m_voices[channel - 1].isPlaying();
    }

    [[nodiscard]] float voicePitchSemitones(const size_t channel) const noexcept
    {
        return m_voices[channel - 1].currentPitchSemitones();
    }

    /// @brief A modulation snapshot for every voice slot, for the voice-monitor UI.
    [[nodiscard]] std::array<AbacDsp::AmbientSynthVoice::ModulationSnapshot, kMaxVoices> getVoiceSnapshots()
        const noexcept
    {
        std::array<AbacDsp::AmbientSynthVoice::ModulationSnapshot, kMaxVoices> snapshots{};
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            snapshots[v] = m_voices[v].snapshot();
        }
        return snapshots;
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

    /// @brief May run on a different thread than processBlock() (e.g. the editor's Apply
    /// button) - m_scriptMutex keeps the two from ever overlapping while this rebuilds
    /// every voice in place.
    bool setScript(const std::string_view source)
    {
        const std::lock_guard<std::mutex> lock(m_scriptMutex);
        const bool ok = m_scriptEngine.loadScript(source);
        if (ok)
        {
            resetVoicesToDefaults();
            resendUiParameters();
            m_scriptEngine.notifyStart();
            applyPendingScriptCommands();
        }
        return ok;
    }

    void setImportResolver(AmbientSynthScriptEngine::ImportResolver resolver)
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
        return std::string(AmbientSynthScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const AmbientSynthScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    /// @brief Real host/keyboard MIDI, forwarded to the script's OnNoteOn/OnNoteOff/etc.
    /// hooks - there is no C++-side note handling of its own, unlike morphexsynth's MPE.
    void processMidi(const uint8_t* msg) override
    {
        const int channel = msg[0] & 0x0F;
        switch (msg[0] & 0xF0)
        {
            case 0x90: // Note On; velocity 0 is a Note Off per MIDI running-status convention
                if (msg[2] == 0)
                {
                    m_scriptEngine.notifyNoteOff(channel, msg[1], 0x40);
                }
                else
                {
                    m_scriptEngine.notifyNoteOn(channel, msg[1], msg[2]);
                }
                break;
            case 0x80:
                m_scriptEngine.notifyNoteOff(channel, msg[1], msg[2]);
                break;
            case 0xB0:
                m_scriptEngine.notifyCC(channel, msg[1], msg[2]);
                break;
            case 0xC0:
                m_scriptEngine.notifyProgramChange(channel, msg[1]);
                break;
            case 0xA0:
                m_scriptEngine.notifyPolyPressure(channel, msg[1], msg[2]);
                break;
            case 0xD0:
                m_scriptEngine.notifyAftertouch(channel, msg[1]);
                break;
            case 0xE0:
            {
                const int raw14Bit = (static_cast<int>(msg[2]) << 7) | msg[1];
                m_scriptEngine.notifyPitchBend(channel, raw14Bit - 8192);
                break;
            }
            default:
                break;
        }
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        const std::lock_guard<std::mutex> lock(m_scriptMutex);
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();
        applyPendingScriptCommands();

        std::array<float, BlockSize> mono{};
        std::array<float, BlockSize> voiceMono{};
        for (auto& voice : m_voices)
        {
            if (!voice.isPlaying())
            {
                continue;
            }
            voice.processBlock(voiceMono.data(), BlockSize);
            for (size_t s = 0; s < BlockSize; ++s)
            {
                mono[s] += voiceMono[s];
            }
        }

        std::array<float, BlockSize> left{mono};
        std::array<float, BlockSize> right{mono};

        m_dcBlocker[0].processBlock(left.data(), BlockSize);
        m_dcBlocker[1].processBlock(right.data(), BlockSize);

        processPhaser(left, right);
        processChorus(left, right);
        processReverb(left, right);

        for (size_t s = 0; s < BlockSize; ++s)
        {
            const auto level = m_level.getValue();
            out(s, 0) = in(s, 0) + left[s] * level;
            out(s, 1) = in(s, 1) + right[s] * level;
        }
    }

  private:
    static constexpr float kDefaultLevelDb{-30.f};
    static constexpr int kManualPlayVelocity{100};

    static constexpr int kHarmonyHomeOctaveBase{60};   ///< C4; home always sits in this octave
    static constexpr int kGlideRepitchMaxSemitones{2}; ///< beyond this, cross-fade, don't glide
    static constexpr float kDefaultGlideSeconds{10.f}; ///< human-perceptible, not instant

    static constexpr size_t kPhaserStagesPerChannel{2};
    static constexpr size_t kPhaserTotalStages{kPhaserStagesPerChannel * 2};
    static constexpr float kPhaserMinHz{200.f};
    static constexpr float kPhaserMaxHz{2000.f};
    static constexpr float kDefaultPhaserRateHz{0.3f};
    static constexpr float kDefaultPhaserFeedback{0.f};

    static constexpr size_t kChorusMaxDelaySamples{4096};
    static constexpr float kChorusBaseDelayMs{18.f};
    static constexpr float kChorusMaxModDepth{0.6f};
    static constexpr float kDefaultChorusRateHz{0.6f};
    static constexpr float kDefaultChorusDepth{0.5f};

    static constexpr size_t kFdnOrder{32};
    static constexpr size_t kFdnMaxSizePerElement{100000};
    static constexpr float kFdnSizeSpread{2.3f};
    static constexpr float kFxSmoothingSeconds{0.01f};
    static constexpr float kDefaultReverbSizeMeters{12.f};
    static constexpr float kDefaultReverbDecayMs{1500.f};
    static constexpr float kDefaultReverbDryDb{0.f};
    static constexpr float kDefaultReverbMixDb{-100.f};

    using Fdn = AbacDsp::FdnTankGlide<kFdnMaxSizePerElement, kFdnOrder, BlockSize>;

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
        std::array<float, BlockSize> monoIn{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            monoIn[s] = 0.5f * (left[s] + right[s]);
        }
        std::array<float, BlockSize> wetLeft{};
        std::array<float, BlockSize> wetRight{};
        m_reverb.processBlockSplit(monoIn.data(), wetLeft.data(), wetRight.data());
        for (size_t s = 0; s < BlockSize; ++s)
        {
            const float dry = m_reverbDry.getValue();
            const float wet = m_reverbMix.getValue();
            left[s] = left[s] * dry + wetLeft[s] * wet;
            right[s] = right[s] * dry + wetRight[s] * wet;
        }
    }

    // Destroys and reconstructs every voice in place, guaranteeing every patch parameter
    // lands back at AmbientSynthVoice's own as-constructed defaults, by construction.
    void resetVoicesToDefaults()
    {
        for (auto& voice : m_voices)
        {
            std::destroy_at(&voice);
            std::construct_at(&voice, sampleRate(), m_waveShaperTables);
        }
    }

    void resendUiParameters() noexcept
    {
        for (size_t i = 0; i < AmbientSynthScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < AmbientSynthScriptEngine::kMaxLuaParams; ++i)
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

    /// @brief Matches currently-playing non-pedal channels against target by pitch proximity:
    /// a close pair glides via setPitch(), a pitch with no close partner cross-fades (release
    /// the old channel, trigger a free one) - same voice-leading ambientpad's own realizer used.
    void realizeHarmony(const std::span<const float> semitones) noexcept
    {
        const auto target = AbacDsp::Voicing::fromFractionalSemitones(semitones);

        struct PlayingChannel
        {
            size_t channelIndex{0};
            int pitch{0};
        };
        std::array<PlayingChannel, kMaxVoices> playing{};
        size_t playingCount = 0;
        std::array<size_t, kMaxVoices> freeChannels{};
        size_t freeCount = 0;
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            if (v == kMaxVoices - 1)
            {
                continue;
            }
            if (m_voices[v].isPlaying())
            {
                playing[playingCount++] = {v, static_cast<int>(std::lround(m_voices[v].currentPitchSemitones()))};
            }
            else
            {
                freeChannels[freeCount++] = v;
            }
        }

        const auto targetNotes = target.notes();
        const auto targetCents = target.centsValues();
        std::array<int, AbacDsp::Voicing::kMaxNotes> absoluteTargets{};
        for (size_t t = 0; t < targetNotes.size(); ++t)
        {
            absoluteTargets[t] = m_harmonyHomeNote + targetNotes[t];
        }
        std::array<bool, AbacDsp::Voicing::kMaxNotes> targetMatched{};
        std::array<bool, kMaxVoices> playingMatched{};
        const auto pairCount = std::min(playingCount, targetNotes.size());
        for (size_t pair = 0; pair < pairCount; ++pair)
        {
            int bestDistance = -1;
            size_t bestPlaying = 0;
            size_t bestTarget = 0;
            for (size_t p = 0; p < playingCount; ++p)
            {
                if (playingMatched[p])
                {
                    continue;
                }
                for (size_t t = 0; t < targetNotes.size(); ++t)
                {
                    if (targetMatched[t])
                    {
                        continue;
                    }
                    const int diff = playing[p].pitch - absoluteTargets[t];
                    const int distance = diff < 0 ? -diff : diff;
                    if (bestDistance < 0 || distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestPlaying = p;
                        bestTarget = t;
                    }
                }
            }
            playingMatched[bestPlaying] = true;
            const auto channelIndex = playing[bestPlaying].channelIndex;
            if (bestDistance <= kGlideRepitchMaxSemitones)
            {
                targetMatched[bestTarget] = true;
                m_voices[channelIndex].setPitch(absoluteTargets[bestTarget], targetCents[bestTarget],
                                                m_transitionGlideSeconds);
            }
            else
            {
                m_voices[channelIndex].stopVoice();
            }
        }

        for (size_t p = 0; p < playingCount; ++p)
        {
            if (!playingMatched[p])
            {
                m_voices[playing[p].channelIndex].stopVoice();
            }
        }

        size_t nextFree = 0;
        for (size_t t = 0; t < targetNotes.size(); ++t)
        {
            if (targetMatched[t] || nextFree >= freeCount)
            {
                continue;
            }
            m_voices[freeChannels[nextFree]].triggerVoice(absoluteTargets[t], kManualPlayVelocity);
            m_voices[freeChannels[nextFree]].setPitch(absoluteTargets[t], targetCents[t], 0.f);
            ++nextFree;
        }
    }

    void applyPendingScriptCommands() noexcept
    {
        const auto noteEvents = m_scriptEngine.drainNoteEvents();
        for (size_t i = 0; i < noteEvents.count; ++i)
        {
            const auto& event = noteEvents.events[i];
            auto& voice = m_voices[event.channel - 1];
            if (event.isOn)
            {
                voice.triggerVoice(event.note, event.velocity);
            }
            else
            {
                voice.stopVoice();
            }
        }

        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            for (size_t i = 0; i < AbacDsp::AmbientSynthVoice::kNumOscillators; ++i)
            {
                if (const auto osc = m_scriptEngine.drainOscillatorCommand(v, i))
                {
                    m_voices[v].setOscillator(i, static_cast<AbacDsp::MaterialPath>(osc->waveform), osc->level,
                                              osc->height, osc->cents);
                }
            }
            if (const auto gain = m_scriptEngine.drainGainCommand(v))
            {
                m_voices[v].setGain(*gain);
            }
            if (const auto pitch = m_scriptEngine.drainPitchCommand(v))
            {
                m_voices[v].setPitch(pitch->note, pitch->cents, pitch->glideTimeSeconds);
            }
            if (const auto cutoffLfo = m_scriptEngine.drainCutoffLfoCommand(v))
            {
                m_voices[v].setCutoffLfo(cutoffLfo->rateCyclesPerMinute, cutoffLfo->depth, cutoffLfo->phaseDegrees);
            }
            if (const auto materialLfo = m_scriptEngine.drainMaterialLfoCommand(v))
            {
                m_voices[v].setMaterialLfo(materialLfo->rateCyclesPerMinute, materialLfo->depth,
                                           materialLfo->phaseDegrees);
            }
            if (const auto resonanceLfo = m_scriptEngine.drainResonanceLfoCommand(v))
            {
                m_voices[v].setResonanceLfo(resonanceLfo->rateCyclesPerMinute, resonanceLfo->depth,
                                            resonanceLfo->phaseDegrees);
            }
            if (const auto pitchLfo = m_scriptEngine.drainPitchLfoCommand(v))
            {
                m_voices[v].setPitchLfo(pitchLfo->rateCyclesPerMinute, pitchLfo->depth, pitchLfo->phaseDegrees);
            }
            if (const auto breathLfo = m_scriptEngine.drainBreathLfoCommand(v))
            {
                m_voices[v].setBreathLfo(breathLfo->rateCyclesPerMinute, breathLfo->depth, breathLfo->phaseDegrees);
            }
            if (const auto driftLfo = m_scriptEngine.drainDriftLfoCommand(v))
            {
                m_voices[v].setDriftLfo(driftLfo->rateCyclesPerMinute, driftLfo->depth, driftLfo->phaseDegrees);
            }
        }

        if (const auto material = m_scriptEngine.drainMaterialCommand())
        {
            setMaterial(*material);
        }
        if (const auto materialRange = m_scriptEngine.drainMaterialRangeCommand())
        {
            setMaterialRange(*materialRange);
        }
        if (const auto cutoff = m_scriptEngine.drainCutoffCommand())
        {
            setCutoff(*cutoff);
        }
        if (const auto resonance = m_scriptEngine.drainResonanceCommand())
        {
            setResonance(*resonance);
        }
        if (const auto filterType = m_scriptEngine.drainFilterTypeCommand())
        {
            setFilterType(*filterType);
        }
        if (const auto bloom = m_scriptEngine.drainBloomCommand())
        {
            setBloom(*bloom);
        }
        if (const auto cutoffOuRange = m_scriptEngine.drainCutoffOuRangeCommand())
        {
            setCutoffOuRange(*cutoffOuRange);
        }
        if (const auto resonanceRange = m_scriptEngine.drainResonanceRangeCommand())
        {
            setResonanceRange(*resonanceRange);
        }
        if (const auto breathOuRange = m_scriptEngine.drainBreathOuRangeCommand())
        {
            setBreathOuRange(*breathOuRange);
        }
        if (const auto pitchOuRange = m_scriptEngine.drainPitchOuRangeCommand())
        {
            setPitchOuRange(*pitchOuRange);
        }
        if (const auto distortion = m_scriptEngine.drainDistortionCommand())
        {
            for (auto& voice : m_voices)
            {
                voice.setDistortion(*distortion);
            }
        }
        if (const auto phaser = m_scriptEngine.drainPhaserCommand())
        {
            setPhaser(phaser->rateHz, phaser->depth, phaser->feedback, phaser->mix);
        }
        if (const auto chorus = m_scriptEngine.drainChorusCommand())
        {
            setChorus(chorus->rateHz, chorus->depth, chorus->mix);
        }
        if (const auto reverb = m_scriptEngine.drainReverbCommand())
        {
            setReverbSize(reverb->sizeMeters);
            setReverbDecay(reverb->decayMs);
            setReverbDry(reverb->dryDb);
            setReverbMix(reverb->mixDb);
        }
        if (const auto home = m_scriptEngine.drainHarmonyHomeCommand())
        {
            setHarmonyHome(*home);
        }
        if (const auto pedalNote = m_scriptEngine.drainPedalNoteCommand())
        {
            setPedalNote(static_cast<float>(*pedalNote));
        }
        if (const auto glideTime = m_scriptEngine.drainHarmonyGlideTimeCommand())
        {
            setHarmonyGlideTime(*glideTime);
        }
        if (const auto playHarmony = m_scriptEngine.drainPlayHarmonyCommand())
        {
            realizeHarmony(std::span<const float>(playHarmony->semitones.data(), playHarmony->count));
        }
    }

    AbacDsp::WaveShaperTableStore m_waveShaperTables{};
    std::array<AbacDsp::AmbientSynthVoice, kMaxVoices> m_voices;
    std::array<AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>, 2> m_dcBlocker;

    std::array<AbacDsp::Phaser24Smooth, kPhaserTotalStages> m_phaser;
    AbacDsp::LfoGenerators m_phaserLfo;
    float m_phaserDepth{0.5f};
    float m_phaserMix{0.f};

    std::array<AbacDsp::ModulationDelayNoFeedback<kChorusMaxDelaySamples>, 2> m_chorusDelay;
    float m_chorusMix{0.f};

    Fdn m_reverb;
    AbacDsp::LinearSmoothing m_reverbDry{1.f};
    AbacDsp::LinearSmoothing m_reverbMix{0.f};

    AbacDsp::LinearSmoothing m_level{1.f};

    int m_harmonyHomeNote{kHarmonyHomeOctaveBase};
    float m_transitionGlideSeconds{kDefaultGlideSeconds};
    std::mutex m_scriptMutex;

    AmbientSynthScriptEngine m_scriptEngine{};
    std::array<float, AmbientSynthScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, AmbientSynthScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{};
};
