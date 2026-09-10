#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"
#include "Harmony/HarmonicPalette.h"
#include "Harmony/HarmonicPreferences.h"

/// @brief SetOscillator's payload: one of a channel's 2 oscillators' material path/tuning/level.
struct AmbientOscillatorSettings
{
    size_t waveform{0}; ///< material path: 0 Saw-Sine-Square, 1 Triangle-Sine-SharkFin, 2 Square-White-Saw
    float level{0.f};   ///< -1..1
    float height{0.f};  ///< semitones offset from the played note
    float cents{0.f};   ///< fine tune, cents
};

/**
 * Adds ambientpad's own scripted entry points on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter machinery. There is no MIDI input in this phase: NoteOn/NoteOff are the
 * only way to sound a voice, whether called from a script or (via AmbientPadImpl) the
 * standalone's own Note/Play controls.
 *
 * `channel` addresses a voice slot directly (1..kMaxChannels, no stealing - see
 * AmbientPadImpl) rather than picking from a pool. SetOscillator/SetGain/SetPitch/NoteOn/NoteOff
 * are per-channel; the musical-intent, effects, and harmonic-organism setters (SetHarmony*,
 * SetPedalNote, SetPedalChannels, the impulse gestures) describe the one shared patch and
 * broadcast to every voice, mirroring the standalone's own dials.
 *
 * Every setter validates and clamps at this boundary, then stores a one-shot pending command -
 * drained once per block by AmbientPadImpl, the same pattern MorphexsynthScriptEngine uses. A
 * non-finite argument or an out-of-range index/channel silently drops the whole call.
 */
class AmbientPadScriptEngine : public LuaScriptEngineBase<AmbientPadScriptEngine>
{
  public:
    static constexpr size_t kMaxChannels{16}; ///< must match AmbientPadImpl::kMaxVoices
    static constexpr size_t kNumOscillators{2};
    static constexpr size_t kNumMaterialPaths{3};
    static constexpr size_t kMaxNoteEventsPerBlock{16};

    struct NoteEvent
    {
        size_t channel{0};
        int note{69};
        int velocity{100};
        bool isOn{true};
    };

    using OscillatorCommands = std::array<std::optional<AmbientOscillatorSettings>, kNumOscillators>;
    using PerChannelOscillatorCommands = std::array<OscillatorCommands, kMaxChannels>;
    using GainCommands = std::array<std::optional<float>, kMaxChannels>;

    /// @brief SetPitch's payload: a channel's held note, glide time in seconds (0 = instant).
    struct AmbientPitchSettings
    {
        int note{69};
        float cents{0.f};
        float glideTimeSeconds{0.f};
    };
    using PitchCommands = std::array<std::optional<AmbientPitchSettings>, kMaxChannels>;

    static constexpr size_t kMaxPendingImpulsesPerBlock{8};

    /// @brief SetPedalChannels' payload: which channels (1-indexed) are pedal channels - see
    /// the plan's design decision 4. Replaces the whole set each call.
    struct PedalChannelsCommand
    {
        std::array<int, kMaxChannels> channels{};
        size_t count{0};
    };

    static constexpr size_t kMaxCustomPaletteEntries{AbacDsp::kMaxPaletteSize};

    /// @brief AddHarmonicState/ClearHarmonicPalette's payload: the full script-authored
    /// custom palette built so far - drained and applied as one atomic replace.
    struct CustomPaletteCommand
    {
        std::array<AbacDsp::HarmonicState, kMaxCustomPaletteEntries> entries{};
        size_t count{0};
    };

    /// @brief SetHarmonyTiming's payload: overrides for the organism's dwell/cooldown pace
    /// and the realizer's per-transition glide time - each defaults to today's fixed value.
    struct HarmonyTimingSettings
    {
        float dwellSeconds{30.f};
        float cooldownSeconds{20.f};
        float glideSeconds{10.f};
    };

    /// @brief SetPhaser's payload: the master-bus phaser (8 allpass poles total).
    struct PhaserSettings
    {
        float rateHz{0.3f};
        float depth{0.5f};
        float feedback{0.f};
        float mix{0.5f};
    };

    /// @brief SetChorus's payload: the master-bus stereo chorus.
    struct ChorusSettings
    {
        float rateHz{0.6f};
        float depth{0.5f};
        float mix{0.5f};
    };

    /// @brief SetReverb's payload: the master-bus FDN reverb. mixDb defaults to off.
    struct ReverbSettings
    {
        float sizeMeters{12.f};
        float decayMs{1500.f};
        float dryDb{0.f};
        float mixDb{-100.f};
    };

