#pragma once

#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <string_view>
#include <utility>

#include "Audio/AudioBuffer.h"
#include "DroneScriptEngine.h"
#include "DroneSequencer.h"
#include "EffectBase.h"
#include "Filters/Biquad.h"
#include "Generators/KarplusStrongEnsemble.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/Convert.h"
#include "Parameters/SmoothingParameter.h"
#include "Reverbs/FdnTankGlide.h"

template <size_t BlockSize>
class DroneSequencerImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxVoices{16};

    explicit DroneSequencerImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_ensemble(sampleRate)
        , m_sequencer(sampleRate)
        , m_fdn(sampleRate)
        , m_lfoWander(AbacDsp::constructArray<AbacDsp::OrnsteinUhlenbeckProcess, kMaxVoices>(
              sampleRate / static_cast<float>(BlockSize)))
        , m_sustainWander(AbacDsp::constructArray<AbacDsp::OrnsteinUhlenbeckProcess, kMaxVoices>(
              sampleRate / static_cast<float>(BlockSize)))
    {
        m_scriptEngine.setSampleRate(sampleRate);
        m_fdn.setModulation(kReverbModulationDepth, kReverbModulationSpeedHz);
        std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist{-1.f, 1.f};
        for (auto& offset : m_voiceLfoStaticOffset)
        {
            offset = dist(rng);
        }
    }

    void setLevel(const float value)
    {
        m_level.newTransition(Convert::dbToGain(value), kParamSmoothingSeconds, sampleRate());
    }

    void setTuning(const float value)
    {
        m_sequencer.setTuning(value);
    }

    void setTranspose(const float semitones)
    {
        m_sequencer.setTranspose(semitones);
    }

    void setDetune(const float value)
    {
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            m_sequencer.setDetuneCents(i, kDetuneFactors[i] * value);
        }
    }

    void setReverbDry(const float valueDb)
    {
        m_reverbDryGain.newTransition(Convert::dbToGain(valueDb), kParamSmoothingSeconds, sampleRate());
    }

    void setReverbWet(const float valueDb)
    {
        m_reverbWetGain.newTransition(Convert::dbToGain(valueDb), kParamSmoothingSeconds, sampleRate());
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

    void setDivision(const int index) noexcept
    {
        m_divisionIndex = clampDivisionIndex(index);
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

    void setImportResolver(DroneScriptEngine::ImportResolver resolver)
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
        return std::string(DroneScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const DroneScriptEngine::UiParamSlots& uiParamSlots() const noexcept
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

    // Channel Voice messages only (status 0x80-0xEF): their length is fully determined
    // by the status byte's high nibble, so no separate length parameter is needed here -
    // matches EffectBase::processMidi()'s existing (byte-pointer-only) signature.
    void processMidi(const uint8_t* msg) override
    {
        const uint8_t channel = msg[0] & 0x0Fu;
        switch (msg[0] & 0xF0u)
        {
            case 0x90u: // Note On; velocity 0 is a Note Off per MIDI running-status convention
                if (msg[2] == 0)
                {
                    m_scriptEngine.notifyNoteOff(channel, msg[1], msg[2]);
                }
                else
                {
                    m_scriptEngine.notifyNoteOn(channel, msg[1], msg[2]);
                }
                break;
            case 0x80u: // Note Off
                m_scriptEngine.notifyNoteOff(channel, msg[1], msg[2]);
                break;
            case 0xB0u: // Control Change
                m_scriptEngine.notifyCC(channel, msg[1], msg[2]);
                break;
            case 0xC0u: // Program Change
                m_scriptEngine.notifyProgramChange(channel, msg[1]);
                break;
            case 0xD0u: // Channel Pressure (Aftertouch)
                m_scriptEngine.notifyAftertouch(channel, msg[1]);
                break;
            case 0xA0u: // Polyphonic Key Pressure (Poly Pressure)
                m_scriptEngine.notifyPolyPressure(channel, msg[1], msg[2]);
                break;
            case 0xE0u: // Pitch Bend: wire format is 14-bit 0..16383 (center 8192); re-centered
                        // to -8192..8191 (center 0) before the script sees it.
                m_scriptEngine.notifyPitchBend(channel, ((static_cast<int>(msg[2]) << 7) | msg[1]) - 8192);
                break;
            default:
                break;
        }
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

    [[nodiscard]] float voiceSustainFeed(const size_t index) const noexcept
    {
        return m_lastVoiceSustainFeed[index];
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

    void setDecayOctave(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setDecayOctaveFactor(value); });
    }

    void setDamper(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setDamper(value); });
    }

    void setLevelSustain(const float value) noexcept
    {
        m_levelSustainBase = value;
    }

    void setSustainHumanize(const float percent) noexcept
    {
        m_sustainHumanizePercent = std::clamp(percent, 0.f, 100.f);
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
        // value is already normalized (1.0 = self-oscillation threshold); see
        // Filter1Pole4StageSmooth::setResonance() in PoleMixingFilter.h.
        forEachVoice([value](auto& voice) { voice.setFilterResonance(value); });
    }

    void setContourFilter(const float value)
    {
        forEachVoice([value](auto& voice) { voice.setKeyTracking(value); });
    }

    void processBlock([[maybe_unused]] const AbacDsp::AudioBuffer<2, BlockSize>& in,
                      AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        updateTiming();
        m_scriptEngine.tickBlock(BlockSize);
        forwardExcitations();
        std::array<float, BlockSize> dry{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_sequencer.step(m_ensemble, m_scriptEngine);
            dry[i] = m_ensemble.step();
        }

        std::array<float, BlockSize> levelBlock{};
        std::array<float, BlockSize> dryGainBlock{};
        std::array<float, BlockSize> wetGainBlock{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            levelBlock[i] = m_level.getValue();
            dryGainBlock[i] = m_reverbDryGain.getValue();
            wetGainBlock[i] = m_reverbWetGain.getValue();
        }

        std::array<std::array<float, BlockSize>, 2> wet{};
        m_fdn.processBlockSplit(dry.data(), wet[0].data(), wet[1].data());
        for (size_t c = 0; c < 2; ++c)
        {
            m_reverbShelfLow[c].processBlock(wet[c].data(), wet[c].data(), BlockSize);
            m_reverbShelfHigh[c].processBlock(wet[c].data(), wet[c].data(), BlockSize);

            for (size_t i = 0; i < BlockSize; ++i)
            {
                out(i, c) = (dry[i] * dryGainBlock[i] + wet[c][i] * wetGainBlock[i]) * levelBlock[i];
            }
        }
    }

  private:
    static constexpr size_t kMaxStringLength{10000};
    // Per-voice detune spread multipliers, indexed by voice/channel; the Detune dial
    // scales all of them. Extends tanpura's 5-voice list with a plausible continuation
    // for the extra voices - a starting point, not a tuned final value.
    static constexpr std::array<float, kMaxVoices> kDetuneFactors{0.f, -1.f, 1.3f, -1.7f, 2.f, -2.3f, 2.6f, -2.9f};
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
    static constexpr float kMaxSustainWanderSigma{0.3f};      // per-voice sustain wander bound at 100% humanize

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
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            fn(m_ensemble.voice(i));
        }
    }

    // Excite() can be called from any script context, not just NextNotes()'s lookahead
    // cycle, so its own pending queue is drained here once per block rather than folded
    // into the sequencer's note-scheduling pipeline.
    void forwardExcitations() noexcept
    {
        const auto pending = m_scriptEngine.drainExcitations();
        for (size_t i = 0; i < pending.count; ++i)
        {
            const auto& excitation = pending.excitations[i];
            const auto voiceIndex = std::min(excitation.channel, kMaxVoices - 1);
            m_ensemble.voice(voiceIndex).scheduleExcitation(excitation.event);
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

    // OnTiming() is user Lua code, so it only runs when bpm/division actually change,
    // not every block - otherwise a host bpm-automation ramp would run it constantly.
    // Exact equality is intentional here: currentBpm() either returns the same stored
    // float untouched or a genuinely new one, so bit-identity is exactly "unchanged".
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

    // Same exact-equality reasoning as notifyTimingIfChanged(). Notifying a claimed and
    // an unclaimed slot are equally cheap - DroneScriptEngine no-ops for an unclaimed one.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < DroneScriptEngine::kMaxLuaParams; ++i)
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
        for (size_t i = 0; i < DroneScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

    // OnStart()/OnStop() fire on any effective start/stop transition - the manual Play
    // switch toggling in manual mode, or the host transport's play state when Host Sync
    // is on - so a script can reset its own counters/state. JUCE's transport only
    // exposes a play/not-playing bool (no distinct pause), and the sequencer itself
    // always resets its clock on any stop->start transition rather than resuming, so
    // there is no separate "paused" state to report here either.
    void notifyTransportIfChanged() noexcept
    {
        const bool playing = effectivePlaying();
        if (playing == m_lastNotifiedPlaying)
        {
            return;
        }
        m_lastNotifiedPlaying = playing;
        if (playing)
        {
            m_scriptEngine.notifyStart();
        }
        else
        {
            m_scriptEngine.notifyStop();
        }
    }

    // Forwards the host's raw playhead (Transport.* in Lua) every block; the engine itself
    // only fires OnTempoChanged/OnTimeSignatureChanged/OnPlayingStart/OnPlayingStop for
    // whichever fields actually changed, so there is no separate "if changed" gate here -
    // unlike notifyTransportIfChanged() above, which is a different concept (the plugin's
    // *effective* play state, not the host's own).
    void updateScriptTransportSnapshot() noexcept
    {
        const auto& transport = hostTransport();
        m_scriptEngine.notifyTransportSnapshot(transport.bpm, transport.ppqPosition, transport.timeInSeconds,
                                               static_cast<int>(transport.beatsPerBar), transport.timeSigDenominator,
                                               transport.isPlaying, transport.isLooping, transport.isRecording);
    }

    void updateTiming() noexcept
    {
        const float bpm = currentBpm();
        m_sequencer.setIntervalMs(intervalMsForDivision(bpm, static_cast<int>(m_divisionIndex)));
        m_sequencer.setPlaying(effectivePlaying());
        notifyTransportIfChanged();
        notifyTimingIfChanged(bpm);
        notifyUiParametersIfChanged();
        updateScriptTransportSnapshot();
        updateLfoSpeeds();
        updateSustainFeed();
    }

    // At 0% variation, every voice's wander sigma/mu settle to 0 (mu = sigma in
    // OrnsteinUhlenbeckProcess), so this reduces exactly to the old shared-speed behavior.
    void updateLfoSpeeds() noexcept
    {
        const auto amount = m_lfoSpeedVariationPercent * 0.01f;
        const auto sigma = amount * kMaxLfoWanderSigma;
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            m_lfoWander[i].setSigma(sigma);
            const auto wander = m_lfoWander[i].step() - sigma;
            const auto speed =
                m_lfoSpeed * (1.f + m_voiceLfoStaticOffset[i] * kMaxVoiceLfoSpreadFraction * amount) * (1.f + wander);
            m_lastVoiceLfoSpeed[i] = std::max(0.01f, speed);
            m_ensemble.voice(i).setFilterLfoSpeed(m_lastVoiceLfoSpeed[i]);
        }
    }

    // Gates the string's constant excitation feed off when not playing, so a voice already
    // in its sustain phase falls back to normal per-period decay instead of ringing forever.
    void updateSustainFeed() noexcept
    {
        const auto gate = effectivePlaying() ? 1.f : 0.f;
        const auto amount = m_sustainHumanizePercent * 0.01f;
        const auto sigma = amount * kMaxSustainWanderSigma;
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            m_sustainWander[i].setSigma(sigma);
            const auto wander = m_sustainWander[i].step() - sigma;
            m_lastVoiceSustainFeed[i] = std::clamp(m_levelSustainBase * (1.f + wander), 0.f, 1.f) * gate;
            m_ensemble.voice(i).setConstFeed(m_lastVoiceSustainFeed[i]);
        }
    }

    AbacDsp::KarplusStrongEnsemble<kMaxVoices, kMaxStringLength> m_ensemble;
    DroneSequencer<kMaxStringLength, kMaxVoices> m_sequencer;
    DroneScriptEngine m_scriptEngine;
    Fdn m_fdn;
    std::array<LoShelfFilter, 2> m_reverbShelfLow{};
    std::array<HiShelfFilter, 2> m_reverbShelfHigh{};
    std::array<AbacDsp::OrnsteinUhlenbeckProcess, kMaxVoices> m_lfoWander;
    std::array<AbacDsp::OrnsteinUhlenbeckProcess, kMaxVoices> m_sustainWander;
    std::array<float, kMaxVoices> m_voiceLfoStaticOffset{};
    std::array<float, kMaxVoices> m_lastVoiceLfoSpeed{};
    std::array<float, kMaxVoices> m_lastVoiceSustainFeed{};

    static constexpr float kParamSmoothingSeconds{0.01f};
    AbacDsp::LinearSmoothing m_level{1.f};
    AbacDsp::LinearSmoothing m_reverbDryGain{1.f};
    AbacDsp::LinearSmoothing m_reverbWetGain{0.f};
    float m_manualBpm{120.f};
    bool m_hostSync{false};
    bool m_manualPlaying{false};
    size_t m_divisionIndex{4};
    float m_lastNotifiedBpm{-1.f};
    size_t m_lastNotifiedDivisionIndex{static_cast<size_t>(-1)};
    bool m_lastNotifiedPlaying{false};
    float m_lfoSpeed{0.5f};
    float m_lfoSpeedVariationPercent{0.f};
    float m_levelSustainBase{0.2f};
    float m_sustainHumanizePercent{0.f};
    float m_attackFilterMsecs{10.f};
    float m_decayFilterMsecs{10.f};
    float m_levelSustainFilter{0.f};

    std::array<float, DroneScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, DroneScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{-1.f, -1.f, -1.f, -1.f,
                                                                                     -1.f, -1.f, -1.f, -1.f};
};
