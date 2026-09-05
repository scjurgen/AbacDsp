#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"

/// @brief SetOscillator's payload: one of the 3 oscillators' waveform/tuning/level.
struct MorphexOscillatorSettings
{
    size_t waveform{0}; ///< 0 Triangle, 1 SharkFin, 2 Saw, 3 Square, 4 White
    float detune{0.f};  ///< semitones
    float level{0.f};   ///< -1..1
    float pwm{0.f};     ///< -1..1, pulse-width-style phase distortion
    float pitchFactor{1.f};
};

/// @brief SetAmpEnvelope/SetFilterEnvelope's payload: one ADSR shape.
struct MorphexEnvelopeSettings
{
    float attackMs{10.f};
    float decayMs{200.f};
    float sustainLevel{0.5f};
    float releaseMs{100.f};
};

/// @brief SetPitchEnvelope's payload: the pitch-EG depth/time plus note-to-note glide.
struct MorphexPitchEnvelopeSettings
{
    float attackMs{0.f};
    float decayMs{0.f};
    float depthSemitones{0.f};
    float glideMsPerOctave{0.f};
};

/// @brief SetLfo's payload. 0 Sine, 1 Triangle, 2 Saw, 3 Square, 4 Noise, 5 SampleHoldNoise,
/// 6 SampleHoldFlipFlop, 7 BrownNoise.
struct MorphexLfoSettings
{
    size_t waveform{0};
    float speedHz{1.f};
    float filterDepth{0.f};
    float oscDepth{0.f}; ///< semitones; applies uniformly to all 3 oscillators, via the pitch-bend bus
    float keyFollow{0.f};
};

/// @brief SetFilter's payload: cutoff (MIDI-note-ish, matching the Cutoff dial's own
/// scale), a normalized resonance (0 none, 1 near self-oscillation), and a preset name
/// out of PoleMixingFilter.h's poleMixingList (e.g. "LP4", "HP2", "BP4", "Notch").
struct MorphexFilterSettings
{
    float cutoff{72.f};
    float resonance{0.f};
    std::string type{"LP4"};
};

/// @brief SetCtrlSlot's payload: one of the 10 MPE routing-matrix slots.
struct MorphexCtrlSlotSettings
{
    size_t source{0};    ///< CtrlDimension: 0 X, 1 Y, 2 Z, 3 Velocity, 4 Note, 5 EnvelopeFilter, 6 EnvelopeAmplitude
    size_t curve{2};     ///< CtrlCurve: 0 CubeRoot, 1 SquareRoot, 2 Linear, 3 Square, 4 Cube
    size_t target{0};    ///< CtrlTarget: see the skeleton script comment for the full list
    size_t valueType{0}; ///< CtrlValueType: 0 Abs, 1 BiPolar
    float depth{0.f};
};

/// @brief SetMpeZone's payload: 1-indexed MIDI channel numbers, matching how a user sees them.
struct MorphexMpeZoneSettings
{
    int master{1};
    int lower{2};
    int upper{16};
};

/// @brief SetPhaser's payload: the master-bus phaser (8 allpass poles total - 2 stages
/// in series per channel, both swept by one shared LFO).
struct MorphexPhaserSettings
{
    float rateHz{0.3f};
    float depth{0.5f};   ///< 0..1, how far the sweep spans the fixed 200 Hz..2 kHz range
    float feedback{0.f}; ///< 0..~1, resonance around the allpass chain; near 1 approaches self-oscillation
    float mix{0.5f};     ///< 0 dry, 1 fully phased
};

/// @brief SetChorus's payload: the master-bus stereo chorus (one modulated delay per
/// channel, right channel phase-offset from left for width).
struct MorphexChorusSettings
{
    float rateHz{0.6f};
    float depth{0.5f}; ///< 0..1
    float mix{0.5f};   ///< 0 dry, 1 fully wet
};

/**
 * Adds morphexsynth's own scripted entry points on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter machinery. Unlike Pingsynth (whose C++ voice makes no sound at all
 * without a script feeding it harmonics), morphexsynth's voice is a complete subtractive
 * synth on its own - every setter here customizes an already-playing instrument rather
 * than supplying its content, so a script is optional, not required, for basic sound.
 *
 * Every setter validates and clamps at this boundary, then stores a one-shot pending
 * command - drained once per block by MorphexsynthImpl, the same drain-once-per-block
 * pattern SpectraltapScriptEngine/PingsynthScriptEngine use. A non-finite (NaN/Inf)
 * argument or an out-of-range index/enum silently drops the whole call, leaving whatever
 * was already active untouched.
 */