    /// @brief SetVolumeLfo/SetCutoffLfo/SetMaterialLfo/SetResonanceLfo/SetPitchLfo's payload.
    /// `depth`'s unit depends on the destination (dB, semitones, 0..1, or cents) - see
    /// AmbientPadVoice's own setters.
    struct LfoSettings
    {
        float rateCyclesPerMinute{4.f};
        float depth{0.f};
        float phaseDegrees{0.f};
    };
    using LfoCommands = std::array<std::optional<LfoSettings>, kMaxChannels>;

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- ambientpad script - the stub does nothing on its own; the standalone's own Note/Play\n"
"-- switch (or the Material/Light/Motion/Breath/Stability/Bloom dials) already play a\n"
"-- useful sound with no script at all. See base-scripts/breathing-drone.lua for a script\n"
"-- that plays itself via NoteOn.\n"
"function OnStart()\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kAmbientPadSkeletonHooks =
"-- ambientpad script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"-- Every SetXxx()/NoteOn()/NoteOff() below must be called from inside a hook (OnStart,\n"
"-- a UICreateParameterSet callback, a Timer.After callback, ...), never at this script's\n"
"-- own top level.\n"
"\n"
"-- NoteOn(channel, note, velocity)  channel 1..16 addresses a voice slot directly (no\n"
"--   stealing - only channel 1 is driven by the standalone's own Note/Play controls).\n"
"--   note: MIDI-style note number, 69 = A4 = 440 Hz. velocity: 0..127.\n"
"-- NoteOff(channel, note)\n"
"\n"
"-- SetOscillator(channel, index, { waveform, level, height, cents })  index 0..1\n"
"--   waveform: 0 Saw-Sine-Square, 1 Triangle-Sine-SharkFin, 2 Square-White-Saw - the\n"
"--             material path this layer's Material position sweeps along\n"
"--   level: -1..1. height: semitones offset from the played note. cents: fine tune\n"
"\n"
"-- SetGain(channel, gainInDb)  smoothed per-voice output trim, independent of Level\n"
"\n"
"-- SetPitch(channel, note, cents, glideTimeSeconds)  repitches a channel's held voice.\n"
"--   glideTimeSeconds: 0 repitches instantly (same as NoteOn's own note), otherwise\n"
"--   glides smoothly to note+cents over that many seconds. cents: fine tune -100..100\n"
"\n"
"-- Each channel can carry its own slow LFO (rateCyclesPerMinute typically 1..10) on one of\n"
"-- five destinations - depth 0 (default) leaves that channel exactly as before.\n"
"-- phaseDegrees (0..360) sets where in the cycle it starts; any other value wraps into range:\n"
"-- SetVolumeLfo(channel, rateCyclesPerMinute, depthDb, phaseDegrees)        tremolo, 0..24 dB\n"
"-- SetCutoffLfo(channel, rateCyclesPerMinute, depthSemitones, phaseDegrees) filter sweep, 0..48\n"
"-- SetMaterialLfo(channel, rateCyclesPerMinute, depth, phaseDegrees)       morph sweep, 0..1\n"
"-- SetResonanceLfo(channel, rateCyclesPerMinute, depth, phaseDegrees)      always-up, 0..(no\n"
"--   upper limit - past 1 deliberately drives resonance past self-oscillation)\n"
"-- SetPitchLfo(channel, rateCyclesPerMinute, depthCents, phaseDegrees)     vibrato, 0..100c\n"
"\n"
"-- SetMaterial(value)  0..1, wavetable position along each oscillator's material path\n"
"-- SetMaterialRange(value)  0..1, full width of the OU sweep around Material's center\n"
"-- SetLight(value)     0..1, filter cutoff and character (dark/Velvet .. bright/Glass)\n"
"-- SetMotion(value)    0..1, shared range/speed of the Breath/Material/Lens/Drift wander\n"
"-- SetBreath(value)    0..1, how much the Breath process moves level and cutoff\n"
"-- SetStability(value) 0..1, fragile (0, full drift/detune/wobble) .. firm (1, none)\n"
"-- SetBloom(value)     0..1, amplitude attack/release time\n"
"-- SetHold(hold)       true freezes all four OU processes and every channel's own LFO\n"
"\n"
"-- Fine-tuning how far each OU-driven destination can wander (all default to a modest\n"
"-- depth; Motion/Stability still scale how much of it actually reaches the voice):\n"
"-- SetCutoffRange(semitones)   0..48, Lens's max pull on the filter cutoff\n"
"-- SetResonanceRange(amount)   0..1, Lens's max pull on resonance\n"
"-- SetPitchDriftRange(cents)   0..100, Drift's max per-oscillator detune\n"
"-- SetBreathVcaRange(amount)   0..10, Breath's max VCA gain boost\n"
"\n"
"-- SetDistortion(presetIndex)  0 = off/bypass, 1.. = WaveShaperTables.h preset (1-indexed)\n"
"\n"
"-- SetPhaser({ rateHz, depth, feedback, mix })  master-bus phaser, 8 allpass poles total.\n"
"--   rateHz: 0.01..10. depth: 0..1, how far the sweep spans 200 Hz..2 kHz.\n"
"--   feedback: 0..~1.1. mix: 0 dry..1 fully phased\n"
"\n"
"-- SetChorus({ rateHz, depth, mix })  master-bus stereo chorus - the voice's own stereo\n"
"--   depth comes from here, the oscillator/filter chain itself runs mono.\n"
"--   rateHz: 0.01..8. depth: 0..1. mix: 0 dry..1 fully wet\n"
"\n"
"-- SetReverb({ sizeMeters, decayMs, dryDb, mixDb })  master-bus FDN reverb, order 32.\n"
"--   sizeMeters: 1..60. decayMs: 0..20000. dryDb/mixDb: -100..12 (mixDb defaults to -100,\n"
"--   i.e. off)\n"
"\n"
"-- SetHarmony(enabled)  off by default - with it off, nothing below this line does anything;\n"
"--   every voice stays exactly as driven by Note/Play/NoteOn/NoteOff, as usual.\n"
"-- SetHarmonyHome(note)  retunes the harmonic organism's palette to a new tonal home\n"
"-- SetHarmonyCharacter(region)  soft-biases toward one palette region: 0 no preference,\n"
"--   1 Home, 2 MajorLight, 3 ModalWarmth, 4 OpenSuspended, 5 ChromaticWeather\n"
"-- SetPedalNote(note)  the dedicated bass pedal (always channel 16, always excluded from\n"
"--   the organism's own voice-leading): 0 is off, 1..127 is a literal MIDI note\n"
"-- SetPedalChannels({ channel, ... })  which channels (1..16) are pedal channels - any\n"
"--   subset, including none or all; a newly added one is triggered at the home note\n"
"\n"
"-- ClearHarmonicPalette()  resets the script-authored custom palette built so far; alone\n"
"--   (no AddHarmonicState calls after it) reverts to the 20 built-in states\n"
"-- AddHarmonicState({ semitones = {...}, region })  appends one custom chord - semitones\n"
"--   are literal, home-relative, already spread across registers exactly as wanted (e.g.\n"
"--   { -24, 0, 4, 7, 10 } for a dominant 7th with a low bass added). A fractional value\n"
"--   (e.g. 3.5) keeps its own rounded semitone for scoring/naming/voice-leading, plus a\n"
"--   cents-level fine tune only on the actual sounding pitch. region: 1..5 as\n"
"--   SetHarmonyCharacter above, default 1. Every wish-axis tag is left neutral (0.5)\n"
"-- SetHarmonyTiming({ dwellSeconds, cooldownSeconds, glideSeconds })  overrides how often\n"
"--   the organism reconsiders, its post-transition pause, and the per-voice glide time -\n"
"--   default 30/20/10 (today's fixed pace); each missing field resets to that default too\n"
"-- SetHarmonyRegionBonus(bonus)  the score bonus SetHarmonyCharacter's matching region\n"
"--   gets, default 0.7 - 0 makes Character a no-op without clearing the preference itself\n"
"-- SetHarmonyMaxVoiceJump(semitones)  the NoLargeVoiceJumps vow's threshold, default 7 -\n"
"--   a candidate moving any voice further than this from the current chord is rejected\n"
"--   outright, region preference notwithstanding; raise it if a custom palette's own\n"
"--   chords are too far apart for Character to ever reach some of them\n"
"\n"
"-- Impulse gestures - performance nudges with a life cycle, not an instant hard switch:\n"
"-- Stay()      delay harmonic departure, retain the current voicing relationship\n"
"-- Lean()      move toward a nearby plausible colour\n"
"-- Open()      admit space and ambiguity - open intervals, wider registers\n"
"-- Gather()    become intimate and coherent - fewer notes, closer affinity\n"
"-- Darken()    reduce light and certainty\n"
"-- Brighten()  allow more light\n"
"-- Disturb()   permit a small rupture, temporarily widen permitted friction\n"
"-- Arrive()    seek a meaningful resting place - calmer, more stable\n"
"-- Release()   fade out every other currently active impulse\n"
"\n";
    // clang-format on

    static const std::string kFullSkeletonScript;

    explicit AmbientPadScriptEngine(size_t poolBytes = 512 * 1024);

    struct PendingNoteEventsResult
    {
        std::array<NoteEvent, kMaxNoteEventsPerBlock> events{};
        size_t count{0};
    };
    [[nodiscard]] PendingNoteEventsResult drainNoteEvents() noexcept;

    // voiceIndex is 0-based (channel - 1), matching AmbientPadImpl's voice array.
    [[nodiscard]] std::optional<AmbientOscillatorSettings> drainOscillatorCommand(size_t voiceIndex,
                                                                                  size_t index) noexcept;
    [[nodiscard]] std::optional<float> drainGainCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<AmbientPitchSettings> drainPitchCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainVolumeLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainCutoffLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainMaterialLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainResonanceLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainPitchLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<bool> drainHarmonyEnabledCommand() noexcept;
    [[nodiscard]] std::optional<int> drainHarmonyHomeCommand() noexcept;
    [[nodiscard]] std::optional<int> drainHarmonyCharacterCommand() noexcept;
    [[nodiscard]] std::optional<int> drainPedalNoteCommand() noexcept;
    [[nodiscard]] std::optional<PedalChannelsCommand> drainPedalChannelsCommand() noexcept;
    [[nodiscard]] std::optional<CustomPaletteCommand> drainCustomPaletteCommand() noexcept;
    [[nodiscard]] std::optional<HarmonyTimingSettings> drainHarmonyTimingCommand() noexcept;
    [[nodiscard]] std::optional<float> drainHarmonyRegionBonusCommand() noexcept;
    [[nodiscard]] std::optional<float> drainHarmonyMaxVoiceJumpCommand() noexcept;

    struct PendingImpulseEventsResult
    {
        std::array<AbacDsp::ImpulseKind, kMaxPendingImpulsesPerBlock> events{};
        size_t count{0};
    };
    [[nodiscard]] PendingImpulseEventsResult drainImpulseEvents() noexcept;

    [[nodiscard]] std::optional<float> drainMaterialCommand() noexcept;
    [[nodiscard]] std::optional<float> drainMaterialRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainLightCommand() noexcept;
    [[nodiscard]] std::optional<float> drainMotionCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBreathCommand() noexcept;
    [[nodiscard]] std::optional<float> drainStabilityCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBloomCommand() noexcept;
    [[nodiscard]] std::optional<bool> drainHoldCommand() noexcept;
    [[nodiscard]] std::optional<float> drainCutoffRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainResonanceRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainPitchDriftRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBreathVcaRangeCommand() noexcept;
    [[nodiscard]] std::optional<size_t> drainDistortionCommand() noexcept;
    [[nodiscard]] std::optional<PhaserSettings> drainPhaserCommand() noexcept;
    [[nodiscard]] std::optional<ChorusSettings> drainChorusCommand() noexcept;
    [[nodiscard]] std::optional<ReverbSettings> drainReverbCommand() noexcept;

  private:
    friend class LuaScriptEngineBase<AmbientPadScriptEngine>;
    void bindScriptFunctions();

    void luaNoteOn(size_t channel, int note, int velocity) noexcept;
    void luaNoteOff(size_t channel, int note) noexcept;
    void luaSetOscillator(size_t channel, size_t index, const sol::table& params) noexcept;
    void luaSetGain(size_t channel, float gainDb) noexcept;
    void luaSetPitch(size_t channel, int note, float cents, float glideTimeSeconds) noexcept;
    void luaSetVolumeLfo(size_t channel, float rateCyclesPerMinute, float depthDb, float phaseDegrees) noexcept;
    void luaSetCutoffLfo(size_t channel, float rateCyclesPerMinute, float depthSemitones, float phaseDegrees) noexcept;
    void luaSetMaterialLfo(size_t channel, float rateCyclesPerMinute, float depth, float phaseDegrees) noexcept;
    void luaSetResonanceLfo(size_t channel, float rateCyclesPerMinute, float depth, float phaseDegrees) noexcept;
    void luaSetPitchLfo(size_t channel, float rateCyclesPerMinute, float depthCents, float phaseDegrees) noexcept;
    void luaSetHarmony(bool enabled) noexcept;
    void luaSetHarmonyHome(int note) noexcept;
    void luaSetHarmonyCharacter(int region) noexcept;
    void luaSetPedalNote(int note) noexcept;
    void luaSetPedalChannels(const sol::table& channels) noexcept;
    void luaClearHarmonicPalette() noexcept;
    void luaAddHarmonicState(const sol::table& params) noexcept;
    void luaSetHarmonyTiming(const sol::table& params) noexcept;
    void luaSetHarmonyRegionBonus(float bonus) noexcept;
    void luaSetHarmonyMaxVoiceJump(float semitones) noexcept;
    void pushImpulse(AbacDsp::ImpulseKind kind) noexcept;
    void luaSetMaterial(float value) noexcept;
    void luaSetMaterialRange(float value) noexcept;
    void luaSetLight(float value) noexcept;
    void luaSetMotion(float value) noexcept;
    void luaSetBreath(float value) noexcept;
    void luaSetStability(float value) noexcept;
    void luaSetBloom(float value) noexcept;
    void luaSetHold(bool hold) noexcept;
    void luaSetCutoffRange(float semitones) noexcept;
    void luaSetResonanceRange(float amount) noexcept;
    void luaSetPitchDriftRange(float cents) noexcept;
    void luaSetBreathVcaRange(float amount) noexcept;
    void luaSetDistortion(size_t presetIndex) noexcept;
    void luaSetPhaser(const sol::table& params) noexcept;
    void luaSetChorus(const sol::table& params) noexcept;
    void luaSetReverb(const sol::table& params) noexcept;

    [[nodiscard]] static bool isValidChannel(size_t channel) noexcept;

    std::array<NoteEvent, kMaxNoteEventsPerBlock> m_pendingNoteEvents{};
    size_t m_pendingNoteEventCount{0};

    PerChannelOscillatorCommands m_pendingOscillator{};
    GainCommands m_pendingGain{};
    PitchCommands m_pendingPitch{};
    LfoCommands m_pendingVolumeLfo{};
    LfoCommands m_pendingCutoffLfo{};
    LfoCommands m_pendingMaterialLfo{};
    LfoCommands m_pendingResonanceLfo{};
    LfoCommands m_pendingPitchLfo{};
    std::optional<bool> m_pendingHarmonyEnabled;
    std::optional<int> m_pendingHarmonyHome;
    std::optional<int> m_pendingHarmonyCharacter;
    std::optional<int> m_pendingPedalNote;
    std::optional<PedalChannelsCommand> m_pendingPedalChannels;
    std::array<AbacDsp::HarmonicState, kMaxCustomPaletteEntries> m_customPaletteEntries{};
    size_t m_customPaletteCount{0};
    bool m_customPaletteDirty{false};
    std::optional<HarmonyTimingSettings> m_pendingHarmonyTiming;
    std::optional<float> m_pendingHarmonyRegionBonus;
    std::optional<float> m_pendingHarmonyMaxVoiceJump;
    std::array<AbacDsp::ImpulseKind, kMaxPendingImpulsesPerBlock> m_pendingImpulses{};
    size_t m_pendingImpulseCount{0};
    std::optional<float> m_pendingMaterial;
    std::optional<float> m_pendingMaterialRange;
    std::optional<float> m_pendingLight;
    std::optional<float> m_pendingMotion;
    std::optional<float> m_pendingBreath;
    std::optional<float> m_pendingStability;
    std::optional<float> m_pendingBloom;
    std::optional<bool> m_pendingHold;
    std::optional<float> m_pendingCutoffRange;
    std::optional<float> m_pendingResonanceRange;
    std::optional<float> m_pendingPitchDriftRange;
    std::optional<float> m_pendingBreathVcaRange;
    std::optional<size_t> m_pendingDistortion;
    std::optional<PhaserSettings> m_pendingPhaser;
    std::optional<ChorusSettings> m_pendingChorus;
    std::optional<ReverbSettings> m_pendingReverb;
};

inline const std::string AmbientPadScriptEngine::kFullSkeletonScript =
    std::string(kAmbientPadSkeletonHooks) + std::string(kCommonSkeletonScript);

inline AmbientPadScriptEngine::AmbientPadScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<AmbientPadScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void AmbientPadScriptEngine::bindScriptFunctions()
{
    m_lua.set_function("NoteOn", &AmbientPadScriptEngine::luaNoteOn, this);
    m_lua.set_function("NoteOff", &AmbientPadScriptEngine::luaNoteOff, this);
    m_lua.set_function("SetOscillator", &AmbientPadScriptEngine::luaSetOscillator, this);
    m_lua.set_function("SetGain", &AmbientPadScriptEngine::luaSetGain, this);
    m_lua.set_function("SetPitch", &AmbientPadScriptEngine::luaSetPitch, this);
    m_lua.set_function("SetHarmony", &AmbientPadScriptEngine::luaSetHarmony, this);
    m_lua.set_function("SetHarmonyHome", &AmbientPadScriptEngine::luaSetHarmonyHome, this);
    m_lua.set_function("SetHarmonyCharacter", &AmbientPadScriptEngine::luaSetHarmonyCharacter, this);
    m_lua.set_function("SetPedalNote", &AmbientPadScriptEngine::luaSetPedalNote, this);
    m_lua.set_function("SetPedalChannels", &AmbientPadScriptEngine::luaSetPedalChannels, this);
    m_lua.set_function("ClearHarmonicPalette", &AmbientPadScriptEngine::luaClearHarmonicPalette, this);
    m_lua.set_function("AddHarmonicState", &AmbientPadScriptEngine::luaAddHarmonicState, this);
    m_lua.set_function("SetHarmonyTiming", &AmbientPadScriptEngine::luaSetHarmonyTiming, this);
    m_lua.set_function("SetHarmonyRegionBonus", &AmbientPadScriptEngine::luaSetHarmonyRegionBonus, this);
    m_lua.set_function("SetHarmonyMaxVoiceJump", &AmbientPadScriptEngine::luaSetHarmonyMaxVoiceJump, this);
    m_lua.set_function("Stay", [this]() { pushImpulse(AbacDsp::ImpulseKind::Stay); });
    m_lua.set_function("Lean", [this]() { pushImpulse(AbacDsp::ImpulseKind::Lean); });
    m_lua.set_function("Open", [this]() { pushImpulse(AbacDsp::ImpulseKind::Open); });
    m_lua.set_function("Gather", [this]() { pushImpulse(AbacDsp::ImpulseKind::Gather); });
    m_lua.set_function("Darken", [this]() { pushImpulse(AbacDsp::ImpulseKind::Darken); });
    m_lua.set_function("Brighten", [this]() { pushImpulse(AbacDsp::ImpulseKind::Brighten); });
    m_lua.set_function("Disturb", [this]() { pushImpulse(AbacDsp::ImpulseKind::Disturb); });
    m_lua.set_function("Arrive", [this]() { pushImpulse(AbacDsp::ImpulseKind::Arrive); });
    m_lua.set_function("Release", [this]() { pushImpulse(AbacDsp::ImpulseKind::Release); });
    m_lua.set_function("SetMaterial", &AmbientPadScriptEngine::luaSetMaterial, this);
    m_lua.set_function("SetMaterialRange", &AmbientPadScriptEngine::luaSetMaterialRange, this);
    m_lua.set_function("SetLight", &AmbientPadScriptEngine::luaSetLight, this);
    m_lua.set_function("SetMotion", &AmbientPadScriptEngine::luaSetMotion, this);
    m_lua.set_function("SetBreath", &AmbientPadScriptEngine::luaSetBreath, this);
    m_lua.set_function("SetStability", &AmbientPadScriptEngine::luaSetStability, this);
    m_lua.set_function("SetBloom", &AmbientPadScriptEngine::luaSetBloom, this);
    m_lua.set_function("SetHold", &AmbientPadScriptEngine::luaSetHold, this);
    m_lua.set_function("SetCutoffRange", &AmbientPadScriptEngine::luaSetCutoffRange, this);
    m_lua.set_function("SetResonanceRange", &AmbientPadScriptEngine::luaSetResonanceRange, this);
    m_lua.set_function("SetPitchDriftRange", &AmbientPadScriptEngine::luaSetPitchDriftRange, this);
    m_lua.set_function("SetBreathVcaRange", &AmbientPadScriptEngine::luaSetBreathVcaRange, this);
    m_lua.set_function("SetVolumeLfo", &AmbientPadScriptEngine::luaSetVolumeLfo, this);
    m_lua.set_function("SetCutoffLfo", &AmbientPadScriptEngine::luaSetCutoffLfo, this);
    m_lua.set_function("SetMaterialLfo", &AmbientPadScriptEngine::luaSetMaterialLfo, this);
    m_lua.set_function("SetResonanceLfo", &AmbientPadScriptEngine::luaSetResonanceLfo, this);
    m_lua.set_function("SetPitchLfo", &AmbientPadScriptEngine::luaSetPitchLfo, this);
    m_lua.set_function("SetDistortion", &AmbientPadScriptEngine::luaSetDistortion, this);
    m_lua.set_function("SetPhaser", &AmbientPadScriptEngine::luaSetPhaser, this);
    m_lua.set_function("SetChorus", &AmbientPadScriptEngine::luaSetChorus, this);
    m_lua.set_function("SetReverb", &AmbientPadScriptEngine::luaSetReverb, this);
}

inline bool AmbientPadScriptEngine::isValidChannel(const size_t channel) noexcept
{
    return channel >= 1 && channel <= kMaxChannels;
}

inline void AmbientPadScriptEngine::luaNoteOn(const size_t channel, const int note, const int velocity) noexcept
{
    if (!isValidChannel(channel) || m_pendingNoteEventCount >= kMaxNoteEventsPerBlock)
    {
        return;
    }
    m_pendingNoteEvents[m_pendingNoteEventCount++] = NoteEvent{channel, note, velocity, true};
}

inline void AmbientPadScriptEngine::luaNoteOff(const size_t channel, const int note) noexcept
{
    if (!isValidChannel(channel) || m_pendingNoteEventCount >= kMaxNoteEventsPerBlock)
    {
        return;
    }
    m_pendingNoteEvents[m_pendingNoteEventCount++] = NoteEvent{channel, note, 0, false};
}

inline void AmbientPadScriptEngine::luaSetOscillator(const size_t channel, const size_t index,
                                                     const sol::table& params) noexcept
{
    if (!isValidChannel(channel) || index >= kNumOscillators)
    {
        return;
    }
    const size_t waveform = params.get_or("waveform", size_t{0});
    const float level = params.get_or("level", 0.f);
    const float height = params.get_or("height", 0.f);
    const float cents = params.get_or("cents", 0.f);
    if (waveform >= kNumMaterialPaths || !std::isfinite(level) || !std::isfinite(height) || !std::isfinite(cents))
    {
        return;
    }
    m_pendingOscillator[channel - 1][index] = AmbientOscillatorSettings{
        waveform, std::clamp(level, -1.f, 1.f), std::clamp(height, -48.f, 48.f), std::clamp(cents, -100.f, 100.f)};
}

inline void AmbientPadScriptEngine::luaSetGain(const size_t channel, const float gainDb) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(gainDb))
    {
        return;
    }
    m_pendingGain[channel - 1] = std::clamp(gainDb, -100.f, 12.f);
}

