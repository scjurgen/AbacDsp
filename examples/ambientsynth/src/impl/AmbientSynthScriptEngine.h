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
#include "Harmony/PitchClassSet.h"
#include "Synthesizer/AmbientSynthVoice.h"

/// @brief SetOscillator's payload: one of a channel's 2 oscillators' material path/tuning/level.
struct AmbientOscillatorSettings
{
    size_t waveform{0}; ///< material path: 0 Saw-Sine-Square, 1 Triangle-Sine-SharkFin, 2 Square-White-Saw
    float level{0.f};   ///< -1..1
    float height{0.f};  ///< semitones offset from the played note
    float cents{0.f};   ///< fine tune, cents
};

/**
 * Adds ambientsynth's own scripted entry points on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter machinery. Real host MIDI reaches OnNoteOn/OnNoteOff (see
 * LuaScriptEngineBase); NoteOn/NoteOff below are for a script to trigger a channel directly.
 *
 * `channel` addresses a voice slot directly (1..kMaxChannels, no stealing) rather than picking
 * from a pool. SetOscillator/SetGain/SetPitch/the per-channel LFO setters/NoteOn/NoteOff are
 * per-channel; the musical-intent, effects, and harmony setters describe the one shared patch
 * and broadcast to every voice, mirroring the standalone's own dials.
 *
 * Every setter validates and clamps at this boundary, then stores a one-shot pending command -
 * drained once per block by AmbientSynthImpl, the same pattern AmbientPadScriptEngine uses. A
 * non-finite argument or an out-of-range index/channel silently drops the whole call.
 */
class AmbientSynthScriptEngine : public LuaScriptEngineBase<AmbientSynthScriptEngine>
{
  public:
    static constexpr size_t kMaxChannels{16}; ///< must match AmbientSynthImpl::kMaxVoices
    static constexpr size_t kNumOscillators{2};
    static constexpr size_t kNumMaterialPaths{3};
    static constexpr size_t kNumFilterTypes{7};
    static constexpr size_t kMaxNoteEventsPerBlock{16};
    static constexpr size_t kMaxHarmonyNotes{AbacDsp::Voicing::kMaxNotes};

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

    /// @brief PlayHarmony's payload: one flat, home-relative semitone array (fractional values
    /// keep a cents-level fine tune) - the whole harmony region/set table lives in Lua only.
    struct PlayHarmonyCommand
    {
        std::array<float, kMaxHarmonyNotes> semitones{};
        size_t count{0};
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

