#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"
#include "Filters/PoleMixingFilter.h"
#include "Sampler/GrooveNoteMap.h"

struct TapeLooperFilterCommand
{
    float cutoffHz{20000.f};
    float resonance{0.f};
    // Unset when an invalid mode name was given - previous mode stays in effect.
    std::optional<size_t> modeIndex;
};

struct TapeLooperWowCommand
{
    float depth{0.f};
    float rate{0.f};
    float drift{0.f};
};

struct TapeLooperFlutterCommand
{
    float depth{0.f};
    float rate{0.f};
};

struct TapeLooperChorusCommand
{
    float depth{0.f};
    float rateHz{0.f};
};

struct TapeLooperEchoCommand
{
    size_t divisionIndex{0};
    float feedback{0.f};
};

struct TapeLooperCompressorCommand
{
    float thresholdDb{0.f};
    float ratio{1.f};
    float attackMs{10.f};
    float releaseMs{100.f};
};

struct TapeLooperRingModCommand
{
    float freqHz{0.f};
    float mix{0.f};
};

struct TapeLooperGrooveStyleCommand
{
    std::string styleName;
    unsigned variationIndex{0};
};

struct TapeLooperTremoloCommand
{
    float rateHz{0.f};
    float depth{0.f};
    float drive{0.f};
};

// Order matters: kEffectNodeNames and the duplicate check in parseTrackEffectChain() both
// index by this enum's ordinal value, not just its name.
enum class EffectNodeType
{
    Filter,
    Distortion,
    Chorus,
    Echo,
    Compressor,
    RingMod,
    Tremolo
};

struct EffectNodeName
{
    std::string_view name;
    EffectNodeType type;
};

constexpr auto kEffectNodeNames = std::to_array<EffectNodeName>({
    {"filter", EffectNodeType::Filter},
    {"distortion", EffectNodeType::Distortion},
    {"chorus", EffectNodeType::Chorus},
    {"echo", EffectNodeType::Echo},
    {"compressor", EffectNodeType::Compressor},
    {"ringmod", EffectNodeType::RingMod},
    {"tremolo", EffectNodeType::Tremolo},
});
constexpr size_t kMaxChainNodes = kEffectNodeNames.size();

[[nodiscard]] constexpr std::optional<EffectNodeType> parseEffectNodeType(const std::string_view name) noexcept
{
    for (const auto& entry : kEffectNodeNames)
    {
        if (entry.name == name)
        {
            return entry.type;
        }
    }
    return std::nullopt;
}

// A script-declared processing order for one track's non-reverb effects (see
// TapeLooperImpl.h's stepTrackEffectsChain() family) - reverb send is not reorderable,
// it always taps whichever signal the chain's last node produces.
struct TrackEffectChain
{
    std::array<EffectNodeType, kMaxChainNodes> nodes{};
    size_t length{0};
};

/**
 * Adds tapelooper's own scripted entry points on top of LuaScriptEngineBase's shared
 * MIDI/UI-parameter machinery: transport (tape speed, BPM, groove variation), per-track
 * record/play, and the groove-vs-click source switch that lets a script decide when the
 * groove track plays the loaded MIDI groove versus a tempo-locked click. Every setter here
 * is drained (returned and cleared) once per block by TapeLooperImpl, the same pattern
 * ResonikScriptEngine uses for its aggregate range commands - so a call this block wins
 * over whatever the host automation set for the same parameter, and no call leaves the
 * previous value untouched.
 */
