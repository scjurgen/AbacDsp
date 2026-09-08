#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "AmbientPadScriptEngine.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/OnePoleFilter.h"
#include "Filters/PoleMixingFilter.h"
#include "Generators/SynthLfo.h"
#include "Harmony/HarmonicOrganism.h"
#include "Helpers/ConstructArray.h"
#include "NonLinear/WaveShaperTables.h"
#include "Numbers/Convert.h"
#include "Parameters/SmoothingParameter.h"
#include "Reverbs/FdnTankGlide.h"
#include "Reverbs/ModulationDelayNoFeedback.h"
#include "Synthesizer/AmbientPadVoice.h"

template <size_t BlockSize>
class AmbientPadImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxVoices{16}; ///< must match AmbientPadScriptEngine::kMaxChannels

    explicit AmbientPadImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_voices(AbacDsp::constructArray<AbacDsp::AmbientPadVoice, kMaxVoices>(sampleRate, m_waveShaperTables))
        , m_dcBlocker(
              AbacDsp::constructArray<AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>, 2>(
                  sampleRate, 20.f))
        , m_phaser(AbacDsp::constructArray<AbacDsp::Phaser24Smooth, kPhaserTotalStages>(sampleRate))
        , m_phaserLfo(sampleRate)
        , m_chorusDelay(
              AbacDsp::constructArray<AbacDsp::ModulationDelayNoFeedback<kChorusMaxDelaySamples>, 2>(sampleRate))
        , m_reverb(sampleRate)
        , m_organism(sampleRate)
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
        // the left, same trick morphexsynth uses - the voices themselves stay mono.
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

    /// @brief Repitches channel 1's voice live, whether it is currently playing or not
    /// (a silent voice just picks up the new note whenever it is next triggered).
    void setNote(const float value) noexcept
    {
        m_manualNote = static_cast<int>(std::lround(value));
        m_voices[0].setNote(m_manualNote);
    }

    /// @brief Manual exploration control: gates channel 1's voice directly (no MIDI in this phase).
    void setPlay(const bool value) noexcept
    {
        if (value)
        {
            m_voices[0].triggerVoice(m_manualNote, kManualPlayVelocity);
        }
        else
        {
            m_voices[0].stopVoice();
        }
    }

    void setMaterial(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setMaterial(value);
        }
    }

    void setLight(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setLight(value);
        }
    }

    void setMotion(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setMotion(value);
        }
    }

    void setBreath(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setBreath(value);
        }
    }

    void setStability(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setStability(value);
        }
    }

    void setBloom(const float value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setBloom(value);
        }
    }

    void setHold(const bool value) noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.setHold(value);
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
    // morphexsynth's own deliberately mono-swept phaser.
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

    /// @brief Off by default: with harmony disabled, the organism never steps and every voice
    /// stays exactly as driven by Note/Play/NoteOn/NoteOff, as today.
    void setHarmonyEnabled(const bool enabled) noexcept
    {
        m_harmonyEnabled = enabled;
    }

    /// @brief Retunes the organism's palette and where the realizer anchors it in register
    /// (always the octave at and above MIDI 60, regardless of which pitch class home is).
    void setHarmonyHome(const int pitchClass) noexcept
    {
        m_organism.setHome(pitchClass);
        m_harmonyHomeNote = kHarmonyHomeOctaveBase + (((pitchClass % 12) + 12) % 12);
    }

    /// @brief Soft-biases the organism toward one palette region: 0 = no preference,
    /// 1..5 = Home/MajorLight/ModalWarmth/OpenSuspended/ChromaticWeather in that order.
    void setHarmonyCharacter(const int index) noexcept
    {
        if (index <= 0 || index > 5)
        {
            m_organism.setPreferredRegion(std::nullopt);
            return;
        }
        m_organism.setPreferredRegion(static_cast<AbacDsp::PaletteRegion>(index - 1));
    }

    /// @brief The last channel is a dedicated bass pedal, always excluded from voice-leading
    /// (see realizeHarmonicTransition()). 0 is off; 1..127 is a literal MIDI note - triggered
    /// fresh from off, repitched instantly otherwise (matching the Note dial's own behaviour).
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

    /// @brief Replaces the pedal-channel set; a channel newly added is triggered at the
    /// current home note (a channel removed is left sounding, not stopped).
    void setPedalChannels(const std::span<const int> channels) noexcept
    {
        std::array<bool, kMaxVoices> newPedalChannels{};
        for (const auto channel : channels)
        {
            if (channel >= 1 && channel <= static_cast<int>(kMaxVoices))
            {
                newPedalChannels[static_cast<size_t>(channel - 1)] = true;
            }
        }
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            if (newPedalChannels[v] && !m_pedalChannels[v])
            {
                m_voices[v].triggerVoice(m_harmonyHomeNote, kManualPlayVelocity);
            }
        }
        m_pedalChannels = newPedalChannels;
    }

    void triggerHarmonyImpulse(const AbacDsp::ImpulseKind kind) noexcept
    {
        m_organism.triggerImpulse(kind);
    }

    /// @brief Count of harmonic transitions realized so far - diagnostics/testing only.
    [[nodiscard]] size_t harmonyTransitionCount() const noexcept
    {
        return m_harmonyTransitionCount;
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
            resetVoicesToDefaults();
            resendUiParameters();
            m_scriptEngine.notifyStart();
            applyPendingScriptCommands();
        }
        return ok;
    }

    void setImportResolver(AmbientPadScriptEngine::ImportResolver resolver)
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
        return std::string(AmbientPadScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const AmbientPadScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();
        applyPendingScriptCommands();

        if (m_harmonyEnabled)
        {
            m_organism.step(BlockSize);
            if (const auto transition = m_organism.takePendingTransition())
            {
                realizeHarmonicTransition(*transition);
            }
        }

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

    static constexpr int kHarmonyHomeOctaveBase{60};      ///< C4; home always sits in this octave
    static constexpr int kGlideRepitchMaxSemitones{2};    ///< beyond this, cross-fade, don't glide
    static constexpr float kTransitionGlideSeconds{10.f}; ///< human-perceptible, not instant

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
    // lands back at AmbientPadVoice's own as-constructed defaults, by construction.
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
        for (size_t i = 0; i < AmbientPadScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < AmbientPadScriptEngine::kMaxLuaParams; ++i)
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

    /// @brief Prints the just-realized state's name and its intervals from home, e.g.
    /// "1m9/lo: 0 -24 -2 2 3 7" - only called on a real transition, never periodically.
    void logHarmonicState(const AbacDsp::HarmonicState& state) const
    {
        std::fprintf(stderr, "harmony: %.*s:", static_cast<int>(state.name.size()), state.name.data());
        for (const auto note : state.voicing.notes())
        {
            std::fprintf(stderr, " %d", note);
        }
        std::fprintf(stderr, "\n");
    }

    /// @brief Matches currently-playing non-pedal channels against target's voicing by pitch
    /// proximity: a close pair glides via setPitch(), a pitch with no close partner cross-fades
    /// (release the old channel, trigger a free one) - see the plan's design decision 3.
    void realizeHarmonicTransition(const AbacDsp::HarmonicState& target) noexcept
    {
        logHarmonicState(target);

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
            if (m_pedalChannels[v] || v == kMaxVoices - 1)
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

        const auto targetNotes = target.voicing.notes();
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
                m_voices[channelIndex].setPitch(absoluteTargets[bestTarget], 0.f, kTransitionGlideSeconds);
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
            ++nextFree;
        }

        ++m_harmonyTransitionCount;
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
            for (size_t i = 0; i < AbacDsp::AmbientPadVoice::kNumOscillators; ++i)
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
        }

        if (const auto enabled = m_scriptEngine.drainHarmonyEnabledCommand())
        {
            setHarmonyEnabled(*enabled);
        }
        if (const auto home = m_scriptEngine.drainHarmonyHomeCommand())
        {
            setHarmonyHome(*home);
        }
        if (const auto character = m_scriptEngine.drainHarmonyCharacterCommand())
        {
            setHarmonyCharacter(*character);
        }
        if (const auto pedalNote = m_scriptEngine.drainPedalNoteCommand())
        {
            setPedalNote(*pedalNote);
        }
        if (const auto pedal = m_scriptEngine.drainPedalChannelsCommand())
        {
            setPedalChannels(std::span<const int>(pedal->channels.data(), pedal->count));
        }
        const auto impulses = m_scriptEngine.drainImpulseEvents();
        for (size_t i = 0; i < impulses.count; ++i)
        {
            triggerHarmonyImpulse(impulses.events[i]);
        }

        if (const auto material = m_scriptEngine.drainMaterialCommand())
        {
            setMaterial(*material);
        }
        if (const auto light = m_scriptEngine.drainLightCommand())
        {
            setLight(*light);
        }
        if (const auto motion = m_scriptEngine.drainMotionCommand())
        {
            setMotion(*motion);
        }
        if (const auto breath = m_scriptEngine.drainBreathCommand())
        {
            setBreath(*breath);
        }
        if (const auto stability = m_scriptEngine.drainStabilityCommand())
        {
            setStability(*stability);
        }
        if (const auto bloom = m_scriptEngine.drainBloomCommand())
        {
            setBloom(*bloom);
        }
        if (const auto hold = m_scriptEngine.drainHoldCommand())
        {
            setHold(*hold);
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
    }

    AbacDsp::WaveShaperTableStore m_waveShaperTables{};
    std::array<AbacDsp::AmbientPadVoice, kMaxVoices> m_voices;
    std::array<AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>, 2> m_dcBlocker;

    std::array<AbacDsp::Phaser24Smooth, kPhaserTotalStages> m_phaser;
    AbacDsp::LfoGenerators m_phaserLfo;
    float m_phaserDepth{0.5f};
    float m_phaserMix{0.f};

    std::array<AbacDsp::ModulationDelayNoFeedback<kChorusMaxDelaySamples>, 2> m_chorusDelay;
    float m_chorusMix{0.f};

    Fdn m_reverb;
    AbacDsp::HarmonicOrganism m_organism;
    AbacDsp::LinearSmoothing m_reverbDry{1.f};
    AbacDsp::LinearSmoothing m_reverbMix{0.f};

    AbacDsp::LinearSmoothing m_level{1.f};
    int m_manualNote{69};

    bool m_harmonyEnabled{false};
    int m_harmonyHomeNote{kHarmonyHomeOctaveBase};
    std::array<bool, kMaxVoices> m_pedalChannels{};
    size_t m_harmonyTransitionCount{0};

    AmbientPadScriptEngine m_scriptEngine{};
    std::array<float, AmbientPadScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, AmbientPadScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{};
};