    /// @brief The per-channel LFO setters' shared payload. `depth`'s unit depends on the
    /// destination (semitones, 0..1, cents, ...) - see AmbientSynthVoice's own setters.
    struct LfoSettings
    {
        float rateCyclesPerMinute{4.f};
        float depth{0.f};
        float phaseDegrees{0.f};
    };
    using LfoCommands = std::array<std::optional<LfoSettings>, kMaxChannels>;

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- ambientsynth script - the stub does nothing on its own; connect a MIDI keyboard and\n"
"-- play a note (channel 1..16 via NoteOn, or OnNoteOn from real MIDI once base scripts\n"
"-- wire up the region/pedal mapping) to hear it.\n"
"function OnStart()\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kAmbientSynthSkeletonHooks =
"-- ambientsynth script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"-- Every SetXxx()/NoteOn()/NoteOff()/PlayHarmony() below must be called from inside a\n"
"-- hook (OnStart, OnNoteOn, a UICreateParameterSet callback, a Timer.After callback,\n"
"-- ...), never at this script's own top level.\n"
"\n"
"-- NoteOn(channel, note, velocity)  channel 1..16 addresses a voice slot directly (no\n"
"--   stealing). note: MIDI-style note number, 69 = A4 = 440 Hz. velocity: 0..127.\n"
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
"--   glideTimeSeconds: 0 repitches instantly, otherwise glides to note+cents over that\n"
"--   many seconds. cents: fine tune -100..100\n"
"\n"
"-- Each channel can carry its own slow LFO (rateCyclesPerMinute typically 1..10) on one\n"
"-- of six destinations - depth 0 (default) leaves that channel exactly as before.\n"
"-- phaseDegrees (0..360) sets where in the cycle it starts:\n"
"-- SetCutoffLfo(channel, rateCyclesPerMinute, depthSemitones, phaseDegrees)   0..48\n"
"-- SetMaterialLfo(channel, rateCyclesPerMinute, depth, phaseDegrees)         0..1\n"
"-- SetResonanceLfo(channel, rateCyclesPerMinute, depth, phaseDegrees)        always-up,\n"
"--   no upper limit - past 1 deliberately drives resonance past self-oscillation\n"
"-- SetPitchLfo(channel, rateCyclesPerMinute, depthCents, phaseDegrees)       0..100c,\n"
"--   identical on both oscillators (vibrato)\n"
"-- SetBreathLfo(channel, rateCyclesPerMinute, depth, phaseDegrees)           0..10,\n"
"--   bipolar output-gain ripple around unity\n"
"-- SetDriftLfo(channel, rateCyclesPerMinute, depthCents, phaseDegrees)       0..100c,\n"
"--   opposite sign between the two oscillator layers\n"
"\n"
"-- SetMaterial(value)      0..1, wavetable position along each oscillator's material path\n"
"-- SetMaterialRange(value) 0..1, full width of the Material OU sweep around its center\n"
"-- SetCutoff(value)        0..1, base filter cutoff position and filter-character blend\n"
"-- SetResonance(value)     0..1, base filter resonance\n"
"-- SetFilterType(type)     discrete filter response - pass a FilterType.* constant:\n"
"--   FilterType.LP4/.LP2/.LP1Notch/.Notch/.BP2/.HP1LP3/.AP4 (AP4 is a 4-pole allpass -\n"
"--   flat magnitude, only phase moves, meant to be swept via Cutoff for a phasing effect)\n"
"-- SetBloom(value)         0..1, amplitude attack/release time\n"
"\n"
"-- Fine-tuning how far each destination's own OU process can wander from its base value\n"
"-- (all global, broadcast to every channel):\n"
"-- SetCutoffOuRange(semitones)  0..48\n"
"-- SetResonanceRange(amount)    0..1\n"
"-- SetBreathOuRange(amount)     0..10\n"
"-- SetPitchOuRange(cents)       0..100\n"
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
"-- SetHarmonyHome(note)  the register PlayHarmony's home-relative semitones are voiced\n"
"--   around (always the octave at and above MIDI 60, regardless of pitch class)\n"
"-- SetPedalNote(note)  the dedicated bass pedal (always channel 16, always excluded from\n"
"--   PlayHarmony's own voice-leading): 0 is off, 1..127 is a literal MIDI note\n"
"-- SetHarmonyGlideTime(seconds)  how long a glided channel takes to reach a PlayHarmony\n"
"--   target note (0..60, default 10) - a cross-faded channel is unaffected\n"
"-- PlayHarmony({ semitones = {...} })  immediately realizes one chord, voice-led from\n"
"--   whatever is currently playing (glide if close, cross-fade if far) - semitones are\n"
"--   literal, home-relative (e.g. { -12, 0, 4, 7 }). A fractional value (e.g. 3.5) keeps\n"
"--   its own rounded semitone for voice-leading, plus a cents-level fine tune on the\n"
"--   actual sounding pitch\n"
"\n";
    // clang-format on

    static const std::string kFullSkeletonScript;

    explicit AmbientSynthScriptEngine(size_t poolBytes = 512 * 1024);

    struct PendingNoteEventsResult
    {
        std::array<NoteEvent, kMaxNoteEventsPerBlock> events{};
        size_t count{0};
    };
    [[nodiscard]] PendingNoteEventsResult drainNoteEvents() noexcept;

    // voiceIndex is 0-based (channel - 1), matching AmbientSynthImpl's voice array.
    [[nodiscard]] std::optional<AmbientOscillatorSettings> drainOscillatorCommand(size_t voiceIndex,
                                                                                  size_t index) noexcept;
    [[nodiscard]] std::optional<float> drainGainCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<AmbientPitchSettings> drainPitchCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainCutoffLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainMaterialLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainResonanceLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainPitchLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainBreathLfoCommand(size_t voiceIndex) noexcept;
    [[nodiscard]] std::optional<LfoSettings> drainDriftLfoCommand(size_t voiceIndex) noexcept;

