#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"

/// @brief Which spectral voice a tap runs. Passed from Lua as a plain integer (SetTap's
/// `type` argument), never a string - matches Pingsynth's Mode dropdown convention.
enum class TapType : size_t
{
    Bypass = 0,
    LowPass = 1,
    HighPass = 2,
    BandPass = 3,
    Notch = 4,
    Resonator = 5,
    Formant = 6,
    CombResonator = 7,
    RingModulator = 8,
};

/// @brief SetTap's topology-tier payload: delay position, voice type, initial gain/pan.
struct TapTopology
{
    float delayMs{0.f};
    TapType type{TapType::Bypass};
    float level{1.f};
    float pan{0.f};
};

/// @brief SetResonance's real-time payload: frequency plus the decay time that maps to
/// Q (filter taps) or feedback gain (CombResonator). negative only affects CombResonator
/// (flips the feedback sign), silently unused by every other type.
struct ResonanceTarget
{
    float freqHz{0.f};
    float decaySeconds{0.f};
    bool negative{false};
};

/// @brief SetFormant's real-time payload: base frequency plus two derived sections.
struct FormantTarget
{
    float freqHz{0.f};
    float f1Factor{0.f};
    float f1Gain{0.f};
    float f2Factor{0.f};
    float f2Gain{0.f};
};

/**
 * Adds Spectraltap's own scripted entry points on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter/pitch-tracking machinery: one topology pair (SetMaxTaps/SetTap) and
 * five real-time per-tap setters (SetFrequency/SetResonance/SetFormant/SetPan/SetGain).
 * Every setter validates and clamps at this boundary, then stores a one-shot pending
 * command per tap index - drained once per block by SpectraltapImpl, the same
 * drain-once-per-block pattern ResonikScriptEngine uses for its aggregate range commands,
 * just indexed per tap instead of aggregated. A non-finite (NaN/Inf) argument, an
 * out-of-range tap index, or an unknown tap type is silently ignored, leaving whatever
 * topology/targets were already active untouched.
 */
class SpectraltapScriptEngine : public LuaScriptEngineBase<SpectraltapScriptEngine>
{
  public:
    static constexpr size_t kMaxTaps{24};

    static constexpr float kMinFreqHz{1.f};
    static constexpr float kMaxFreqHz{20000.f};
    static constexpr float kMinDecaySeconds{0.001f};
    static constexpr float kMaxDecaySeconds{20.f};
    static constexpr float kMinGain{0.f};
    static constexpr float kMaxGain{4.f};
    static constexpr float kMinPan{-1.f};
    static constexpr float kMaxPan{1.f};
    static constexpr float kMinDelayMs{0.f};
    static constexpr float kMaxDelayMs{4000.f};