class TapeLooperScriptEngine : public LuaScriptEngineBase<TapeLooperScriptEngine>
{
  public:
    // Kept as its own constant since this file doesn't know about TapeLooperImpl - must
    // match TapeLooperDetail::kFreeTracks.
    static constexpr size_t kTracks{3};
    static constexpr size_t kInstrumentTags{static_cast<size_t>(AbacDsp::GrooveTag::Count)};

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- Click while any track is recording, groove otherwise.\n"
"local recording = {}\n"
"\n"
"function OnRecordStateChanged(track, isRecording)\n"
"    recording[track] = isRecording\n"
"    local anyRecording = recording[0] or recording[1] or recording[2]\n"
"    SetGrooveSource(anyRecording and \"click\" or \"groove\")\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kTapeLooperSkeletonHooks =
"-- Tapelooper script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"\n"
"-- Transport, take effect on the next audio block:\n"
"--   SetTapeSpeed(ratio)                 1.0 is nominal\n"
"--   SetBpm(bpm)                         groove/click tempo\n"
"--   SetGrooveVariation(index)           0-based, picks among the loaded style's variations\n"
"--   SetGrooveStyle(styleName[, variationIndex])  styleName is one of listGrooveNames()'s\n"
"--     own \"<Genre>/<style>\" entries; variationIndex defaults to 0\n"
"--   GetLoopPhase()                      0..1 position within the recorded loop, polled\n"
"--   SetTrackRecord(track, isRecording)  track is 0-based (A=0, B=1, C=2)\n"
"--   SetTrackPlay(track, isPlaying)\n"
"--   SetTrackGain(track, gain)           linear multiplier, 0 silent, 1 unity\n"
"--   SetGrooveSource(mode)               \"groove\" (loaded MIDI groove) or \"click\"\n"
"--   SetTrackFilter(track, cutoffHz, resonance[, modeName])  modeName defaults to \"LP4\";\n"
"--     resonance 1.0 is the self-oscillation threshold; any AbacDsp::poleMixingList name\n"
"--     works (PoleMixingFilter.h), not just the dial's curated LP4/HP4/BP4/Notch subset\n"
"--   SetTrackReverbSend(track, amount)    0 dry, 1 fully sent to the shared reverb bus\n"
"--   SetReverbSize(meters)                shared by every track's send\n"
"--   SetReverbDecay(ms)                   shared by every track's send\n"
"--   SetTrackWow(track, depth, rate, drift)      tape speed-drift character\n"
"--   SetTrackFlutter(track, depth, rate)         fast speed-irregularity character\n"
"--   SetTrackDrive(track, amount)                0 clean, 1 hysteresis-distorted\n"
"--   SetTrackChorus(track, depth, rateHz)         depth also sets the wet/dry mix\n"
"--   SetTrackEcho(track, divisionIndex, feedback) 0-based sync division; feedback also\n"
"--     sets the send level (0 = no echo at all, not just no repeats)\n"
"--   SetTrackCompressor(track, thresholdDb, ratio, attackMs, releaseMs)  ratio 1 = off\n"
"--   SetTrackRingMod(track, freqHz, mix)\n"
"--   SetTrackTremolo(track, rateHz, depth, drive)  drive squares the LFO toward a gate\n"
"--   SetTrackChain(track, {\"filter\", \"distortion\", ...})  reorders that track's own\n"
"--     effects (reverb send excluded, always last); an unknown or repeated name rejects\n"
"--     the whole call and leaves the previous chain in effect\n"
"--   SetInstrumentGain(name, gain)         groove-kit instrument, e.g. \"kick\", \"snare\",\n"
"--     \"hihat_closed\" (see Sampler/GrooveNoteMap.h's kGrooveTagNames); unknown name is a\n"
"--     no-op, as is a name the loaded kit has no piece for\n"
"--   MuteInstrument(name)                  sugar for SetInstrumentGain(name, 0)\n"
"--   SetInstrumentReverbSend(name, amount) sent to the groove track's own reverb bus\n"
"\n"
"-- Fires whenever a track's applied record state changes (edge-triggered, not polled).\n"
"function OnRecordStateChanged(track, isRecording)\n"
"end\n"
"\n"
"-- Fires once each time the recorded loop wraps back to its start (edge-triggered, not\n"
"-- polled) - a no-op until a loop length exists.\n"
"function OnLoopEnd()\n"
"end\n"
"\n";
    // clang-format on

    // Shown by the popup editor's Reset button: kTapeLooperSkeletonHooks above followed by
    // LuaScriptEngineBase::kCommonSkeletonScript, as opposed to kStubScript (deliberately
    // minimal - what a fresh patch actually plays out of the box).
    static const std::string kFullSkeletonScript;

    explicit TapeLooperScriptEngine(size_t poolBytes = 512 * 1024);

    // Notifies the script that track's applied record flag changed this block; called by
    // TapeLooperImpl only on an actual edge, never every block.
    void notifyRecordStateChanged(size_t track, bool isRecording) noexcept;