    [[nodiscard]] std::optional<float> drainMaterialCommand() noexcept;
    [[nodiscard]] std::optional<float> drainMaterialRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainCutoffCommand() noexcept;
    [[nodiscard]] std::optional<float> drainResonanceCommand() noexcept;
    [[nodiscard]] std::optional<int> drainFilterTypeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBloomCommand() noexcept;
    [[nodiscard]] std::optional<float> drainCutoffOuRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainResonanceRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBreathOuRangeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainPitchOuRangeCommand() noexcept;
    [[nodiscard]] std::optional<size_t> drainDistortionCommand() noexcept;
    [[nodiscard]] std::optional<PhaserSettings> drainPhaserCommand() noexcept;
    [[nodiscard]] std::optional<ChorusSettings> drainChorusCommand() noexcept;
    [[nodiscard]] std::optional<ReverbSettings> drainReverbCommand() noexcept;

    [[nodiscard]] std::optional<int> drainHarmonyHomeCommand() noexcept;
    [[nodiscard]] std::optional<int> drainPedalNoteCommand() noexcept;
    [[nodiscard]] std::optional<float> drainHarmonyGlideTimeCommand() noexcept;
    [[nodiscard]] std::optional<PlayHarmonyCommand> drainPlayHarmonyCommand() noexcept;

  private:
    friend class LuaScriptEngineBase<AmbientSynthScriptEngine>;
    void bindScriptFunctions();
    void registerFilterTypeTable();

    void luaNoteOn(size_t channel, int note, int velocity) noexcept;
    void luaNoteOff(size_t channel, int note) noexcept;
    void luaSetOscillator(size_t channel, size_t index, const sol::table& params) noexcept;
    void luaSetGain(size_t channel, float gainDb) noexcept;
    void luaSetPitch(size_t channel, int note, float cents, float glideTimeSeconds) noexcept;
    void luaSetCutoffLfo(size_t channel, float rateCyclesPerMinute, float depthSemitones, float phaseDegrees) noexcept;
    void luaSetMaterialLfo(size_t channel, float rateCyclesPerMinute, float depth, float phaseDegrees) noexcept;
    void luaSetResonanceLfo(size_t channel, float rateCyclesPerMinute, float depth, float phaseDegrees) noexcept;
    void luaSetPitchLfo(size_t channel, float rateCyclesPerMinute, float depthCents, float phaseDegrees) noexcept;
    void luaSetBreathLfo(size_t channel, float rateCyclesPerMinute, float depth, float phaseDegrees) noexcept;
    void luaSetDriftLfo(size_t channel, float rateCyclesPerMinute, float depthCents, float phaseDegrees) noexcept;
    void luaSetMaterial(float value) noexcept;
    void luaSetMaterialRange(float value) noexcept;
    void luaSetCutoff(float value) noexcept;
    void luaSetResonance(float value) noexcept;
    void luaSetFilterType(int type) noexcept;
    void luaSetBloom(float value) noexcept;
    void luaSetCutoffOuRange(float semitones) noexcept;
    void luaSetResonanceRange(float amount) noexcept;
    void luaSetBreathOuRange(float amount) noexcept;
    void luaSetPitchOuRange(float cents) noexcept;
    void luaSetDistortion(size_t presetIndex) noexcept;
    void luaSetPhaser(const sol::table& params) noexcept;
    void luaSetChorus(const sol::table& params) noexcept;
    void luaSetReverb(const sol::table& params) noexcept;
    void luaSetHarmonyHome(int note) noexcept;
    void luaSetPedalNote(int note) noexcept;
    void luaSetHarmonyGlideTime(float seconds) noexcept;
    void luaPlayHarmony(const sol::table& params) noexcept;

    [[nodiscard]] static bool isValidChannel(size_t channel) noexcept;

    std::array<NoteEvent, kMaxNoteEventsPerBlock> m_pendingNoteEvents{};
    size_t m_pendingNoteEventCount{0};

    PerChannelOscillatorCommands m_pendingOscillator{};
    GainCommands m_pendingGain{};
    PitchCommands m_pendingPitch{};
    LfoCommands m_pendingCutoffLfo{};
    LfoCommands m_pendingMaterialLfo{};
    LfoCommands m_pendingResonanceLfo{};
    LfoCommands m_pendingPitchLfo{};
    LfoCommands m_pendingBreathLfo{};
    LfoCommands m_pendingDriftLfo{};