    using TapCommands = std::array<std::optional<TapTopology>, kMaxTaps>;
    using FrequencyCommands = std::array<std::optional<float>, kMaxTaps>;
    using ResonanceCommands = std::array<std::optional<ResonanceTarget>, kMaxTaps>;
    using FormantCommands = std::array<std::optional<FormantTarget>, kMaxTaps>;
    using GainCommands = std::array<std::optional<float>, kMaxTaps>;
    using PanCommands = std::array<std::optional<float>, kMaxTaps>;

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- Spectraltap script - 4 scale-sequenced taps, retimed whenever BPM, Division, Root or\n"
"-- Scale changes.\n"
"UICreateParameterSet({\n"
"    { id = \"root\", name = \"Root\", type = \"drop\",\n"
"      items = { \"C\", \"C#\", \"D\", \"D#\", \"E\", \"F\", \"F#\", \"G\", \"G#\", \"A\", \"A#\", \"B\" }, default = 0,\n"
"      description = \"Root note the tap frequencies are built from\" },\n"
"    { id = \"scale\", name = \"Scale\", type = \"drop\",\n"
"      items = { \"Major\", \"NaturalMinor\", \"Dorian\", \"MajorPentatonic\", \"MinorPentatonic\", \"Blues\", \"WholeTone\", \"Chromatic\" },\n"
"      default = 0,\n"
"      description = \"Scale the tap frequencies are drawn from\" },\n"
"})\n"
"\n"
"local kScaleNames = { \"Major\", \"NaturalMinor\", \"Dorian\", \"MajorPentatonic\", \"MinorPentatonic\", \"Blues\", \"WholeTone\", \"Chromatic\" }\n"
"\n"
"Root = 60          -- MIDI note (C4)\n"
"Scale = kScaleNames[1]\n"
"LastBpm = 120\n"
"LastDivisionIndex = 4\n"
"\n"
"TapType = { Bypass = 0, LowPass = 1, HighPass = 2, BandPass = 3, Notch = 4, Resonator = 5, Formant = 6, CombResonator = 7, RingModulator = 8 }\n"
"\n"
"-- Division dropdown index -> beats per quarter note; mirrors the plugin's own Division\n"
"-- list (1/1 .. 1/16T), 0-based to match the index OnTiming() passes.\n"
"local kDivisionBeats = {\n"
"    [0] = 4, [1] = 2, [2] = 3, [3] = 4 / 3,\n"
"    [4] = 1, [5] = 1.5, [6] = 2 / 3,\n"
"    [7] = 0.5, [8] = 0.75, [9] = 1 / 3,\n"
"    [10] = 0.25, [11] = 0.375, [12] = 1 / 6,\n"
"}\n"
"\n"
"local degreeIndex = { 1, 3, 5, 7 }                          -- 1st/3rd/5th/7th scale steps\n"
"local types = { TapType.BandPass, TapType.Resonator, TapType.Formant, TapType.Notch }\n"
"local pans = { -0.6, 0.6, -0.6, 0.6 }\n"
"\n"
"function RetuneTaps(bpm, divisionIndex)\n"
"    SetMaxTaps(4)\n"
"    local beats = kDivisionBeats[divisionIndex] or 1\n"
"    local scale = Music.Scales[Scale]\n"
"    for i = 1, 4 do\n"
"        local delayMs = Rhythm.BeatsToMs(beats * i, bpm)\n"
"        local hz = Music.NoteToHz(Root + scale[degreeIndex[i]])\n"
"        SetTap(i - 1, delayMs, types[i], 0.8, pans[i])\n"
"        if types[i] == TapType.Formant then\n"
"            SetFormant(i - 1, hz, 2.0, 0.6, 3.5, 0.35)\n"
"        else\n"
"            SetResonance(i - 1, hz, 1.2)\n"
"        end\n"
"    end\n"
"end\n"
"\n"
"function OnRootChanged(index)\n"
"    Root = 60 + index\n"
"    RetuneTaps(LastBpm, LastDivisionIndex)\n"
"end\n"
"\n"
"function OnScaleChanged(index)\n"
"    Scale = kScaleNames[index + 1]\n"
"    RetuneTaps(LastBpm, LastDivisionIndex)\n"
"end\n"
"\n"
"-- Fires once at startup and again whenever the BPM dial, Host Sync, or the Division\n"
"-- dropdown changes - bpm is the effective tempo (manual dial, or the host's own tempo\n"
"-- while Host Sync is on), independent of the raw Transport.Tempo() every script gets.\n"
"function OnTiming(bpm, divisionIndex)\n"
"    LastBpm = bpm\n"
"    LastDivisionIndex = divisionIndex\n"
"    RetuneTaps(bpm, divisionIndex)\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kSpectraltapSkeletonHooks =
"-- Spectraltap script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"\n"
"-- Fires once at startup and again whenever the BPM dial, Host Sync switch, or Division\n"
"-- dropdown changes (not every block). bpm is the effective tempo those controls produce -\n"
"-- the manual dial, or the host's own tempo while Host Sync is on - distinct from the\n"
"-- shared Transport.Tempo() (always the raw host tempo). divisionIndex is 0-based into\n"
"-- the plugin's own Division dropdown (1/1 .. 1/16T); interpret it yourself, e.g. with a\n"
"-- local beats-per-quarter-note lookup table (see kStubScript for one).\n"
"function OnTiming(bpm, divisionIndex)\n"
"end\n"
"\n"
"-- Topology - rebuilds a tap's DSP state, not expected to be sample-accurate:\n"
"--   SetMaxTaps(n)                                   active tap count, 0..24\n"
"--   SetTap(index, delayMs, type, level, pan)         0-based index; type: 0 Bypass,\n"
"--                                                     1 LowPass, 2 HighPass, 3 BandPass,\n"
"--                                                     4 Notch, 5 Resonator, 6 Formant,\n"
"--                                                     7 CombResonator, 8 RingModulator;\n"
"--                                                     level/pan are the initial\n"
"--                                                     SetGain/SetPan targets\n"
"\n"
"-- Real-time per-tap setters - safe to call every block, smoothed, never allocate:\n"
"--   SetFrequency(index, fHz)                                   centre/fundamental Hz\n"
"--   SetResonance(index, fHz, decayTimeSeconds[, negative])      frequency + decay time;\n"
"--                                                               negative (CombResonator\n"
"--                                                               only) flips the feedback\n"
"--                                                               sign - peaks move to odd\n"
"--                                                               harmonics of half fHz\n"
"--   SetFormant(index, fHz, f1Factor, f1Gain, f2Factor, f2Gain) F0=fHz, F1=f1Factor*F0,\n"
"--                                                               F2=f2Factor*F0, linear gains\n"
"--   SetPan(index, pan)                                         -1..1, constant-power\n"
"--   SetGain(index, gain)                                       linear per-tap output gain\n"
"\n";
    // clang-format on