    // Fires OnLoopEnd(); called by TapeLooperImpl only on the block the loop wraps, never
    // every block.
    void notifyLoopEnd() noexcept;

    // Updates GetLoopPhase()'s live value; called by TapeLooperImpl once every block.
    void updateLoopPhase(float normalized) noexcept;

    [[nodiscard]] std::optional<float> drainTapeSpeedCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBpmCommand() noexcept;
    [[nodiscard]] std::optional<float> drainGrooveVariationCommand() noexcept;
    [[nodiscard]] std::optional<TapeLooperGrooveStyleCommand> drainGrooveStyleCommand() noexcept;
    [[nodiscard]] std::optional<bool> drainRecordCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<bool> drainPlayCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<float> drainTrackGainCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperFilterCommand> drainTrackFilterCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<float> drainTrackReverbSendCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<float> drainReverbSizeCommand() noexcept;
    [[nodiscard]] std::optional<float> drainReverbDecayCommand() noexcept;
    // true = click, false = groove.
    [[nodiscard]] std::optional<bool> drainGrooveSourceCommand() noexcept;
    [[nodiscard]] std::optional<TapeLooperWowCommand> drainTrackWowCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperFlutterCommand> drainTrackFlutterCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<float> drainTrackDriveCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperChorusCommand> drainTrackChorusCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperEchoCommand> drainTrackEchoCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperCompressorCommand> drainTrackCompressorCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperRingModCommand> drainTrackRingModCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperTremoloCommand> drainTrackTremoloCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TrackEffectChain> drainTrackChainCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<float> drainInstrumentGainCommand(size_t tagIndex) noexcept;
    [[nodiscard]] std::optional<float> drainInstrumentReverbSendCommand(size_t tagIndex) noexcept;

  private:
    friend class LuaScriptEngineBase<TapeLooperScriptEngine>;
    void bindScriptFunctions();

    void luaSetTrackRecord(size_t track, bool value) noexcept;
    void luaSetTrackPlay(size_t track, bool value) noexcept;
    void luaSetTrackGain(size_t track, float value) noexcept;
    void luaSetTrackFilter(size_t track, float cutoffHz, float resonance, sol::optional<std::string> modeName);
    void luaSetTrackReverbSend(size_t track, float value) noexcept;
    void luaSetReverbSize(float value) noexcept;
    void luaSetReverbDecay(float value) noexcept;
    void luaSetTapeSpeed(float value) noexcept;
    void luaSetBpm(float value) noexcept;
    void luaSetGrooveVariation(float value) noexcept;
    void luaSetGrooveStyle(const std::string& styleName, sol::optional<int> variationIndex) noexcept;
    void luaSetGrooveSource(const std::string& mode) noexcept;
    void luaSetTrackWow(size_t track, float depth, float rate, float drift) noexcept;
    void luaSetTrackFlutter(size_t track, float depth, float rate) noexcept;
    void luaSetTrackDrive(size_t track, float value) noexcept;
    void luaSetTrackChorus(size_t track, float depth, float rateHz) noexcept;
    void luaSetTrackEcho(size_t track, size_t divisionIndex, float feedback) noexcept;
    void luaSetTrackCompressor(size_t track, float thresholdDb, float ratio, float attackMs, float releaseMs) noexcept;
    void luaSetTrackRingMod(size_t track, float freqHz, float mix) noexcept;
    void luaSetTrackTremolo(size_t track, float rateHz, float depth, float drive) noexcept;
    void luaSetTrackChain(size_t track, const sol::table& nodeNames);
    [[nodiscard]] static std::optional<TrackEffectChain> parseTrackEffectChain(const sol::table& nodeNames);
    void luaSetInstrumentGain(const std::string& name, float value) noexcept;
    void luaMuteInstrument(const std::string& name) noexcept;
    void luaSetInstrumentReverbSend(const std::string& name, float value) noexcept;