    std::optional<float> m_pendingMaterial;
    std::optional<float> m_pendingMaterialRange;
    std::optional<float> m_pendingCutoff;
    std::optional<float> m_pendingResonance;
    std::optional<int> m_pendingFilterType;
    std::optional<float> m_pendingBloom;
    std::optional<float> m_pendingCutoffOuRange;
    std::optional<float> m_pendingResonanceRange;
    std::optional<float> m_pendingBreathOuRange;
    std::optional<float> m_pendingPitchOuRange;
    std::optional<size_t> m_pendingDistortion;
    std::optional<PhaserSettings> m_pendingPhaser;
    std::optional<ChorusSettings> m_pendingChorus;
    std::optional<ReverbSettings> m_pendingReverb;

    std::optional<int> m_pendingHarmonyHome;
    std::optional<int> m_pendingPedalNote;
    std::optional<float> m_pendingHarmonyGlideTime;
    std::optional<PlayHarmonyCommand> m_pendingPlayHarmony;
};

inline const std::string AmbientSynthScriptEngine::kFullSkeletonScript =
    std::string(kAmbientSynthSkeletonHooks) + std::string(kCommonSkeletonScript);

inline AmbientSynthScriptEngine::AmbientSynthScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<AmbientSynthScriptEngine>(poolBytes)
{
    registerFilterTypeTable();
    loadScript(kStubScript);
}

inline void AmbientSynthScriptEngine::registerFilterTypeTable()
{
    sol::table filterType = m_lua.create_table();
    filterType["LP4"] = static_cast<int>(AbacDsp::FilterType::LP4);
    filterType["LP2"] = static_cast<int>(AbacDsp::FilterType::LP2);
    filterType["LP1Notch"] = static_cast<int>(AbacDsp::FilterType::LP1Notch);
    filterType["Notch"] = static_cast<int>(AbacDsp::FilterType::Notch);
    filterType["BP2"] = static_cast<int>(AbacDsp::FilterType::BP2);
    filterType["HP1LP3"] = static_cast<int>(AbacDsp::FilterType::HP1LP3);
    filterType["AP4"] = static_cast<int>(AbacDsp::FilterType::AP4);
    m_lua["FilterType"] = filterType;
}

inline void AmbientSynthScriptEngine::bindScriptFunctions()
{
    m_lua.set_function("NoteOn", &AmbientSynthScriptEngine::luaNoteOn, this);
    m_lua.set_function("NoteOff", &AmbientSynthScriptEngine::luaNoteOff, this);
    m_lua.set_function("SetOscillator", &AmbientSynthScriptEngine::luaSetOscillator, this);
    m_lua.set_function("SetGain", &AmbientSynthScriptEngine::luaSetGain, this);
    m_lua.set_function("SetPitch", &AmbientSynthScriptEngine::luaSetPitch, this);
    m_lua.set_function("SetCutoffLfo", &AmbientSynthScriptEngine::luaSetCutoffLfo, this);
    m_lua.set_function("SetMaterialLfo", &AmbientSynthScriptEngine::luaSetMaterialLfo, this);
    m_lua.set_function("SetResonanceLfo", &AmbientSynthScriptEngine::luaSetResonanceLfo, this);
    m_lua.set_function("SetPitchLfo", &AmbientSynthScriptEngine::luaSetPitchLfo, this);
    m_lua.set_function("SetBreathLfo", &AmbientSynthScriptEngine::luaSetBreathLfo, this);
    m_lua.set_function("SetDriftLfo", &AmbientSynthScriptEngine::luaSetDriftLfo, this);
    m_lua.set_function("SetMaterial", &AmbientSynthScriptEngine::luaSetMaterial, this);
    m_lua.set_function("SetMaterialRange", &AmbientSynthScriptEngine::luaSetMaterialRange, this);
    m_lua.set_function("SetCutoff", &AmbientSynthScriptEngine::luaSetCutoff, this);
    m_lua.set_function("SetResonance", &AmbientSynthScriptEngine::luaSetResonance, this);
    m_lua.set_function("SetFilterType", &AmbientSynthScriptEngine::luaSetFilterType, this);
    m_lua.set_function("SetBloom", &AmbientSynthScriptEngine::luaSetBloom, this);
    m_lua.set_function("SetCutoffOuRange", &AmbientSynthScriptEngine::luaSetCutoffOuRange, this);
    m_lua.set_function("SetResonanceRange", &AmbientSynthScriptEngine::luaSetResonanceRange, this);
    m_lua.set_function("SetBreathOuRange", &AmbientSynthScriptEngine::luaSetBreathOuRange, this);
    m_lua.set_function("SetPitchOuRange", &AmbientSynthScriptEngine::luaSetPitchOuRange, this);
    m_lua.set_function("SetDistortion", &AmbientSynthScriptEngine::luaSetDistortion, this);
    m_lua.set_function("SetPhaser", &AmbientSynthScriptEngine::luaSetPhaser, this);
    m_lua.set_function("SetChorus", &AmbientSynthScriptEngine::luaSetChorus, this);
    m_lua.set_function("SetReverb", &AmbientSynthScriptEngine::luaSetReverb, this);
    m_lua.set_function("SetHarmonyHome", &AmbientSynthScriptEngine::luaSetHarmonyHome, this);
    m_lua.set_function("SetPedalNote", &AmbientSynthScriptEngine::luaSetPedalNote, this);
    m_lua.set_function("SetHarmonyGlideTime", &AmbientSynthScriptEngine::luaSetHarmonyGlideTime, this);
    m_lua.set_function("PlayHarmony", &AmbientSynthScriptEngine::luaPlayHarmony, this);
}

inline bool AmbientSynthScriptEngine::isValidChannel(const size_t channel) noexcept
{
    return channel >= 1 && channel <= kMaxChannels;
}

inline void AmbientSynthScriptEngine::luaNoteOn(const size_t channel, const int note, const int velocity) noexcept
{
    if (!isValidChannel(channel) || m_pendingNoteEventCount >= kMaxNoteEventsPerBlock)
    {
        return;
    }
    m_pendingNoteEvents[m_pendingNoteEventCount++] = NoteEvent{channel, note, velocity, true};
}

inline void AmbientSynthScriptEngine::luaNoteOff(const size_t channel, const int note) noexcept
{
    if (!isValidChannel(channel) || m_pendingNoteEventCount >= kMaxNoteEventsPerBlock)
    {
        return;
    }
    m_pendingNoteEvents[m_pendingNoteEventCount++] = NoteEvent{channel, note, 0, false};
}

inline void AmbientSynthScriptEngine::luaSetOscillator(const size_t channel, const size_t index,
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

inline void AmbientSynthScriptEngine::luaSetGain(const size_t channel, const float gainDb) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(gainDb))
    {
        return;
    }
    m_pendingGain[channel - 1] = std::clamp(gainDb, -100.f, 12.f);
}