inline void AmbientPadScriptEngine::luaSetPitch(const size_t channel, const int note, const float cents,
                                                const float glideTimeSeconds) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(cents) || !std::isfinite(glideTimeSeconds))
    {
        return;
    }
    m_pendingPitch[channel - 1] =
        AmbientPitchSettings{note, std::clamp(cents, -100.f, 100.f), std::clamp(glideTimeSeconds, 0.f, 60.f)};
}

inline void AmbientPadScriptEngine::luaSetHarmony(const bool enabled) noexcept
{
    m_pendingHarmonyEnabled = enabled;
}

inline void AmbientPadScriptEngine::luaSetHarmonyHome(const int note) noexcept
{
    m_pendingHarmonyHome = note;
}

inline void AmbientPadScriptEngine::luaSetHarmonyCharacter(const int region) noexcept
{
    m_pendingHarmonyCharacter = region;
}

inline void AmbientPadScriptEngine::luaSetPedalNote(const int note) noexcept
{
    m_pendingPedalNote = note;
}

inline void AmbientPadScriptEngine::luaSetPedalChannels(const sol::table& channels) noexcept
{
    PedalChannelsCommand command{};
    const size_t luaCount = channels.size();
    for (size_t i = 1; i <= luaCount && command.count < kMaxChannels; ++i)
    {
        const sol::optional<int> channel = channels[i];
        if (!channel || *channel < 1 || *channel > static_cast<int>(kMaxChannels))
        {
            continue;
        }
        command.channels[command.count++] = *channel;
    }
    m_pendingPedalChannels = command;
}