    // Shown by the popup editor's Reset button: kSpectraltapSkeletonHooks above plus
    // LuaScriptEngineBase::kCommonSkeletonScript, unlike kStubScript (deliberately minimal).
    static const std::string kFullSkeletonScript;

    explicit SpectraltapScriptEngine(size_t poolBytes = 512 * 1024);

    // Fires OnTiming(bpm, divisionIndex) - see SpectraltapImpl::notifyTimingIfChanged(),
    // which calls this only when the effective bpm or division actually changed.
    void notifyTiming(float bpm, int divisionIndex) noexcept;

    [[nodiscard]] std::optional<size_t> drainMaxTapsCommand() noexcept;
    [[nodiscard]] std::optional<TapTopology> drainTapCommand(size_t index) noexcept;
    [[nodiscard]] std::optional<float> drainFrequencyCommand(size_t index) noexcept;
    [[nodiscard]] std::optional<ResonanceTarget> drainResonanceCommand(size_t index) noexcept;
    [[nodiscard]] std::optional<FormantTarget> drainFormantCommand(size_t index) noexcept;
    [[nodiscard]] std::optional<float> drainGainCommand(size_t index) noexcept;
    [[nodiscard]] std::optional<float> drainPanCommand(size_t index) noexcept;

  private:
    friend class LuaScriptEngineBase<SpectraltapScriptEngine>;
    void bindScriptFunctions();

    void luaSetMaxTaps(size_t n) noexcept;
    void luaSetTap(size_t index, float delayMs, size_t type, float level, float pan) noexcept;
    void luaSetFrequency(size_t index, float fHz) noexcept;
    void luaSetResonance(size_t index, float fHz, float decaySeconds, bool negative = false) noexcept;
    void luaSetFormant(size_t index, float fHz, float f1Factor, float f1Gain, float f2Factor, float f2Gain) noexcept;
    void luaSetPan(size_t index, float pan) noexcept;
    void luaSetGain(size_t index, float gain) noexcept;

    std::optional<size_t> m_pendingMaxTaps;
    TapCommands m_pendingTap{};
    FrequencyCommands m_pendingFrequency{};
    ResonanceCommands m_pendingResonance{};
    FormantCommands m_pendingFormant{};
    GainCommands m_pendingGain{};
    PanCommands m_pendingPan{};
    sol::protected_function m_onTimingFn;
};

inline const std::string SpectraltapScriptEngine::kFullSkeletonScript =
    std::string(kSpectraltapSkeletonHooks) + std::string(kCommonSkeletonScript);

inline SpectraltapScriptEngine::SpectraltapScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<SpectraltapScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void SpectraltapScriptEngine::bindScriptFunctions()
{
    m_lua.set_function("SetMaxTaps", &SpectraltapScriptEngine::luaSetMaxTaps, this);
    m_lua.set_function("SetTap", &SpectraltapScriptEngine::luaSetTap, this);
    m_lua.set_function("SetFrequency", &SpectraltapScriptEngine::luaSetFrequency, this);
    m_lua.set_function(
        "SetResonance",
        sol::overload([this](const size_t index, const float fHz, const float decaySeconds)
                      { luaSetResonance(index, fHz, decaySeconds); },
                      [this](const size_t index, const float fHz, const float decaySeconds, const bool negative)
                      { luaSetResonance(index, fHz, decaySeconds, negative); }));
    m_lua.set_function("SetFormant", &SpectraltapScriptEngine::luaSetFormant, this);
    m_lua.set_function("SetPan", &SpectraltapScriptEngine::luaSetPan, this);
    m_lua.set_function("SetGain", &SpectraltapScriptEngine::luaSetGain, this);
    m_onTimingFn = m_lua["OnTiming"];
}

inline void SpectraltapScriptEngine::notifyTiming(const float bpm, const int divisionIndex) noexcept
{
    callHandler(m_onTimingFn, bpm, divisionIndex);
}

inline void SpectraltapScriptEngine::luaSetMaxTaps(const size_t n) noexcept
{
    m_pendingMaxTaps = std::min(n, kMaxTaps);
}