inline void AmbientSynthScriptEngine::luaSetPitch(const size_t channel, const int note, const float cents,
                                                  const float glideTimeSeconds) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(cents) || !std::isfinite(glideTimeSeconds))
    {
        return;
    }
    m_pendingPitch[channel - 1] =
        AmbientPitchSettings{note, std::clamp(cents, -100.f, 100.f), std::clamp(glideTimeSeconds, 0.f, 60.f)};
}

inline void AmbientSynthScriptEngine::luaSetCutoffLfo(const size_t channel, const float rateCyclesPerMinute,
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

inline void AmbientSynthScriptEngine::luaSetMaterialLfo(const size_t channel, const float rateCyclesPerMinute,
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

inline void AmbientSynthScriptEngine::luaSetResonanceLfo(const size_t channel, const float rateCyclesPerMinute,
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

inline void AmbientSynthScriptEngine::luaSetPitchLfo(const size_t channel, const float rateCyclesPerMinute,
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

inline void AmbientSynthScriptEngine::luaSetBreathLfo(const size_t channel, const float rateCyclesPerMinute,
                                                      const float depth, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depth) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingBreathLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::clamp(depth, 0.f, 10.f), phaseDegrees};
}

inline void AmbientSynthScriptEngine::luaSetDriftLfo(const size_t channel, const float rateCyclesPerMinute,
                                                     const float depthCents, const float phaseDegrees) noexcept
{
    if (!isValidChannel(channel) || !std::isfinite(rateCyclesPerMinute) || !std::isfinite(depthCents) ||
        !std::isfinite(phaseDegrees))
    {
        return;
    }
    m_pendingDriftLfo[channel - 1] =
        LfoSettings{std::clamp(rateCyclesPerMinute, 0.f, 60.f), std::clamp(depthCents, 0.f, 100.f), phaseDegrees};
}

inline void AmbientSynthScriptEngine::luaSetMaterial(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingMaterial = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetMaterialRange(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingMaterialRange = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetCutoff(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingCutoff = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetResonance(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingResonance = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetFilterType(const int type) noexcept
{
    if (type >= 0 && type < static_cast<int>(kNumFilterTypes))
    {
        m_pendingFilterType = type;
    }
}

inline void AmbientSynthScriptEngine::luaSetBloom(const float value) noexcept
{
    if (std::isfinite(value))
    {
        m_pendingBloom = std::clamp(value, 0.f, 1.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetCutoffOuRange(const float semitones) noexcept
{
    if (std::isfinite(semitones))
    {
        m_pendingCutoffOuRange = std::clamp(semitones, 0.f, 48.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetResonanceRange(const float amount) noexcept
{
    if (std::isfinite(amount))
    {
        m_pendingResonanceRange = std::clamp(amount, 0.f, 1.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetBreathOuRange(const float amount) noexcept
{
    if (std::isfinite(amount))
    {
        m_pendingBreathOuRange = std::clamp(amount, 0.f, 10.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetPitchOuRange(const float cents) noexcept
{
    if (std::isfinite(cents))
    {
        m_pendingPitchOuRange = std::clamp(cents, 0.f, 100.f);
    }
}

inline void AmbientSynthScriptEngine::luaSetDistortion(const size_t presetIndex) noexcept
{
    m_pendingDistortion = presetIndex;
}

inline void AmbientSynthScriptEngine::luaSetPhaser(const sol::table& params) noexcept
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

inline void AmbientSynthScriptEngine::luaSetChorus(const sol::table& params) noexcept
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

inline void AmbientSynthScriptEngine::luaSetReverb(const sol::table& params) noexcept
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

inline void AmbientSynthScriptEngine::luaSetHarmonyHome(const int note) noexcept
{
    m_pendingHarmonyHome = note;
}

inline void AmbientSynthScriptEngine::luaSetPedalNote(const int note) noexcept
{
    m_pendingPedalNote = note;
}

inline void AmbientSynthScriptEngine::luaSetHarmonyGlideTime(const float seconds) noexcept
{
    if (std::isfinite(seconds))
    {
        m_pendingHarmonyGlideTime = std::clamp(seconds, 0.f, 60.f);
    }
}

inline void AmbientSynthScriptEngine::luaPlayHarmony(const sol::table& params) noexcept
{
    const sol::optional<sol::table> semitonesOpt = params["semitones"];
    if (!semitonesOpt)
    {
        return;
    }
    const auto& semitonesTable = *semitonesOpt;
    PlayHarmonyCommand command{};
    const size_t luaCount = semitonesTable.size();
    for (size_t i = 1; i <= luaCount && command.count < kMaxHarmonyNotes; ++i)
    {
        const sol::optional<float> semitone = semitonesTable[i];
        if (!semitone || !std::isfinite(*semitone))
        {
            return;
        }
        command.semitones[command.count++] = *semitone;
    }
    if (command.count == 0)
    {
        return;
    }
    m_pendingPlayHarmony = command;
}

inline AmbientSynthScriptEngine::PendingNoteEventsResult AmbientSynthScriptEngine::drainNoteEvents() noexcept
{
    PendingNoteEventsResult result{};
    result.events = m_pendingNoteEvents;
    result.count = m_pendingNoteEventCount;
    m_pendingNoteEventCount = 0;
    return result;
}

inline std::optional<AmbientOscillatorSettings> AmbientSynthScriptEngine::drainOscillatorCommand(
    const size_t voiceIndex, const size_t index) noexcept
{
    const auto result = m_pendingOscillator[voiceIndex][index];
    m_pendingOscillator[voiceIndex][index].reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainGainCommand(const size_t voiceIndex) noexcept
{
    const auto result = m_pendingGain[voiceIndex];
    m_pendingGain[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::AmbientPitchSettings> AmbientSynthScriptEngine::drainPitchCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingPitch[voiceIndex];
    m_pendingPitch[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::LfoSettings> AmbientSynthScriptEngine::drainCutoffLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingCutoffLfo[voiceIndex];
    m_pendingCutoffLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::LfoSettings> AmbientSynthScriptEngine::drainMaterialLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingMaterialLfo[voiceIndex];
    m_pendingMaterialLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::LfoSettings> AmbientSynthScriptEngine::drainResonanceLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingResonanceLfo[voiceIndex];
    m_pendingResonanceLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::LfoSettings> AmbientSynthScriptEngine::drainPitchLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingPitchLfo[voiceIndex];
    m_pendingPitchLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::LfoSettings> AmbientSynthScriptEngine::drainBreathLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingBreathLfo[voiceIndex];
    m_pendingBreathLfo[voiceIndex].reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::LfoSettings> AmbientSynthScriptEngine::drainDriftLfoCommand(
    const size_t voiceIndex) noexcept
{
    const auto result = m_pendingDriftLfo[voiceIndex];
    m_pendingDriftLfo[voiceIndex].reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainMaterialCommand() noexcept
{
    const auto result = m_pendingMaterial;
    m_pendingMaterial.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainMaterialRangeCommand() noexcept
{
    const auto result = m_pendingMaterialRange;
    m_pendingMaterialRange.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainCutoffCommand() noexcept
{
    const auto result = m_pendingCutoff;
    m_pendingCutoff.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainResonanceCommand() noexcept
{
    const auto result = m_pendingResonance;
    m_pendingResonance.reset();
    return result;
}

inline std::optional<int> AmbientSynthScriptEngine::drainFilterTypeCommand() noexcept
{
    const auto result = m_pendingFilterType;
    m_pendingFilterType.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainBloomCommand() noexcept
{
    const auto result = m_pendingBloom;
    m_pendingBloom.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainCutoffOuRangeCommand() noexcept
{
    const auto result = m_pendingCutoffOuRange;
    m_pendingCutoffOuRange.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainResonanceRangeCommand() noexcept
{
    const auto result = m_pendingResonanceRange;
    m_pendingResonanceRange.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainBreathOuRangeCommand() noexcept
{
    const auto result = m_pendingBreathOuRange;
    m_pendingBreathOuRange.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainPitchOuRangeCommand() noexcept
{
    const auto result = m_pendingPitchOuRange;
    m_pendingPitchOuRange.reset();
    return result;
}

inline std::optional<size_t> AmbientSynthScriptEngine::drainDistortionCommand() noexcept
{
    const auto result = m_pendingDistortion;
    m_pendingDistortion.reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::PhaserSettings> AmbientSynthScriptEngine::drainPhaserCommand() noexcept
{
    const auto result = m_pendingPhaser;
    m_pendingPhaser.reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::ChorusSettings> AmbientSynthScriptEngine::drainChorusCommand() noexcept
{
    const auto result = m_pendingChorus;
    m_pendingChorus.reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::ReverbSettings> AmbientSynthScriptEngine::drainReverbCommand() noexcept
{
    const auto result = m_pendingReverb;
    m_pendingReverb.reset();
    return result;
}

inline std::optional<int> AmbientSynthScriptEngine::drainHarmonyHomeCommand() noexcept
{
    const auto result = m_pendingHarmonyHome;
    m_pendingHarmonyHome.reset();
    return result;
}

inline std::optional<int> AmbientSynthScriptEngine::drainPedalNoteCommand() noexcept
{
    const auto result = m_pendingPedalNote;
    m_pendingPedalNote.reset();
    return result;
}

inline std::optional<float> AmbientSynthScriptEngine::drainHarmonyGlideTimeCommand() noexcept
{
    const auto result = m_pendingHarmonyGlideTime;
    m_pendingHarmonyGlideTime.reset();
    return result;
}

inline std::optional<AmbientSynthScriptEngine::PlayHarmonyCommand>
AmbientSynthScriptEngine::drainPlayHarmonyCommand() noexcept
{
    const auto result = m_pendingPlayHarmony;
    m_pendingPlayHarmony.reset();
    return result;
}
