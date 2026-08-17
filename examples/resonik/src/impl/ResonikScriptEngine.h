#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"

struct RangeCommand
{
    float low{0.f};
    float high{0.f};
};

struct FreqRangeCommand
{
    float low{0.f};
    float high{0.f};
    std::optional<size_t> distribution;
};

struct ResonanceBodyOverride
{
    std::optional<float> freq;
    std::optional<float> decay;
    std::optional<float> gainDb;
    std::optional<float> q;
    std::optional<float> delayMs;
};

/**
 * Adds Resonik's own scripted entry points on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter/pitch-tracking machinery: aggregate range/Q commands (mirroring the
 * plugin's old dials) plus per-body fine control. The aggregate commands are drained
 * once per block, the same pattern DroneScriptEngine uses for Excite(); a body override
 * is persistent instead - read directly every block, never cleared - so it always wins
 * over whatever the aggregate commands do to that same chain.
 */
class ResonikScriptEngine : public LuaScriptEngineBase<ResonikScriptEngine>
{
  public:
    // Must stay >= ResonikImpl::kMaxChains; kept as its own constant since this file
    // doesn't know about ResonikImpl (the include goes the other way).
    static constexpr size_t kMaxBodies{100};
    using BodyOverrides = std::array<std::optional<ResonanceBodyOverride>, kMaxBodies>;

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- Resonik script - these mimic the old Decay/Gain/Delay/Q/Distribution dials as plain\n"
"-- variables; edit the numbers directly and hit Apply.\n"
"DecayMin = 0.3\n"
"DecayMax = 4.0\n"
"GainMin = -18\n"
"GainMax = 0\n"
"DelayMin = 0\n"
"DelayMax = 300\n"
"Q = 6\n"
"Distribution = 1 -- 0 = linear, 1 = logarithmic\n"
"\n"
"-- Deferred one tick: SetDecayRange/SetGainRange/etc. aren't registered yet while this\n"
"-- script's own top level is still running, only once it has finished loading.\n"
"Timer.After(1, function()\n"
"    SetDecayRange(DecayMin, DecayMax)\n"
"    SetGainRange(GainMin, GainMax)\n"
"    SetDelayRange(DelayMin, DelayMax)\n"
"    SetQ(Q)\n"
"end)\n"
"\n"
"-- Retunes the resonator bank's frequency range around the incoming signal's detected\n"
"-- pitch (YIN), gated on a minimum confidence so silence/noise doesn't retune it.\n"
"function OnPitchDetected(hz, confidence)\n"
"    if hz > 0 and confidence > 0.5 then\n"
"        SetFreqRange(hz, hz * 8, Distribution)\n"
"    end\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kResonikSkeletonHooks =
"-- Resonik script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"\n"
"-- Aggregate controls - each mimics one of the plugin's old dials. Take effect on the\n"
"-- next audio block:\n"
"--   SetFreqRange(low, high[, distribution])  Hz spread; distribution: 0 linear, 1 log\n"
"--   SetDecayRange(min, max)                  seconds spread\n"
"--   SetGainRange(minDb, maxDb)                dB spread\n"
"--   SetDelayRange(minMs, maxMs)               ms spread\n"
"--   SetQ(value)                               shared by every chain not overridden below\n"
"\n"
"-- Per-body fine control - overrides one chain (0-based index), any subset of fields; a\n"
"-- field left out stays under aggregate control for that body. Persists until this body\n"
"-- is overridden again:\n"
"--   SetResonanceBody(index, { freq = .., decay = .., gainDb = .., q = .., delayMs = .. })\n"
"\n"
"function OnPitchDetected(hz, confidence)\n"
"end\n"
"\n";
    // clang-format on

    // Shown by the popup editor's Reset button: kResonikSkeletonHooks above followed by
    // LuaScriptEngineBase::kCommonSkeletonScript, as opposed to kStubScript (deliberately
    // minimal - what a fresh patch actually plays out of the box).
    static const std::string kFullSkeletonScript;

    explicit ResonikScriptEngine(size_t poolBytes = 512 * 1024);

    // Drains (returns and clears) the latest command a script requested since the last
    // drain, or nullopt if none is pending. Called once per block, same spirit as
    // DroneScriptEngine::drainExcitations().
    [[nodiscard]] std::optional<FreqRangeCommand> drainFreqRangeCommand() noexcept;
    [[nodiscard]] std::optional<RangeCommand> drainDecayRangeCommand() noexcept;
    [[nodiscard]] std::optional<RangeCommand> drainGainRangeCommand() noexcept;
    [[nodiscard]] std::optional<RangeCommand> drainDelayRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainQCommand() noexcept;

    // Read-only: per-body overrides are persistent, not drained - see class doc.
    [[nodiscard]] const BodyOverrides& bodyOverrides() const noexcept
    {
        return m_bodyOverrides;
    }