    sol::protected_function m_onRecordStateChangedFn;
    sol::protected_function m_onLoopEndFn;
    float m_loopPhase{0.f};
    std::optional<float> m_pendingTapeSpeed;
    std::optional<float> m_pendingBpm;
    std::optional<float> m_pendingGrooveVariation;
    std::optional<TapeLooperGrooveStyleCommand> m_pendingGrooveStyle;
    std::array<std::optional<bool>, kTracks> m_pendingRecord{};
    std::array<std::optional<bool>, kTracks> m_pendingPlay{};
    std::array<std::optional<float>, kTracks> m_pendingTrackGain{};
    std::array<std::optional<TapeLooperFilterCommand>, kTracks> m_pendingTrackFilter{};
    std::array<std::optional<float>, kTracks> m_pendingTrackReverbSend{};
    std::optional<float> m_pendingReverbSize;
    std::optional<float> m_pendingReverbDecay;
    std::optional<bool> m_pendingGrooveSource;
    std::array<std::optional<TapeLooperWowCommand>, kTracks> m_pendingTrackWow{};
    std::array<std::optional<TapeLooperFlutterCommand>, kTracks> m_pendingTrackFlutter{};
    std::array<std::optional<float>, kTracks> m_pendingTrackDrive{};
    std::array<std::optional<TapeLooperChorusCommand>, kTracks> m_pendingTrackChorus{};
    std::array<std::optional<TapeLooperEchoCommand>, kTracks> m_pendingTrackEcho{};
    std::array<std::optional<TapeLooperCompressorCommand>, kTracks> m_pendingTrackCompressor{};
    std::array<std::optional<TapeLooperRingModCommand>, kTracks> m_pendingTrackRingMod{};
    std::array<std::optional<TapeLooperTremoloCommand>, kTracks> m_pendingTrackTremolo{};
    std::array<std::optional<TrackEffectChain>, kTracks> m_pendingTrackChain{};
    std::array<std::optional<float>, kInstrumentTags> m_pendingInstrumentGain{};
    std::array<std::optional<float>, kInstrumentTags> m_pendingInstrumentReverbSend{};
};

inline const std::string TapeLooperScriptEngine::kFullSkeletonScript =
    std::string(kTapeLooperSkeletonHooks) + std::string(kCommonSkeletonScript);

inline TapeLooperScriptEngine::TapeLooperScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<TapeLooperScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void TapeLooperScriptEngine::bindScriptFunctions()
{
    m_onRecordStateChangedFn = m_lua["OnRecordStateChanged"];
    m_onLoopEndFn = m_lua["OnLoopEnd"];
    m_lua.set_function("GetLoopPhase", [this]() noexcept { return m_loopPhase; });
    m_lua.set_function("SetTrackRecord", &TapeLooperScriptEngine::luaSetTrackRecord, this);
    m_lua.set_function("SetTrackPlay", &TapeLooperScriptEngine::luaSetTrackPlay, this);
    m_lua.set_function("SetTrackGain", &TapeLooperScriptEngine::luaSetTrackGain, this);
    m_lua.set_function("SetTrackFilter", &TapeLooperScriptEngine::luaSetTrackFilter, this);
    m_lua.set_function("SetTrackReverbSend", &TapeLooperScriptEngine::luaSetTrackReverbSend, this);
    m_lua.set_function("SetReverbSize", &TapeLooperScriptEngine::luaSetReverbSize, this);
    m_lua.set_function("SetReverbDecay", &TapeLooperScriptEngine::luaSetReverbDecay, this);
    m_lua.set_function("SetTapeSpeed", &TapeLooperScriptEngine::luaSetTapeSpeed, this);
    m_lua.set_function("SetBpm", &TapeLooperScriptEngine::luaSetBpm, this);
    m_lua.set_function("SetGrooveVariation", &TapeLooperScriptEngine::luaSetGrooveVariation, this);
    m_lua.set_function("SetGrooveStyle", &TapeLooperScriptEngine::luaSetGrooveStyle, this);
    m_lua.set_function("SetGrooveSource", &TapeLooperScriptEngine::luaSetGrooveSource, this);
    m_lua.set_function("SetTrackWow", &TapeLooperScriptEngine::luaSetTrackWow, this);
    m_lua.set_function("SetTrackFlutter", &TapeLooperScriptEngine::luaSetTrackFlutter, this);
    m_lua.set_function("SetTrackDrive", &TapeLooperScriptEngine::luaSetTrackDrive, this);
    m_lua.set_function("SetTrackChorus", &TapeLooperScriptEngine::luaSetTrackChorus, this);
    m_lua.set_function("SetTrackEcho", &TapeLooperScriptEngine::luaSetTrackEcho, this);
    m_lua.set_function("SetTrackCompressor", &TapeLooperScriptEngine::luaSetTrackCompressor, this);
    m_lua.set_function("SetTrackRingMod", &TapeLooperScriptEngine::luaSetTrackRingMod, this);
    m_lua.set_function("SetTrackTremolo", &TapeLooperScriptEngine::luaSetTrackTremolo, this);
    m_lua.set_function("SetTrackChain", &TapeLooperScriptEngine::luaSetTrackChain, this);
    m_lua.set_function("SetInstrumentGain", &TapeLooperScriptEngine::luaSetInstrumentGain, this);
    m_lua.set_function("MuteInstrument", &TapeLooperScriptEngine::luaMuteInstrument, this);
    m_lua.set_function("SetInstrumentReverbSend", &TapeLooperScriptEngine::luaSetInstrumentReverbSend, this);
}

