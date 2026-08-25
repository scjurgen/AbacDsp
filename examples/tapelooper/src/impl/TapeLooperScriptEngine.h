#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"
#include "Filters/PoleMixingFilter.h"

struct TapeLooperFilterCommand
{
    float cutoffHz{20000.f};
    float resonance{0.f};
    // Unset when an invalid mode name was given - previous mode stays in effect.
    std::optional<size_t> modeIndex;
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
"--   SetTrackRecord(track, isRecording)  track is 0-based (A=0, B=1, C=2)\n"
"--   SetTrackPlay(track, isPlaying)\n"
"--   SetTrackGain(track, gain)           linear multiplier, 0 silent, 1 unity\n"
"--   SetGrooveSource(mode)               \"groove\" (loaded MIDI groove) or \"click\"\n"
"--   SetTrackFilter(track, cutoffHz, resonance[, modeName])  modeName defaults to \"LP4\";\n"
"--     resonance 1.0 is the self-oscillation threshold; any AbacDsp::poleMixingList name\n"
"--     works (PoleMixingFilter.h), not just the dial's curated LP4/HP4/BP4/Notch subset\n"
"\n"
"-- Fires whenever a track's applied record state changes (edge-triggered, not polled).\n"
"function OnRecordStateChanged(track, isRecording)\n"
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

    [[nodiscard]] std::optional<float> drainTapeSpeedCommand() noexcept;
    [[nodiscard]] std::optional<float> drainBpmCommand() noexcept;
    [[nodiscard]] std::optional<float> drainGrooveVariationCommand() noexcept;
    [[nodiscard]] std::optional<bool> drainRecordCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<bool> drainPlayCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<float> drainTrackGainCommand(size_t track) noexcept;
    [[nodiscard]] std::optional<TapeLooperFilterCommand> drainTrackFilterCommand(size_t track) noexcept;
    // true = click, false = groove.
    [[nodiscard]] std::optional<bool> drainGrooveSourceCommand() noexcept;

  private:
    friend class LuaScriptEngineBase<TapeLooperScriptEngine>;
    void bindScriptFunctions();

    void luaSetTrackRecord(size_t track, bool value) noexcept;
    void luaSetTrackPlay(size_t track, bool value) noexcept;
    void luaSetTrackGain(size_t track, float value) noexcept;
    void luaSetTrackFilter(size_t track, float cutoffHz, float resonance, sol::optional<std::string> modeName);
    void luaSetTapeSpeed(float value) noexcept;
    void luaSetBpm(float value) noexcept;
    void luaSetGrooveVariation(float value) noexcept;
    void luaSetGrooveSource(const std::string& mode) noexcept;

    sol::protected_function m_onRecordStateChangedFn;
    std::optional<float> m_pendingTapeSpeed;
    std::optional<float> m_pendingBpm;
    std::optional<float> m_pendingGrooveVariation;
    std::array<std::optional<bool>, kTracks> m_pendingRecord{};
    std::array<std::optional<bool>, kTracks> m_pendingPlay{};
    std::array<std::optional<float>, kTracks> m_pendingTrackGain{};
    std::array<std::optional<TapeLooperFilterCommand>, kTracks> m_pendingTrackFilter{};
    std::optional<bool> m_pendingGrooveSource;
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
    m_lua.set_function("SetTrackRecord", &TapeLooperScriptEngine::luaSetTrackRecord, this);
    m_lua.set_function("SetTrackPlay", &TapeLooperScriptEngine::luaSetTrackPlay, this);
    m_lua.set_function("SetTrackGain", &TapeLooperScriptEngine::luaSetTrackGain, this);
    m_lua.set_function("SetTrackFilter", &TapeLooperScriptEngine::luaSetTrackFilter, this);
    m_lua.set_function("SetTapeSpeed", &TapeLooperScriptEngine::luaSetTapeSpeed, this);
    m_lua.set_function("SetBpm", &TapeLooperScriptEngine::luaSetBpm, this);
    m_lua.set_function("SetGrooveVariation", &TapeLooperScriptEngine::luaSetGrooveVariation, this);
    m_lua.set_function("SetGrooveSource", &TapeLooperScriptEngine::luaSetGrooveSource, this);
}

inline void TapeLooperScriptEngine::notifyRecordStateChanged(const size_t track, const bool isRecording) noexcept
{
    callHandler(m_onRecordStateChangedFn, track, isRecording);
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

inline std::optional<bool> TapeLooperScriptEngine::drainGrooveSourceCommand() noexcept
{
    const auto result = m_pendingGrooveSource;
    m_pendingGrooveSource.reset();
    return result;
}