class MorphexsynthScriptEngine : public LuaScriptEngineBase<MorphexsynthScriptEngine>
{
  public:
    static constexpr size_t kNumOscillators{3};
    static constexpr size_t kNumMpeSlots{10};
    static constexpr size_t kNumOscillatorWaveforms{5};
    static constexpr size_t kNumLfoWaveforms{8};
    static constexpr size_t kNumCtrlDimensions{7};
    static constexpr size_t kNumCtrlCurves{5};
    static constexpr size_t kNumCtrlTargets{17};
    static constexpr size_t kNumCtrlValueTypes{2};

    using OscillatorCommands = std::array<std::optional<MorphexOscillatorSettings>, kNumOscillators>;
    using CtrlSlotCommands = std::array<std::optional<MorphexCtrlSlotSettings>, kNumMpeSlots>;

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- morphexsynth script - the C++ voice already plays on its own (Vol/Cutoff/Reso dials\n"
"-- cover the basics); this stub layers a slow LFO-to-filter sweep and one MPE routing\n"
"-- slot (MPE X -> pitch bend) on top. Edit freely - see the Reset button for every hook.\n"
"-- OnStart() fires once the plugin's engine is ready (see MorphexsynthImpl); a SetXxx()\n"
"-- call needs to happen from inside a hook like this one, never at the script's own top\n"
"-- level - top-level code runs before this script's own SetXxx() bindings exist yet.\n"
"function OnStart()\n"
"    SetLfo({ waveform = 0, speedHz = 0.15, filterDepth = 0.6, keyFollow = 0 })\n"
"    SetCtrlSlot(0, { source = 0, curve = 2, target = 1, valueType = 1, depth = 1 })\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kMorphexsynthSkeletonHooks =
"-- morphexsynth script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"-- The C++ voice already plays without any of this - these calls customize an\n"
"-- already-working instrument, they don't supply its sound the way Pingsynth's do.\n"
"-- Every SetXxx() below must be called from inside a hook (OnStart, OnNoteOn, a\n"
"-- UICreateParameterSet callback, ...), never at this script's own top level - see\n"
"-- OnStart() below for the usual place to set a patch's static configuration once.\n"
"\n"
"-- SetOscillator(index, { waveform, detune, level, pwm, pitchFactor })  index 0..2\n"
"--   waveform: 0 Triangle, 1 SharkFin, 2 Saw, 3 Square, 4 White\n"
"--   detune: semitones. level: -1..1. pwm: -1..1. pitchFactor: frequency multiplier (1 = unison)\n"
"\n"
"-- SetAmpEnvelope({ attackMs, decayMs, sustainLevel, releaseMs })\n"
"-- SetFilterEnvelope({ attackMs, decayMs, sustainLevel, releaseMs })\n"
"-- SetPitchEnvelope({ attackMs, decayMs, depthSemitones, glideMsPerOctave })\n"
"\n"
"-- SetLfo({ waveform, speedHz, filterDepth, oscDepth, keyFollow })\n"
"--   waveform: 0 Sine, 1 Triangle, 2 Saw, 3 Square, 4 Noise, 5 SampleHoldNoise,\n"
"--             6 SampleHoldFlipFlop, 7 BrownNoise\n"
"--   oscDepth: semitones of pitch modulation, applied to all 3 oscillators uniformly\n"
"\n"
"-- SetFilter({ cutoff, resonance, type })  cutoff matches the Cutoff dial's own scale;\n"
"--   resonance 0..~1.2 (1 is near self-oscillation); type is a PoleMixingFilter.h preset\n"
"--   name, e.g. \"LP4\", \"HP2\", \"BP4\", \"Notch\" (see PoleMixingFilter.h's poleMixingList\n"
"--   for the full ~48-entry set)\n"
"\n"
"-- SetDistortion(presetIndex)  0 = off/bypass, 1.. = WaveShaperTables.h preset (1-indexed)\n"
"\n"
"-- SetCtrlSlot(slot, { source, curve, target, valueType, depth })  slot 0..9, one of the\n"
"-- 10 MPE routing-matrix slots. Each slot reads one source, shapes it through curve, scales\n"
"-- by depth, and adds the result into target.\n"
"--   source: 0 X, 1 Y, 2 Z, 3 Velocity, 4 Note, 5 EnvelopeFilter, 6 EnvelopeAmplitude\n"
"--   curve: 0 CubeRoot, 1 SquareRoot, 2 Linear, 3 Square, 4 Cube\n"
"--   valueType: 0 Abs, 1 BiPolar (BiPolar is the natural choice for pitch bend / any\n"
"--             signed-deflection source; Abs for a one-directional source like Velocity)\n"
"--   target: 0 None, 1 PitchBend, 2 PitchBendSecondary, 3 FilterCutoff, 4 FilterResonance,\n"
"--           5 Pan, 6 OscFreq2, 7 OscFreq3, 8 OscLevel1, 9 OscLevel2, 10 OscLevel3,\n"
"--           11 LfoLevel (unused), 12 SustainVol, 13 AttackTime, 14 DecayTime,\n"
"--           15 ReleaseTime, 16 Distortion\n"
"\n"
"-- SetMpeZone(masterChannel, lowerChannel, upperChannel)  1-indexed MIDI channels.\n"
"-- Default Lower Zone: master=1, lower=2, upper=16. A note-on outside [master, lower..upper]\n"
"-- is ignored.\n"
"\n"
"-- SetPhaser({ rateHz, depth, feedback, mix })  master-bus phaser, 8 allpass poles total\n"
"--   (2 stages in series per channel), both channels swept by one shared LFO.\n"
"--   rateHz: 0.01..10. depth: 0..1, how far the sweep spans 200 Hz..2 kHz.\n"
"--   feedback: 0..~1.1, resonance around the allpass chain. mix: 0 dry..1 fully phased\n"
"\n"
"-- SetChorus({ rateHz, depth, mix })  master-bus stereo chorus, one modulated delay per\n"
"--   channel, right channel phase-offset from left for width.\n"
"--   rateHz: 0.01..8. depth: 0..1. mix: 0 dry..1 fully wet\n"
"\n";
    // clang-format on

    // Shown by the popup editor's Reset button, not the engine's own default script.
    static const std::string kFullSkeletonScript;

    explicit MorphexsynthScriptEngine(size_t poolBytes = 512 * 1024);

    [[nodiscard]] std::optional<MorphexOscillatorSettings> drainOscillatorCommand(size_t index) noexcept;
    [[nodiscard]] std::optional<MorphexEnvelopeSettings> drainAmpEnvelopeCommand() noexcept;
    [[nodiscard]] std::optional<MorphexEnvelopeSettings> drainFilterEnvelopeCommand() noexcept;
    [[nodiscard]] std::optional<MorphexPitchEnvelopeSettings> drainPitchEnvelopeCommand() noexcept;
    [[nodiscard]] std::optional<MorphexLfoSettings> drainLfoCommand() noexcept;
    [[nodiscard]] std::optional<MorphexFilterSettings> drainFilterCommand() noexcept;
    [[nodiscard]] std::optional<size_t> drainDistortionCommand() noexcept;
    [[nodiscard]] std::optional<MorphexCtrlSlotSettings> drainCtrlSlotCommand(size_t slot) noexcept;
    [[nodiscard]] std::optional<MorphexMpeZoneSettings> drainMpeZoneCommand() noexcept;
    [[nodiscard]] std::optional<MorphexPhaserSettings> drainPhaserCommand() noexcept;
    [[nodiscard]] std::optional<MorphexChorusSettings> drainChorusCommand() noexcept;

  private:
    friend class LuaScriptEngineBase<MorphexsynthScriptEngine>;
    void bindScriptFunctions();

    void luaSetOscillator(size_t index, const sol::table& params) noexcept;
    void luaSetAmpEnvelope(const sol::table& params) noexcept;
    void luaSetFilterEnvelope(const sol::table& params) noexcept;
    void luaSetPitchEnvelope(const sol::table& params) noexcept;
    void luaSetLfo(const sol::table& params) noexcept;
    void luaSetFilter(const sol::table& params) noexcept;
    void luaSetDistortion(size_t presetIndex) noexcept;
    void luaSetCtrlSlot(size_t slot, const sol::table& params) noexcept;
    void luaSetMpeZone(int master, int lower, int upper) noexcept;
    void luaSetPhaser(const sol::table& params) noexcept;
    void luaSetChorus(const sol::table& params) noexcept;

    static MorphexEnvelopeSettings parseEnvelope(const sol::table& params) noexcept;

    OscillatorCommands m_pendingOscillator{};
    std::optional<MorphexEnvelopeSettings> m_pendingAmpEnvelope;
    std::optional<MorphexEnvelopeSettings> m_pendingFilterEnvelope;
    std::optional<MorphexPitchEnvelopeSettings> m_pendingPitchEnvelope;
    std::optional<MorphexLfoSettings> m_pendingLfo;
    std::optional<MorphexFilterSettings> m_pendingFilter;
    std::optional<size_t> m_pendingDistortion;
    CtrlSlotCommands m_pendingCtrlSlot{};
    std::optional<MorphexMpeZoneSettings> m_pendingMpeZone;
    std::optional<MorphexPhaserSettings> m_pendingPhaser;
    std::optional<MorphexChorusSettings> m_pendingChorus;
};

inline const std::string MorphexsynthScriptEngine::kFullSkeletonScript =
    std::string(kMorphexsynthSkeletonHooks) + std::string(kCommonSkeletonScript);

inline MorphexsynthScriptEngine::MorphexsynthScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<MorphexsynthScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void MorphexsynthScriptEngine::bindScriptFunctions()
{
    m_lua.set_function("SetOscillator", &MorphexsynthScriptEngine::luaSetOscillator, this);
    m_lua.set_function("SetAmpEnvelope", &MorphexsynthScriptEngine::luaSetAmpEnvelope, this);
    m_lua.set_function("SetFilterEnvelope", &MorphexsynthScriptEngine::luaSetFilterEnvelope, this);
    m_lua.set_function("SetPitchEnvelope", &MorphexsynthScriptEngine::luaSetPitchEnvelope, this);
    m_lua.set_function("SetLfo", &MorphexsynthScriptEngine::luaSetLfo, this);
    m_lua.set_function("SetFilter", &MorphexsynthScriptEngine::luaSetFilter, this);
    m_lua.set_function("SetDistortion", &MorphexsynthScriptEngine::luaSetDistortion, this);
    m_lua.set_function("SetCtrlSlot", &MorphexsynthScriptEngine::luaSetCtrlSlot, this);
    m_lua.set_function("SetMpeZone", &MorphexsynthScriptEngine::luaSetMpeZone, this);
    m_lua.set_function("SetPhaser", &MorphexsynthScriptEngine::luaSetPhaser, this);
    m_lua.set_function("SetChorus", &MorphexsynthScriptEngine::luaSetChorus, this);
}

inline void MorphexsynthScriptEngine::luaSetOscillator(const size_t index, const sol::table& params) noexcept
{
    if (index >= kNumOscillators)
    {
        return;
    }
    const size_t waveform = params.get_or("waveform", size_t{0});
    const float detune = params.get_or("detune", 0.f);
    const float level = params.get_or("level", 0.f);
    const float pwm = params.get_or("pwm", 0.f);
    const float pitchFactor = params.get_or("pitchFactor", 1.f);
    if (waveform >= kNumOscillatorWaveforms || !std::isfinite(detune) || !std::isfinite(level) || !std::isfinite(pwm) ||
        !std::isfinite(pitchFactor))
    {
        return;
    }
    m_pendingOscillator[index] =
        MorphexOscillatorSettings{waveform, std::clamp(detune, -48.f, 48.f), std::clamp(level, -1.f, 1.f),
                                  std::clamp(pwm, -1.f, 1.f), std::clamp(pitchFactor, 0.01f, 16.f)};
}

inline MorphexEnvelopeSettings MorphexsynthScriptEngine::parseEnvelope(const sol::table& params) noexcept
{
    const float attackMs = std::clamp(static_cast<float>(params.get_or("attackMs", 10.f)), 0.f, 20000.f);
    const float decayMs = std::clamp(static_cast<float>(params.get_or("decayMs", 200.f)), 0.f, 20000.f);
    const float sustainLevel = std::clamp(static_cast<float>(params.get_or("sustainLevel", 0.5f)), 0.f, 1.f);
    const float releaseMs = std::clamp(static_cast<float>(params.get_or("releaseMs", 100.f)), 0.f, 20000.f);
    return {attackMs, decayMs, sustainLevel, releaseMs};
}

inline void MorphexsynthScriptEngine::luaSetAmpEnvelope(const sol::table& params) noexcept
{
    m_pendingAmpEnvelope = parseEnvelope(params);
}

inline void MorphexsynthScriptEngine::luaSetFilterEnvelope(const sol::table& params) noexcept
{
    m_pendingFilterEnvelope = parseEnvelope(params);
}

inline void MorphexsynthScriptEngine::luaSetPitchEnvelope(const sol::table& params) noexcept
{
    const float attackMs = params.get_or("attackMs", 0.f);
    const float decayMs = params.get_or("decayMs", 0.f);
    const float depthSemitones = params.get_or("depthSemitones", 0.f);
    const float glideMsPerOctave = params.get_or("glideMsPerOctave", 0.f);
    if (!std::isfinite(attackMs) || !std::isfinite(decayMs) || !std::isfinite(depthSemitones) ||
        !std::isfinite(glideMsPerOctave))
    {
        return;
    }
    m_pendingPitchEnvelope = MorphexPitchEnvelopeSettings{
        std::clamp(attackMs, 0.f, 20000.f), std::clamp(decayMs, 0.f, 20000.f), std::clamp(depthSemitones, -48.f, 48.f),
        std::clamp(glideMsPerOctave, 0.f, 5000.f)};
}

inline void MorphexsynthScriptEngine::luaSetLfo(const sol::table& params) noexcept
{
    const size_t waveform = params.get_or("waveform", size_t{0});
    const float speedHz = params.get_or("speedHz", 1.f);
    const float filterDepth = params.get_or("filterDepth", 0.f);
    const float oscDepth = params.get_or("oscDepth", 0.f);
    const float keyFollow = params.get_or("keyFollow", 0.f);
    if (waveform >= kNumLfoWaveforms || !std::isfinite(speedHz) || !std::isfinite(filterDepth) ||
        !std::isfinite(oscDepth) || !std::isfinite(keyFollow))
    {
        return;
    }
    m_pendingLfo = MorphexLfoSettings{waveform, std::clamp(speedHz, 0.01f, 50.f), std::clamp(filterDepth, -4.f, 4.f),
                                      std::clamp(oscDepth, -4.f, 4.f), std::clamp(keyFollow, -4.f, 4.f)};
}

inline void MorphexsynthScriptEngine::luaSetFilter(const sol::table& params) noexcept
{
    const float cutoff = params.get_or("cutoff", 72.f);
    const float resonance = params.get_or("resonance", 0.f);
    const std::string type = params.get_or("type", std::string{"LP4"});
    if (!std::isfinite(cutoff) || !std::isfinite(resonance) || type.empty())
    {
        return;
    }
    m_pendingFilter = MorphexFilterSettings{std::clamp(cutoff, 0.f, 135.f), std::clamp(resonance, 0.f, 2.f), type};
}

inline void MorphexsynthScriptEngine::luaSetDistortion(const size_t presetIndex) noexcept
{
    m_pendingDistortion = presetIndex;
}

inline void MorphexsynthScriptEngine::luaSetCtrlSlot(const size_t slot, const sol::table& params) noexcept
{
    if (slot >= kNumMpeSlots)
    {
        return;
    }
    const size_t source = params.get_or("source", size_t{0});
    const size_t curve = params.get_or("curve", size_t{2});
    const size_t target = params.get_or("target", size_t{0});
    const size_t valueType = params.get_or("valueType", size_t{0});
    const float depth = params.get_or("depth", 0.f);
    if (source >= kNumCtrlDimensions || curve >= kNumCtrlCurves || target >= kNumCtrlTargets ||
        valueType >= kNumCtrlValueTypes || !std::isfinite(depth))
    {
        return;
    }
    m_pendingCtrlSlot[slot] = MorphexCtrlSlotSettings{source, curve, target, valueType, depth};
}

inline void MorphexsynthScriptEngine::luaSetMpeZone(const int master, const int lower, const int upper) noexcept
{
    if (master < 1 || master > 16 || lower < 1 || lower > 16 || upper < 1 || upper > 16)
    {
        return;
    }
    m_pendingMpeZone = MorphexMpeZoneSettings{master, lower, upper};
}

inline void MorphexsynthScriptEngine::luaSetPhaser(const sol::table& params) noexcept
{
    const float rateHz = params.get_or("rateHz", 0.3f);
    const float depth = params.get_or("depth", 0.5f);
    const float feedback = params.get_or("feedback", 0.f);
    const float mix = params.get_or("mix", 0.5f);
    if (!std::isfinite(rateHz) || !std::isfinite(depth) || !std::isfinite(feedback) || !std::isfinite(mix))
    {
        return;
    }
    m_pendingPhaser = MorphexPhaserSettings{std::clamp(rateHz, 0.01f, 10.f), std::clamp(depth, 0.f, 1.f),
                                            std::clamp(feedback, 0.f, 1.1f), std::clamp(mix, 0.f, 1.f)};
}

inline void MorphexsynthScriptEngine::luaSetChorus(const sol::table& params) noexcept
{
    const float rateHz = params.get_or("rateHz", 0.6f);
    const float depth = params.get_or("depth", 0.5f);
    const float mix = params.get_or("mix", 0.5f);
    if (!std::isfinite(rateHz) || !std::isfinite(depth) || !std::isfinite(mix))
    {
        return;
    }
    m_pendingChorus =
        MorphexChorusSettings{std::clamp(rateHz, 0.01f, 8.f), std::clamp(depth, 0.f, 1.f), std::clamp(mix, 0.f, 1.f)};
}

inline std::optional<MorphexOscillatorSettings> MorphexsynthScriptEngine::drainOscillatorCommand(
    const size_t index) noexcept
{
    const auto result = m_pendingOscillator[index];
    m_pendingOscillator[index].reset();
    return result;
}

inline std::optional<MorphexEnvelopeSettings> MorphexsynthScriptEngine::drainAmpEnvelopeCommand() noexcept
{
    const auto result = m_pendingAmpEnvelope;
    m_pendingAmpEnvelope.reset();
    return result;
}

inline std::optional<MorphexEnvelopeSettings> MorphexsynthScriptEngine::drainFilterEnvelopeCommand() noexcept
{
    const auto result = m_pendingFilterEnvelope;
    m_pendingFilterEnvelope.reset();
    return result;
}

inline std::optional<MorphexPitchEnvelopeSettings> MorphexsynthScriptEngine::drainPitchEnvelopeCommand() noexcept
{
    const auto result = m_pendingPitchEnvelope;
    m_pendingPitchEnvelope.reset();
    return result;
}

inline std::optional<MorphexLfoSettings> MorphexsynthScriptEngine::drainLfoCommand() noexcept
{
    const auto result = m_pendingLfo;
    m_pendingLfo.reset();
    return result;
}

inline std::optional<MorphexFilterSettings> MorphexsynthScriptEngine::drainFilterCommand() noexcept
{
    const auto result = m_pendingFilter;
    m_pendingFilter.reset();
    return result;
}

inline std::optional<size_t> MorphexsynthScriptEngine::drainDistortionCommand() noexcept
{
    const auto result = m_pendingDistortion;
    m_pendingDistortion.reset();
    return result;
}

inline std::optional<MorphexCtrlSlotSettings> MorphexsynthScriptEngine::drainCtrlSlotCommand(const size_t slot) noexcept
{
    const auto result = m_pendingCtrlSlot[slot];
    m_pendingCtrlSlot[slot].reset();
    return result;
}

inline std::optional<MorphexMpeZoneSettings> MorphexsynthScriptEngine::drainMpeZoneCommand() noexcept
{
    const auto result = m_pendingMpeZone;
    m_pendingMpeZone.reset();
    return result;
}

inline std::optional<MorphexPhaserSettings> MorphexsynthScriptEngine::drainPhaserCommand() noexcept
{
    const auto result = m_pendingPhaser;
    m_pendingPhaser.reset();
    return result;
}

inline std::optional<MorphexChorusSettings> MorphexsynthScriptEngine::drainChorusCommand() noexcept
{
    const auto result = m_pendingChorus;
    m_pendingChorus.reset();
    return result;
}
