#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"

// One partial as sent by a script's SetHarmonics() call. delayMs stays in milliseconds
// here - converting to whole audio blocks needs BlockSize, which this engine (unlike
// PingsynthImpl) doesn't know, so that conversion happens where the request is drained.
struct PingHarmonic
{
    float freq{0.f};
    float gain{1.f};
    float decay{0.3f};
    float delayMs{0.f};
};

/**
 * Adds Pingsynth's own scripted entry point on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter machinery: SetHarmonics(channel, note, velocity, params) queues one
 * fresh trigger for a note, carrying an arbitrary Lua-computed harmonic list plus the
 * voice's excitation shape (attackMs, softExcitation). Mirrors DroneScriptEngine's
 * Excite()/drainExcitations() - a bounded pending array, drained once per block, never a
 * persistent per-voice override.
 */
class PingsynthScriptEngine : public LuaScriptEngineBase<PingsynthScriptEngine>
{
  public:
    static constexpr size_t kMaxHarmonicsPerVoice{60};
    static constexpr size_t kMaxSetHarmonicsRequestsPerBlock{16};

    struct SetHarmonicsRequest
    {
        int channel{0};
        int note{0};
        float velocity{0.f};
        float attackMs{0.f};
        float softExcitation{0.f};
        std::array<PingHarmonic, kMaxHarmonicsPerVoice> harmonics{};
        size_t harmonicCount{0};
    };

    struct PendingSetHarmonicsResult
    {
        std::array<SetHarmonicsRequest, kMaxSetHarmonicsRequestsPerBlock> requests{};
        size_t count{0};
    };

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- Pingsynth script - builds an odd-harmonic series in plain Lua and rings the resonator\n"
"-- bank on every note. Edit the loop in OnNoteOn to try a different timbre (even, stretched,\n"
"-- inharmonic, whatever the math expresses).\n"
"NumHarmonics = 12\n"
"Decay = 1.2\n"
"GainFalloff = 0.7\n"
"\n"
"function OnNoteOn(channel, note, velocity)\n"
"    local fundamental = Music.NoteToHz(note)\n"
"    local vel = Vel.Cubic(velocity / 127)\n"
"    local harmonics = {}\n"
"    for i = 1, NumHarmonics do\n"
"        local ratio = 2 * i - 1 -- odd harmonics: 1, 3, 5, 7, ...\n"
"        harmonics[i] = {\n"
"            freq = fundamental * ratio,\n"
"            gain = vel * (GainFalloff ^ (i - 1)),\n"
"            decay = Decay,\n"
"            delayMs = 0,\n"
"        }\n"
"    end\n"
"    SetHarmonics(channel, note, velocity, { harmonics = harmonics, attackMs = 1, softExcitation = 0 })\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kPingsynthSkeletonHooks =
"-- Pingsynth script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"\n"
"-- SetHarmonics(channel, note, velocity, { harmonics = { .. }, attackMs = .., softExcitation = .. })\n"
"-- fires a fresh set of resonators for one note - call it from OnNoteOn below. harmonics is\n"
"-- a plain array of { freq = Hz, gain = 0..1, decay = seconds, delayMs = ms before this\n"
"-- partial enters }; freq is required, everything else optional (gain 1, decay 0.3,\n"
"-- delayMs 0). attackMs/softExcitation shape the whole voice's excitation (fade-in,\n"
"-- noise-burst tail); both default to 0 (a plain impulse, no fade).\n"
"\n"
"-- SetPitchBendRange(semitones) sets how far a full pitch-wheel deflection bends the\n"
"-- ringing voice(s) on that MIDI channel. Defaults to 12 semitones in MPE mode, 2 in\n"
"-- polyphonic mode; call this to override either.\n"
"\n"
"-- Fires whenever the Mode dial's polyphonic/mpe setting changes.\n"
"function OnMpeModeChanged(mpeMode)\n"
"end\n"
"\n";
    // clang-format on

    // Shown by the popup editor's Reset button, not the engine's own default script.
    static const std::string kFullSkeletonScript;

    explicit PingsynthScriptEngine(size_t poolBytes = 512 * 1024);

    // Drains (returns and clears) every SetHarmonics() request queued since the last
    // drain. Called once per block, same spirit as DroneScriptEngine::drainExcitations().
    [[nodiscard]] PendingSetHarmonicsResult drainSetHarmonicsRequests() noexcept;