inline void AmbientPadScriptEngine::luaClearHarmonicPalette() noexcept
{
    m_customPaletteCount = 0;
    m_customPaletteDirty = true;
}

inline void AmbientPadScriptEngine::luaAddHarmonicState(const sol::table& params) noexcept
{
    if (m_customPaletteCount >= kMaxCustomPaletteEntries)
    {
        return;
    }
    const sol::optional<sol::table> semitonesOpt = params["semitones"];
    if (!semitonesOpt)
    {
        return;
    }
    const auto& semitonesTable = *semitonesOpt;
    std::array<float, AbacDsp::Voicing::kMaxNotes> semitones{};
    size_t semitoneCount = 0;
    const size_t luaCount = semitonesTable.size();
    for (size_t i = 1; i <= luaCount && semitoneCount < AbacDsp::Voicing::kMaxNotes; ++i)
    {
        const sol::optional<float> semitone = semitonesTable[i];
        if (!semitone || !std::isfinite(*semitone))
        {
            return;
        }
        semitones[semitoneCount++] = *semitone;
    }
    if (semitoneCount == 0)
    {
        return;
    }
    const int regionIndex = params.get_or("region", 1);
    if (regionIndex < 1 || regionIndex > 5)
    {
        return;
    }
    const auto region = static_cast<AbacDsp::PaletteRegion>(regionIndex - 1);
    m_customPaletteEntries[m_customPaletteCount++] =
        AbacDsp::makeCustomHarmonicState(region, std::span<const float>(semitones.data(), semitoneCount));
    m_customPaletteDirty = true;
}