inline void TapeLooperScriptEngine::notifyRecordStateChanged(const size_t track, const bool isRecording) noexcept
{
    callHandler(m_onRecordStateChangedFn, track, isRecording);
}

inline void TapeLooperScriptEngine::notifyLoopEnd() noexcept
{
    callHandler(m_onLoopEndFn);
}

inline void TapeLooperScriptEngine::updateLoopPhase(const float normalized) noexcept
{
    m_loopPhase = normalized;
}

inline void TapeLooperScriptEngine::luaSetTrackRecord(const size_t track, const bool value) noexcept
{
    if (track < kTracks)
    {
        m_pendingRecord[track] = value;
    }
}

inline void TapeLooperScriptEngine::luaSetTrackPlay(const size_t track, const bool value) noexcept
{
    if (track < kTracks)
    {
        m_pendingPlay[track] = value;
    }
}

inline void TapeLooperScriptEngine::luaSetTrackGain(const size_t track, const float value) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackGain[track] = value;
    }
}

inline void TapeLooperScriptEngine::luaSetTrackFilter(const size_t track, const float cutoffHz, const float resonance,
                                                      const sol::optional<std::string> modeName)
{
    if (track >= kTracks)
    {
        return;
    }
    TapeLooperFilterCommand command{cutoffHz, resonance, std::nullopt};
    try
    {
        command.modeIndex = AbacDsp::findFilterIndex(modeName.value_or("LP4"));
    }
    catch (const std::out_of_range&)
    {
        // Unknown mode name: cutoff/resonance still apply, mode stays whatever it was.
    }
    m_pendingTrackFilter[track] = command;
}

inline void TapeLooperScriptEngine::luaSetTrackReverbSend(const size_t track, const float value) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackReverbSend[track] = value;
    }
}

inline void TapeLooperScriptEngine::luaSetReverbSize(const float value) noexcept
{
    m_pendingReverbSize = value;
}

inline void TapeLooperScriptEngine::luaSetReverbDecay(const float value) noexcept
{
    m_pendingReverbDecay = value;
}

inline void TapeLooperScriptEngine::luaSetTapeSpeed(const float value) noexcept
{
    m_pendingTapeSpeed = value;
}

inline void TapeLooperScriptEngine::luaSetBpm(const float value) noexcept
{
    m_pendingBpm = value;
}

inline void TapeLooperScriptEngine::luaSetGrooveVariation(const float value) noexcept
{
    m_pendingGrooveVariation = value;
}

inline void TapeLooperScriptEngine::luaSetGrooveStyle(const std::string& styleName,
                                                      const sol::optional<int> variationIndex) noexcept
{
    if (styleName.empty())
    {
        return;
    }
    m_pendingGrooveStyle =
        TapeLooperGrooveStyleCommand{styleName, static_cast<unsigned>(std::max(0, variationIndex.value_or(0)))};
}

inline void TapeLooperScriptEngine::luaSetGrooveSource(const std::string& mode) noexcept
{
    if (mode == "click")
    {
        m_pendingGrooveSource = true;
    }
    else if (mode == "groove")
    {
        m_pendingGrooveSource = false;
    }
    // Anything else is ignored - previous source stays in effect.
}