  private:
    friend class LuaScriptEngineBase<ResonikScriptEngine>;
    void bindScriptFunctions();

    void luaSetFreqRange(float low, float high, std::optional<size_t> distribution) noexcept;
    void luaSetDecayRange(float min, float max) noexcept;
    void luaSetGainRange(float minDb, float maxDb) noexcept;
    void luaSetDelayRange(float minMs, float maxMs) noexcept;
    void luaSetQ(float value) noexcept;
    // Only reads sol::optional fields (never throws) and ignores an out-of-range index -
    // stays audio-thread-safe like every other bound setter here.
    void luaSetResonanceBody(size_t index, const sol::table& params);

    std::optional<FreqRangeCommand> m_pendingFreqRange;
    std::optional<RangeCommand> m_pendingDecayRange;
    std::optional<RangeCommand> m_pendingGainRange;
    std::optional<RangeCommand> m_pendingDelayRange;
    std::optional<float> m_pendingQ;
    BodyOverrides m_bodyOverrides{};
};

inline const std::string ResonikScriptEngine::kFullSkeletonScript =
    std::string(kResonikSkeletonHooks) + std::string(kCommonSkeletonScript);

inline ResonikScriptEngine::ResonikScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<ResonikScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void ResonikScriptEngine::bindScriptFunctions()
{
    m_lua.set_function("SetFreqRange",
                       sol::overload([this](const float low, const float high)
                                     { luaSetFreqRange(low, high, std::nullopt); },
                                     [this](const float low, const float high, const size_t distribution)
                                     { luaSetFreqRange(low, high, distribution); }));
    m_lua.set_function("SetDecayRange", &ResonikScriptEngine::luaSetDecayRange, this);
    m_lua.set_function("SetGainRange", &ResonikScriptEngine::luaSetGainRange, this);
    m_lua.set_function("SetDelayRange", &ResonikScriptEngine::luaSetDelayRange, this);
    m_lua.set_function("SetQ", &ResonikScriptEngine::luaSetQ, this);
    m_lua.set_function("SetResonanceBody", &ResonikScriptEngine::luaSetResonanceBody, this);
}

inline void ResonikScriptEngine::luaSetFreqRange(const float low, const float high,
                                                 const std::optional<size_t> distribution) noexcept
{
    m_pendingFreqRange = FreqRangeCommand{low, high, distribution};
}

inline void ResonikScriptEngine::luaSetDecayRange(const float min, const float max) noexcept
{
    m_pendingDecayRange = RangeCommand{min, max};
}

inline void ResonikScriptEngine::luaSetGainRange(const float minDb, const float maxDb) noexcept
{
    m_pendingGainRange = RangeCommand{minDb, maxDb};
}

inline void ResonikScriptEngine::luaSetDelayRange(const float minMs, const float maxMs) noexcept
{
    m_pendingDelayRange = RangeCommand{minMs, maxMs};
}

inline void ResonikScriptEngine::luaSetQ(const float value) noexcept
{
    m_pendingQ = value;
}

inline void ResonikScriptEngine::luaSetResonanceBody(const size_t index, const sol::table& params)
{
    if (index >= kMaxBodies)
    {
        return;
    }
    ResonanceBodyOverride override{};
    const sol::optional<float> freq = params["freq"];
    const sol::optional<float> decay = params["decay"];
    const sol::optional<float> gainDb = params["gainDb"];
    const sol::optional<float> q = params["q"];
    const sol::optional<float> delayMs = params["delayMs"];
    if (freq)
    {
        override.freq = *freq;
    }
    if (decay)
    {
        override.decay = *decay;
    }
    if (gainDb)
    {
        override.gainDb = *gainDb;
    }
    if (q)
    {
        override.q = *q;
    }
    if (delayMs)
    {
        override.delayMs = *delayMs;
    }
    m_bodyOverrides[index] = override;
}

inline std::optional<FreqRangeCommand> ResonikScriptEngine::drainFreqRangeCommand() noexcept
{
    const auto result = m_pendingFreqRange;
    m_pendingFreqRange.reset();
    return result;
}

inline std::optional<RangeCommand> ResonikScriptEngine::drainDecayRangeCommand() noexcept
{
    const auto result = m_pendingDecayRange;
    m_pendingDecayRange.reset();
    return result;
}

inline std::optional<RangeCommand> ResonikScriptEngine::drainGainRangeCommand() noexcept
{
    const auto result = m_pendingGainRange;
    m_pendingGainRange.reset();
    return result;
}

inline std::optional<RangeCommand> ResonikScriptEngine::drainDelayRangeCommand() noexcept
{
    const auto result = m_pendingDelayRange;
    m_pendingDelayRange.reset();
    return result;
}

inline std::optional<float> ResonikScriptEngine::drainQCommand() noexcept
{
    const auto result = m_pendingQ;
    m_pendingQ.reset();
    return result;
}