inline void AmbientPadScriptEngine::luaSetHarmonyTiming(const sol::table& params) noexcept
{
    const float dwellSeconds = params.get_or("dwellSeconds", 30.f);
    const float cooldownSeconds = params.get_or("cooldownSeconds", 20.f);
    const float glideSeconds = params.get_or("glideSeconds", 10.f);
    if (!std::isfinite(dwellSeconds) || !std::isfinite(cooldownSeconds) || !std::isfinite(glideSeconds))
    {
        return;
    }
    m_pendingHarmonyTiming =
        HarmonyTimingSettings{std::clamp(dwellSeconds, 0.1f, 300.f), std::clamp(cooldownSeconds, 0.f, 300.f),
                              std::clamp(glideSeconds, 0.f, 60.f)};
}

inline void AmbientPadScriptEngine::luaSetHarmonyRegionBonus(const float bonus) noexcept
{
    if (std::isfinite(bonus))
    {
        m_pendingHarmonyRegionBonus = std::clamp(bonus, 0.f, 5.f);
    }
}

inline void AmbientPadScriptEngine::luaSetHarmonyMaxVoiceJump(const float semitones) noexcept
{
    if (std::isfinite(semitones))
    {
        m_pendingHarmonyMaxVoiceJump = std::clamp(semitones, 1.f, 48.f);
    }
}

