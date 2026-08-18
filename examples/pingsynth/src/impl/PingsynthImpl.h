#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/ResoGenerator.h"
#include "Helpers/ConstructArray.h"
#include "PingsynthScriptEngine.h"
#include "Reverbs/FdnTankSpiced.h"

template <size_t BlockSize>
class PingsynthImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxVoices{16};
    using Voice = AbacDsp::ResoGenerator<BlockSize, PingsynthScriptEngine::kMaxHarmonicsPerVoice>;

    explicit PingsynthImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_voices(AbacDsp::constructArray<Voice, kMaxVoices>(sampleRate))
        , m_reverb(sampleRate)
    {
        m_scriptEngine.setSampleRate(sampleRate);
        m_reverb.setDecay(3000);
        m_reverb.setUniqueDelay(true);
        m_reverb.setMinSize(3.5f);
        m_reverb.setMaxSize(35.f);
        m_reverb.setSpreadBulge(-0.35f);
    }

    // Mode dial: polyphonic (index 0, shared voice pool with oldest-steals-newest
    // stealing) vs. MPE (index 1, one voice per MIDI channel; a new note-on on a channel
    // that already has an active voice retriggers it instead of stealing another one).
    void setMode(const size_t value)
    {
        const bool mpeMode = value != 0;
        if (mpeMode != m_mpeMode)
        {
            m_mpeMode = mpeMode;
            m_scriptEngine.notifyMpeMode(m_mpeMode);
        }
    }

    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }

    void setReverbLevel(const float value)
    {
        m_reverbLevel = 0.1f * std::pow(10.f, value / 20.f);
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

    void setImportResolver(PingsynthScriptEngine::ImportResolver resolver)
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
        return std::string(PingsynthScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const PingsynthScriptEngine::UiParamSlots& uiParamSlots() const noexcept
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

    // Channel Voice messages only (status 0x80-0xEF); voice allocation/release is C++
    // plumbing (see allocateVoice()/releaseVoice()), the harmonic content that eventually
    // rings a voice is entirely up to whatever the script's OnNoteOn calls SetHarmonics
    // with, drained in processBlock().
    void processMidi(const uint8_t* msg) override
    {
        const int channel = msg[0] & 0x0F;
        switch (msg[0] & 0xF0)
        {
            case 0x90: // Note On; velocity 0 is a Note Off per MIDI running-status convention
                if (msg[2] == 0)
                {
                    releaseVoice(channel, msg[1]);
                    m_scriptEngine.notifyNoteOff(channel, msg[1], msg[2]);
                }
                else
                {
                    m_scriptEngine.notifyNoteOn(channel, msg[1], msg[2]);
                }
                break;
            case 0x80: // Note Off
                releaseVoice(channel, msg[1]);
                m_scriptEngine.notifyNoteOff(channel, msg[1], msg[2]);
                break;
            case 0xB0: // Control Change
                m_scriptEngine.notifyCC(channel, msg[1], msg[2]);
                break;
            case 0xC0: // Program Change
                m_scriptEngine.notifyProgramChange(channel, msg[1]);
                break;
            case 0xD0: // Channel Pressure (Aftertouch)
                m_scriptEngine.notifyAftertouch(channel, msg[1]);
                break;
            case 0xA0: // Polyphonic Key Pressure (Poly Pressure)
                m_scriptEngine.notifyPolyPressure(channel, msg[1], msg[2]);
                break;
            case 0xE0: // Pitch Bend: wire format is 14-bit 0..16383 (center 8192); re-centered
                       // to -8192..8191 (center 0) before the script sees it.
            {
                const int bendValue = ((static_cast<int>(msg[2]) << 7) | msg[1]) - 8192;
                bendVoicesOnChannel(channel, bendValue);
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
        applyPendingSetHarmonicsRequests();

        std::array<float, BlockSize> dry{};
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            if (!m_voices[v].isActive())
            {
                continue;
            }
            ++m_voiceState[v].age;
            std::array<float, BlockSize> tmp{};
            m_voices[v].processBlock(tmp);
            for (size_t s = 0; s < BlockSize; ++s)
            {
                dry[s] += tmp[s];
            }
        }

        std::array<std::array<float, BlockSize>, 2> wet{};
        m_reverb.processBlockSplit(dry.data(), wet[0].data(), wet[1].data());

        for (size_t s = 0; s < BlockSize; ++s)
        {
            out(s, 0) = in(s, 0) + dry[s] * m_vol + wet[0][s] * m_reverbLevel;
            out(s, 1) = in(s, 1) + dry[s] * m_vol + wet[1][s] * m_reverbLevel;
        }
    }

  private:
    struct VoiceState
    {
        int channel{-1};
        int note{-1};
        size_t age{0};
    };

    // Same exact-equality reasoning as DroneScriptEngine's notifyTimingIfChanged(): a
    // stored float either stays bit-identical or is genuinely a new host/UI value.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < PingsynthScriptEngine::kMaxLuaParams; ++i)
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
        for (size_t i = 0; i < PingsynthScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

    // Assigns (or re-assigns) the voice backing one SetHarmonics() request. MPE mode maps
    // a channel straight to its voice (channel is always 0..15, matching kMaxVoices);
    // polyphonic mode reuses any inactive voice first, then steals the oldest active one.
    [[nodiscard]] size_t allocateVoice(const int channel, const int note) noexcept
    {
        if (m_mpeMode)
        {
            const auto index = static_cast<size_t>(channel);
            m_voiceState[index] = VoiceState{channel, note, 0};
            return index;
        }

        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (!m_voices[i].isActive())
            {
                m_voiceState[i] = VoiceState{channel, note, 0};
                return i;
            }
        }

        size_t oldestIdx = 0;
        size_t maxAge = 0;
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (m_voiceState[i].age > maxAge)
            {
                maxAge = m_voiceState[i].age;
                oldestIdx = i;
            }
        }
        m_voiceState[oldestIdx] = VoiceState{channel, note, 0};
        return oldestIdx;
    }

    // Frees the VoiceState tracking a note occupied, so it becomes reusable by
    // allocateVoice(); the voice itself keeps ringing out naturally (no forced damping on
    // note-off), matching a physically-modeled resonator's behavior.
    void releaseVoice(const int channel, const int note) noexcept
    {
        if (m_mpeMode)
        {
            m_voiceState[static_cast<size_t>(channel)].channel = -1;
            return;
        }
        for (auto& state : m_voiceState)
        {
            if (state.channel == channel && state.note == note)
            {
                state.channel = -1;
                state.note = -1;
            }
        }
    }

    // Bends every voice currently assigned to channel (MPE: at most one; polyphonic: every
    // active voice a note-on claimed on that channel) by the script's pitch-bend range.
    // ResoGenerator::pitchBendCents() only bends the harmonic at index 0 of whatever list
    // runHarmonicList() was last given - a pre-existing library limitation, not new here.
    void bendVoicesOnChannel(const int channel, const int bendValue) noexcept
    {
        const float normalized = static_cast<float>(bendValue) / 8192.f;
        const float cents = normalized * m_scriptEngine.pitchBendRangeSemitones() * 100.f;
        for (size_t i = 0; i < kMaxVoices; ++i)
        {
            if (m_voiceState[i].channel == channel)
            {
                m_voices[i].pitchBendCents(0, 0, cents);
            }
        }
    }

    // ResoGenerator::runHarmonicList() consumes Harmonic::delay as whole audio blocks
    // (decremented once per processBlock() call), not milliseconds.
    [[nodiscard]] int delayMsToBlocks(const float delayMs) const noexcept
    {
        const float blocksPerMs = sampleRate() * 0.001f / static_cast<float>(BlockSize);
        return static_cast<int>(std::max(0.f, delayMs) * blocksPerMs + 0.5f);
    }

    void applyPendingSetHarmonicsRequests()
    {
        const auto pending = m_scriptEngine.drainSetHarmonicsRequests();
        for (size_t i = 0; i < pending.count; ++i)
        {
            const auto& request = pending.requests[i];
            const size_t voiceIndex = allocateVoice(request.channel, request.note);
            auto& voice = m_voices[voiceIndex];

            std::array<AbacDsp::Harmonic, PingsynthScriptEngine::kMaxHarmonicsPerVoice> harmonics{};
            for (size_t h = 0; h < request.harmonicCount; ++h)
            {
                const auto& source = request.harmonics[h];
                harmonics[h] =
                    AbacDsp::Harmonic{source.freq, source.gain, source.decay, delayMsToBlocks(source.delayMs)};
            }
            voice.setAttack(request.attackMs);
            voice.setSoftExcitation(request.softExcitation);
            voice.runHarmonicList(harmonics.data(), request.harmonicCount);
        }
    }

    PingsynthScriptEngine m_scriptEngine;
    std::array<Voice, kMaxVoices> m_voices;
    std::array<VoiceState, kMaxVoices> m_voiceState{};
    AbacDsp::FdnTankSpiced<48000, 32, BlockSize> m_reverb;
    float m_vol{0.f};
    float m_reverbLevel{0.f};
    bool m_mpeMode{true};

    std::array<float, PingsynthScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, PingsynthScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{-1.f, -1.f, -1.f, -1.f,
                                                                                         -1.f, -1.f, -1.f, -1.f};
};