inline void TapeLooperScriptEngine::luaSetTrackWow(const size_t track, const float depth, const float rate,
                                                   const float drift) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackWow[track] = TapeLooperWowCommand{depth, rate, drift};
    }
}

inline void TapeLooperScriptEngine::luaSetTrackFlutter(const size_t track, const float depth, const float rate) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackFlutter[track] = TapeLooperFlutterCommand{depth, rate};
    }
}

inline void TapeLooperScriptEngine::luaSetTrackDrive(const size_t track, const float value) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackDrive[track] = value;
    }
}

inline void TapeLooperScriptEngine::luaSetTrackChorus(const size_t track, const float depth,
                                                      const float rateHz) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackChorus[track] = TapeLooperChorusCommand{depth, rateHz};
    }
}

inline void TapeLooperScriptEngine::luaSetTrackEcho(const size_t track, const size_t divisionIndex,
                                                    const float feedback) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackEcho[track] = TapeLooperEchoCommand{divisionIndex, feedback};
    }
}

inline void TapeLooperScriptEngine::luaSetTrackCompressor(const size_t track, const float thresholdDb,
                                                          const float ratio, const float attackMs,
                                                          const float releaseMs) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackCompressor[track] = TapeLooperCompressorCommand{thresholdDb, ratio, attackMs, releaseMs};
    }
}

inline void TapeLooperScriptEngine::luaSetTrackRingMod(const size_t track, const float freqHz, const float mix) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackRingMod[track] = TapeLooperRingModCommand{freqHz, mix};
    }
}

inline std::optional<TrackEffectChain> TapeLooperScriptEngine::parseTrackEffectChain(const sol::table& nodeNames)
{
    TrackEffectChain chain{};
    std::array<bool, kMaxChainNodes> seen{};
    for (size_t i = 1; i <= kMaxChainNodes; ++i)
    {
        const sol::optional<std::string> name = nodeNames[i];
        if (!name)
        {
            break;
        }
        const auto nodeType = parseEffectNodeType(*name);
        if (!nodeType || seen[static_cast<size_t>(*nodeType)])
        {
            return std::nullopt;
        }
        seen[static_cast<size_t>(*nodeType)] = true;
        chain.nodes[chain.length++] = *nodeType;
    }
    return chain;
}

inline void TapeLooperScriptEngine::luaSetTrackChain(const size_t track, const sol::table& nodeNames)
{
    if (track >= kTracks)
    {
        return;
    }
    if (const auto chain = parseTrackEffectChain(nodeNames))
    {
        m_pendingTrackChain[track] = *chain;
    }
}

inline void TapeLooperScriptEngine::luaSetTrackTremolo(const size_t track, const float rateHz, const float depth,
                                                       const float drive) noexcept
{
    if (track < kTracks)
    {
        m_pendingTrackTremolo[track] = TapeLooperTremoloCommand{rateHz, depth, drive};
    }
}

inline void TapeLooperScriptEngine::luaSetInstrumentGain(const std::string& name, const float value) noexcept
{
    if (const auto tag = AbacDsp::tagFromName(name); tag != AbacDsp::GrooveTag::None)
    {
        m_pendingInstrumentGain[static_cast<size_t>(tag)] = value;
    }
}

inline void TapeLooperScriptEngine::luaMuteInstrument(const std::string& name) noexcept
{
    luaSetInstrumentGain(name, 0.f);
}

inline void TapeLooperScriptEngine::luaSetInstrumentReverbSend(const std::string& name, const float value) noexcept
{
    if (const auto tag = AbacDsp::tagFromName(name); tag != AbacDsp::GrooveTag::None)
    {
        m_pendingInstrumentReverbSend[static_cast<size_t>(tag)] = value;
    }
}