inline void AmbientPadScriptEngine::pushImpulse(const AbacDsp::ImpulseKind kind) noexcept
{
    if (m_pendingImpulseCount >= kMaxPendingImpulsesPerBlock)
    {
        return;
    }
    m_pendingImpulses[m_pendingImpulseCount++] = kind;
}

inline void AmbientPadScriptEngine::luaSetMaterial(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingMaterial = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetMaterialRange(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingMaterialRange = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetLight(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingLight = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetMotion(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingMotion = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetBreath(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingBreath = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetStability(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingStability = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetBloom(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingBloom = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetHold(const bool hold) noexcept
{
    m_pendingHold = hold;
}

inline void AmbientPadScriptEngine::luaSetCutoffRange(const float semitones) noexcept
{
    if (std::isfinite(semitones))
    {
        m_pendingCutoffRange = std::clamp(semitones, 0.f, 48.f);
    }
}

inline void AmbientPadScriptEngine::luaSetResonanceRange(const float amount) noexcept
{
    if (std::isfinite(amount))
    {
        m_pendingResonanceRange = std::clamp(amount, 0.f, 1.f);
    }
}

inline void AmbientPadScriptEngine::luaSetPitchDriftRange(const float cents) noexcept
{
    if (std::isfinite(cents))
    {
        m_pendingPitchDriftRange = std::clamp(cents, 0.f, 100.f);
    }
}

inline void AmbientPadScriptEngine::luaSetBreathVcaRange(const float amount) noexcept
{
    if (std::isfinite(amount))
    {
        m_pendingBreathVcaRange = std::clamp(amount, 0.f, 10.f);
    }
}

inline void AmbientPadScriptEngine::luaSetVolumeLfo(const size_t channel, const float rateCyclesPerMinute,
                                                    const float depthDb, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depthDb) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingVolumeLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::clamp(depthDb, 0.f, 24.f), phaseDegrees};
}

inline void AmbientPadScriptEngine::luaSetCutoffLfo(const size_t channel, const float rateCyclesPerMinute,
                                                    const float depthSemitones, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depthSemitones) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingCutoffLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::clamp(depthSemitones, 0.f, 48.f), phaseDegrees};
}

inline void AmbientPadScriptEngine::luaSetMaterialLfo(const size_t channel, const float rateCyclesPerMinute,
                                                      const float depth, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depth) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingMaterialLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::clamp(depth, 0.f, 1.f), phaseDegrees};
}

inline void AmbientPadScriptEngine::luaSetResonanceLfo(const size_t channel, const float rateCyclesPerMinute,
                                                       const float depth, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depth) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingResonanceLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::max(depth, 0.f), phaseDegrees};
}