    // Fires the script's OnMpeModeChanged(mpeMode) hook, if defined, and remembers the
    // mode so pitchBendRangeSemitones() below can pick the right default.
    void notifyMpeMode(bool mpeMode) noexcept;

    // Read every time a MIDI pitch-bend message arrives (not drained): how many semitones
    // a full bend-wheel deflection covers. A script's SetPitchBendRange() always wins;
    // absent that, MPE mode defaults to a full octave (per-note bends are expected to be
    // expressive there) and polyphonic mode to 2 semitones (the conventional MIDI default).
    [[nodiscard]] float pitchBendRangeSemitones() const noexcept
    {
        if (m_pitchBendRangeOverride)
        {
            return *m_pitchBendRangeOverride;
        }
        return m_mpeMode ? kDefaultMpePitchBendRangeSemitones : kDefaultPolyPitchBendRangeSemitones;
    }

  private:
    friend class LuaScriptEngineBase<PingsynthScriptEngine>;
    void bindScriptFunctions();

    // Reads only get_or-defaulted fields (never throws) and drops the request once the
    // pending array is full - stays audio-thread-safe like every other bound setter here.
    void luaSetHarmonics(int channel, int note, float velocity, const sol::table& params);

    static constexpr float kDefaultMpePitchBendRangeSemitones{12.f};
    static constexpr float kDefaultPolyPitchBendRangeSemitones{2.f};

    sol::protected_function m_onMpeModeChangedFn;
    std::array<SetHarmonicsRequest, kMaxSetHarmonicsRequestsPerBlock> m_pendingRequests{};
    size_t m_pendingCount{0};
    std::optional<float> m_pitchBendRangeOverride;
    bool m_mpeMode{true};
};

inline const std::string PingsynthScriptEngine::kFullSkeletonScript =
    std::string(kPingsynthSkeletonHooks) + std::string(kCommonSkeletonScript);

inline PingsynthScriptEngine::PingsynthScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<PingsynthScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void PingsynthScriptEngine::bindScriptFunctions()
{
    m_lua.set_function("SetHarmonics", &PingsynthScriptEngine::luaSetHarmonics, this);
    m_lua.set_function("SetPitchBendRange", [this](const float semitones) { m_pitchBendRangeOverride = semitones; });
    m_onMpeModeChangedFn = m_lua["OnMpeModeChanged"];
}

inline void PingsynthScriptEngine::luaSetHarmonics(const int channel, const int note, const float velocity,
                                                   const sol::table& params)
{
    if (m_pendingCount >= kMaxSetHarmonicsRequestsPerBlock)
    {
        return;
    }

    SetHarmonicsRequest request{};
    request.channel = channel;
    request.note = note;
    request.velocity = velocity;
    request.attackMs = params.get_or("attackMs", 0.f);
    request.softExcitation = params.get_or("softExcitation", 0.f);

    const sol::optional<sol::table> harmonicsOpt = params["harmonics"];
    if (harmonicsOpt)
    {
        const auto& harmonicsTable = *harmonicsOpt;
        const size_t count = std::min(harmonicsTable.size(), kMaxHarmonicsPerVoice);
        for (size_t i = 1; i <= count; ++i)
        {
            const sol::optional<sol::table> entryOpt = harmonicsTable[i];
            if (!entryOpt)
            {
                continue;
            }
            const auto& entry = *entryOpt;
            const float freq = entry.get_or("freq", 0.f);
            if (freq <= 0.f)
            {
                continue;
            }
            request.harmonics[request.harmonicCount] = PingHarmonic{
                freq, entry.get_or("gain", 1.f), entry.get_or("decay", 0.3f), entry.get_or("delayMs", 0.f)};
            ++request.harmonicCount;
        }
    }

    m_pendingRequests[m_pendingCount] = request;
    ++m_pendingCount;
}

inline PingsynthScriptEngine::PendingSetHarmonicsResult PingsynthScriptEngine::drainSetHarmonicsRequests() noexcept
{
    PendingSetHarmonicsResult out{};
    out.requests = m_pendingRequests;
    out.count = m_pendingCount;
    m_pendingCount = 0;
    return out;
}

inline void PingsynthScriptEngine::notifyMpeMode(const bool mpeMode) noexcept
{
    m_mpeMode = mpeMode;
    callHandler(m_onMpeModeChangedFn, mpeMode);
}