inline std::optional<float> TapeLooperScriptEngine::drainTapeSpeedCommand() noexcept
{
    const auto result = m_pendingTapeSpeed;
    m_pendingTapeSpeed.reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainBpmCommand() noexcept
{
    const auto result = m_pendingBpm;
    m_pendingBpm.reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainGrooveVariationCommand() noexcept
{
    const auto result = m_pendingGrooveVariation;
    m_pendingGrooveVariation.reset();
    return result;
}

inline std::optional<TapeLooperGrooveStyleCommand> TapeLooperScriptEngine::drainGrooveStyleCommand() noexcept
{
    auto result = std::move(m_pendingGrooveStyle);
    m_pendingGrooveStyle.reset();
    return result;
}

inline std::optional<bool> TapeLooperScriptEngine::drainRecordCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingRecord[track];
    m_pendingRecord[track].reset();
    return result;
}

inline std::optional<bool> TapeLooperScriptEngine::drainPlayCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingPlay[track];
    m_pendingPlay[track].reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainTrackGainCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackGain[track];
    m_pendingTrackGain[track].reset();
    return result;
}

inline std::optional<TapeLooperFilterCommand> TapeLooperScriptEngine::drainTrackFilterCommand(
    const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackFilter[track];
    m_pendingTrackFilter[track].reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainTrackReverbSendCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackReverbSend[track];
    m_pendingTrackReverbSend[track].reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainReverbSizeCommand() noexcept
{
    const auto result = m_pendingReverbSize;
    m_pendingReverbSize.reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainReverbDecayCommand() noexcept
{
    const auto result = m_pendingReverbDecay;
    m_pendingReverbDecay.reset();
    return result;
}

inline std::optional<bool> TapeLooperScriptEngine::drainGrooveSourceCommand() noexcept
{
    const auto result = m_pendingGrooveSource;
    m_pendingGrooveSource.reset();
    return result;
}

inline std::optional<TapeLooperWowCommand> TapeLooperScriptEngine::drainTrackWowCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackWow[track];
    m_pendingTrackWow[track].reset();
    return result;
}

inline std::optional<TapeLooperFlutterCommand> TapeLooperScriptEngine::drainTrackFlutterCommand(
    const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackFlutter[track];
    m_pendingTrackFlutter[track].reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainTrackDriveCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackDrive[track];
    m_pendingTrackDrive[track].reset();
    return result;
}

inline std::optional<TapeLooperChorusCommand> TapeLooperScriptEngine::drainTrackChorusCommand(
    const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackChorus[track];
    m_pendingTrackChorus[track].reset();
    return result;
}

inline std::optional<TapeLooperEchoCommand> TapeLooperScriptEngine::drainTrackEchoCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackEcho[track];
    m_pendingTrackEcho[track].reset();
    return result;
}

inline std::optional<TapeLooperCompressorCommand> TapeLooperScriptEngine::drainTrackCompressorCommand(
    const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackCompressor[track];
    m_pendingTrackCompressor[track].reset();
    return result;
}

inline std::optional<TapeLooperRingModCommand> TapeLooperScriptEngine::drainTrackRingModCommand(
    const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackRingMod[track];
    m_pendingTrackRingMod[track].reset();
    return result;
}

inline std::optional<TapeLooperTremoloCommand> TapeLooperScriptEngine::drainTrackTremoloCommand(
    const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackTremolo[track];
    m_pendingTrackTremolo[track].reset();
    return result;
}

inline std::optional<TrackEffectChain> TapeLooperScriptEngine::drainTrackChainCommand(const size_t track) noexcept
{
    if (track >= kTracks)
    {
        return std::nullopt;
    }
    const auto result = m_pendingTrackChain[track];
    m_pendingTrackChain[track].reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainInstrumentGainCommand(const size_t tagIndex) noexcept
{
    if (tagIndex >= kInstrumentTags)
    {
        return std::nullopt;
    }
    const auto result = m_pendingInstrumentGain[tagIndex];
    m_pendingInstrumentGain[tagIndex].reset();
    return result;
}

inline std::optional<float> TapeLooperScriptEngine::drainInstrumentReverbSendCommand(const size_t tagIndex) noexcept
{
    if (tagIndex >= kInstrumentTags)
    {
        return std::nullopt;
    }
    const auto result = m_pendingInstrumentReverbSend[tagIndex];
    m_pendingInstrumentReverbSend[tagIndex].reset();
    return result;
}