inline void AmbientPadScriptEngine::luaSetPitchLfo(const size_t channel, const float rateCyclesPerMinute,
                                                   const float depthCents, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depthCents) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingPitchLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::clamp(depthCents, 0.f, 100.f), phaseDegrees};
}

inline void AmbientPadScriptEngine::luaSetDistortion(const size_t presetIndex) noexcept
{
    m_pendingDistortion = presetIndex;
}

inline void AmbientPadScriptEngine::luaSetPhaser(const sol::table& params) noexcept
{
    const float rateHz = params.get_or("rateHz", 0.3f);
    const float depth = params.get_or("depth", 0.5f);
    const float feedback = params.get_or("feedback", 0.f);
    const float mix = params.get_or("mix", 0.5f);
    if (!std::isfinite(rateHz) || !std::isfinite(depth) || !std::isfinite(feedback) || !std::isfinite(mix))
    {
        return;
    }
    m_pendingPhaser = PhaserSettings{std::clamp(rateHz, 0.01f, 10.f), std::clamp(depth, 0.f, 1.f),
                                     std::clamp(feedback, 0.f, 1.1f), std::clamp(mix, 0.f, 1.f)};
}

inline void AmbientPadScriptEngine::luaSetChorus(const sol::table& params) noexcept
{
    const float rateHz = params.get_or("rateHz", 0.6f);
    const float depth = params.get_or("depth", 0.5f);
    const float mix = params.get_or("mix", 0.5f);
    if (!std::isfinite(rateHz) || !std::isfinite(depth) || !std::isfinite(mix))
    {
        return;
    }
    m_pendingChorus =
        ChorusSettings{std::clamp(rateHz, 0.01f, 8.f), std::clamp(depth, 0.f, 1.f), std::clamp(mix, 0.f, 1.f)};
}

inline void AmbientPadScriptEngine::luaSetReverb(const sol::table& params) noexcept
{
    const float sizeMeters = params.get_or("sizeMeters", 12.f);
    const float decayMs = params.get_or("decayMs", 1500.f);
    const float dryDb = params.get_or("dryDb", 0.f);
    const float mixDb = params.get_or("mixDb", -100.f);
    if (!std::isfinite(sizeMeters) || !std::isfinite(decayMs) || !std::isfinite(dryDb) || !std::isfinite(mixDb))
    {
        return;
    }
    m_pendingReverb = ReverbSettings{std::clamp(sizeMeters, 1.f, 60.f), std::clamp(decayMs, 0.f, 20000.f),
                                     std::clamp(dryDb, -100.f, 12.f), std::clamp(mixDb, -100.f, 12.f)};
}

inline AmbientPadScriptEngine::PendingNoteEventsResult AmbientPadScriptEngine::drainNoteEvents() noexcept
{
    PendingNoteEventsResult result{};
    result.events = m_pendingNoteEvents;
    result.count = m_pendingNoteEventCount;
    m_pendingNoteEventCount = 0;
    return result;
}