inline void SpectraltapScriptEngine::luaSetTap(const size_t index, const float delayMs, const size_t type,
                                               const float level, const float pan) noexcept
{
    if (index >= kMaxTaps || type > static_cast<size_t>(TapType::RingModulator))
    {
        return;
    }
    if (!std::isfinite(delayMs) || !std::isfinite(level) || !std::isfinite(pan))
    {
        return;
    }
    m_pendingTap[index] = TapTopology{std::clamp(delayMs, kMinDelayMs, kMaxDelayMs), static_cast<TapType>(type),
                                      std::clamp(level, kMinGain, kMaxGain), std::clamp(pan, kMinPan, kMaxPan)};
}

inline void SpectraltapScriptEngine::luaSetFrequency(const size_t index, const float fHz) noexcept
{
    if (index >= kMaxTaps || !std::isfinite(fHz))
    {
        return;
    }
    m_pendingFrequency[index] = std::clamp(fHz, kMinFreqHz, kMaxFreqHz);
}

inline void SpectraltapScriptEngine::luaSetResonance(const size_t index, const float fHz, const float decaySeconds,
                                                     const bool negative) noexcept
{
    if (index >= kMaxTaps || !std::isfinite(fHz) || !std::isfinite(decaySeconds))
    {
        return;
    }
    m_pendingResonance[index] = ResonanceTarget{std::clamp(fHz, kMinFreqHz, kMaxFreqHz),
                                                std::clamp(decaySeconds, kMinDecaySeconds, kMaxDecaySeconds), negative};
}

inline void SpectraltapScriptEngine::luaSetFormant(const size_t index, const float fHz, const float f1Factor,
                                                   const float f1Gain, const float f2Factor,
                                                   const float f2Gain) noexcept
{
    if (index >= kMaxTaps)
    {
        return;
    }
    if (!std::isfinite(fHz) || !std::isfinite(f1Factor) || !std::isfinite(f1Gain) || !std::isfinite(f2Factor) ||
        !std::isfinite(f2Gain))
    {
        return;
    }
    constexpr float kMinFactor{0.1f};
    constexpr float kMaxFactor{16.f};
    m_pendingFormant[index] =
        FormantTarget{std::clamp(fHz, kMinFreqHz, kMaxFreqHz), std::clamp(f1Factor, kMinFactor, kMaxFactor),
                      std::clamp(f1Gain, kMinGain, kMaxGain), std::clamp(f2Factor, kMinFactor, kMaxFactor),
                      std::clamp(f2Gain, kMinGain, kMaxGain)};
}

inline void SpectraltapScriptEngine::luaSetPan(const size_t index, const float pan) noexcept
{
    if (index >= kMaxTaps || !std::isfinite(pan))
    {
        return;
    }
    m_pendingPan[index] = std::clamp(pan, kMinPan, kMaxPan);
}

inline void SpectraltapScriptEngine::luaSetGain(const size_t index, const float gain) noexcept
{
    if (index >= kMaxTaps || !std::isfinite(gain))
    {
        return;
    }
    m_pendingGain[index] = std::clamp(gain, kMinGain, kMaxGain);
}

inline std::optional<size_t> SpectraltapScriptEngine::drainMaxTapsCommand() noexcept
{
    const auto result = m_pendingMaxTaps;
    m_pendingMaxTaps.reset();
    return result;
}

inline std::optional<TapTopology> SpectraltapScriptEngine::drainTapCommand(const size_t index) noexcept
{
    const auto result = m_pendingTap[index];
    m_pendingTap[index].reset();
    return result;
}

inline std::optional<float> SpectraltapScriptEngine::drainFrequencyCommand(const size_t index) noexcept
{
    const auto result = m_pendingFrequency[index];
    m_pendingFrequency[index].reset();
    return result;
}

inline std::optional<ResonanceTarget> SpectraltapScriptEngine::drainResonanceCommand(const size_t index) noexcept
{
    const auto result = m_pendingResonance[index];
    m_pendingResonance[index].reset();
    return result;
}

inline std::optional<FormantTarget> SpectraltapScriptEngine::drainFormantCommand(const size_t index) noexcept
{
    const auto result = m_pendingFormant[index];
    m_pendingFormant[index].reset();
    return result;
}

inline std::optional<float> SpectraltapScriptEngine::drainGainCommand(const size_t index) noexcept
{
    const auto result = m_pendingGain[index];
    m_pendingGain[index].reset();
    return result;
}

inline std::optional<float> SpectraltapScriptEngine::drainPanCommand(const size_t index) noexcept
{
    const auto result = m_pendingPan[index];
    m_pendingPan[index].reset();
    return result;
}