inline std::optional<AmbientOscillatorSettings> AmbientPadScriptEngine::drainOscillatorCommand(
    const size_t voiceIndex, const size_t index) noexcept
{
    const auto result = m_pendingOscillator[voiceIndex][index];
    m_pendingOscillator[voiceIndex][index].reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainGainCommand(const size_t voiceIndex) noexcept
{
    const auto result = m_pendingGain[voiceIndex];
    m_pendingGain[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::AmbientPitchSettings> AmbientPadScriptEngine::drainPitchCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingPitch[voiceIndex];
    m_pendingPitch[voiceIndex].reset();
    return result;
}

inline std::optional<bool> AmbientPadScriptEngine::drainHarmonyEnabledCommand() noexcept
{
    const auto result = m_pendingHarmonyEnabled;
    m_pendingHarmonyEnabled.reset();
    return result;
}

inline std::optional<int> AmbientPadScriptEngine::drainHarmonyHomeCommand() noexcept
{
    const auto result = m_pendingHarmonyHome;
    m_pendingHarmonyHome.reset();
    return result;
}

inline std::optional<int> AmbientPadScriptEngine::drainHarmonyCharacterCommand() noexcept
{
    const auto result = m_pendingHarmonyCharacter;
    m_pendingHarmonyCharacter.reset();
    return result;
}

inline std::optional<int> AmbientPadScriptEngine::drainPedalNoteCommand() noexcept
{
    const auto result = m_pendingPedalNote;
    m_pendingPedalNote.reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::PedalChannelsCommand>
AmbientPadScriptEngine::drainPedalChannelsCommand() noexcept
{
    const auto result = m_pendingPedalChannels;
    m_pendingPedalChannels.reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::CustomPaletteCommand>
AmbientPadScriptEngine::drainCustomPaletteCommand() noexcept
{
    if (!m_customPaletteDirty)
    {
        return std::nullopt;
    }
    m_customPaletteDirty = false;
    CustomPaletteCommand command{};
    command.entries = m_customPaletteEntries;
    command.count = m_customPaletteCount;
    return command;
}

inline std::optional<AmbientPadScriptEngine::HarmonyTimingSettings>
AmbientPadScriptEngine::drainHarmonyTimingCommand() noexcept
{
    const auto result = m_pendingHarmonyTiming;
    m_pendingHarmonyTiming.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainHarmonyRegionBonusCommand() noexcept
{
    const auto result = m_pendingHarmonyRegionBonus;
    m_pendingHarmonyRegionBonus.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainHarmonyMaxVoiceJumpCommand() noexcept
{
    const auto result = m_pendingHarmonyMaxVoiceJump;
    m_pendingHarmonyMaxVoiceJump.reset();
    return result;
}

inline AmbientPadScriptEngine::PendingImpulseEventsResult AmbientPadScriptEngine::drainImpulseEvents() noexcept
{
    PendingImpulseEventsResult result{};
    result.events = m_pendingImpulses;
    result.count = m_pendingImpulseCount;
    m_pendingImpulseCount = 0;
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainMaterialCommand() noexcept
{
    const auto result = m_pendingMaterial;
    m_pendingMaterial.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainMaterialRangeCommand() noexcept
{
    const auto result = m_pendingMaterialRange;
    m_pendingMaterialRange.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainLightCommand() noexcept
{
    const auto result = m_pendingLight;
    m_pendingLight.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainMotionCommand() noexcept
{
    const auto result = m_pendingMotion;
    m_pendingMotion.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainBreathCommand() noexcept
{
    const auto result = m_pendingBreath;
    m_pendingBreath.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainStabilityCommand() noexcept
{
    const auto result = m_pendingStability;
    m_pendingStability.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainBloomCommand() noexcept
{
    const auto result = m_pendingBloom;
    m_pendingBloom.reset();
    return result;
}

inline std::optional<bool> AmbientPadScriptEngine::drainHoldCommand() noexcept
{
    const auto result = m_pendingHold;
    m_pendingHold.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainCutoffRangeCommand() noexcept
{
    const auto result = m_pendingCutoffRange;
    m_pendingCutoffRange.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainResonanceRangeCommand() noexcept
{
    const auto result = m_pendingResonanceRange;
    m_pendingResonanceRange.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainPitchDriftRangeCommand() noexcept
{
    const auto result = m_pendingPitchDriftRange;
    m_pendingPitchDriftRange.reset();
    return result;
}

inline std::optional<float> AmbientPadScriptEngine::drainBreathVcaRangeCommand() noexcept
{
    const auto result = m_pendingBreathVcaRange;
    m_pendingBreathVcaRange.reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::LfoSettings> AmbientPadScriptEngine::drainVolumeLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingVolumeLfo[voiceIndex];
    m_pendingVolumeLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::LfoSettings> AmbientPadScriptEngine::drainCutoffLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingCutoffLfo[voiceIndex];
    m_pendingCutoffLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::LfoSettings> AmbientPadScriptEngine::drainMaterialLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingMaterialLfo[voiceIndex];
    m_pendingMaterialLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::LfoSettings> AmbientPadScriptEngine::drainResonanceLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingResonanceLfo[voiceIndex];
    m_pendingResonanceLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::LfoSettings> AmbientPadScriptEngine::drainPitchLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingPitchLfo[voiceIndex];
    m_pendingPitchLfo[voiceIndex].reset();
    return result;
}

inline std::optional<size_t> AmbientPadScriptEngine::drainDistortionCommand() noexcept
{
    const auto result = m_pendingDistortion;
    m_pendingDistortion.reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::PhaserSettings> AmbientPadScriptEngine::drainPhaserCommand() noexcept
{
    const auto result = m_pendingPhaser;
    m_pendingPhaser.reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::ChorusSettings> AmbientPadScriptEngine::drainChorusCommand() noexcept
{
    const auto result = m_pendingChorus;
    m_pendingChorus.reset();
    return result;
}

inline std::optional<AmbientPadScriptEngine::ReverbSettings> AmbientPadScriptEngine::drainReverbCommand() noexcept
{
    const auto result = m_pendingReverb;
    m_pendingReverb.reset();
    return result;
}
