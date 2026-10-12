#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"
#include "DrumPattern.h"

/**
 * Runs a pattern script: its top-level code calls SetName(), SetBeatsPerBar(), ClearPattern() and
 * AddInstrument(), which fill a staging result that the caller publishes after a successful load. Positions are beats from the bar
 * start, levels are dB; the global BarBeats holds the current bar length.
 * The functions only work while loadPattern() runs, never from a handler or timer.
 */
class DrumTrainerScriptEngine : public LuaScriptEngineBase<DrumTrainerScriptEngine>
{
  public:
    struct Result
    {
        DrumPattern::HitPattern pattern;
        std::string name;
    };

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- DrumTrainer pattern script. Leave it empty to use the Preset and Voicing controls; see the\n"
"-- Reset button for the available functions.\n";

    static constexpr std::string_view kSkeletonScript =
"-- DrumTrainer pattern script. Top-level code runs on every Apply and Preset change.\n"
"-- BarBeats is the bar length (beats): the selected Preset's, or the one set by SetBeatsPerBar.\n"
"--\n"
"-- SetName(name)                           names the script; shown in the analysis report and\n"
"--                                         the window title (at most 64 bytes)\n"
"-- SetBeatsPerBar(beats)                   bar length 1 .. 16; replaces the Preset's and\n"
"--                                         silences the Preset/Voicing sound\n"
"-- ClearPattern()                          start the played pattern from scratch\n"
"-- AddInstrument(position, instrument, db) one hit; several hits may share a position\n"
"--\n"
"-- position: beats from the bar start, 0 = beat 1, 0.5 = the 'and' of 1, 1 = beat 2.\n"
"--           Must satisfy 0 <= position < BarBeats.\n"
"-- db:       level relative to the sample (0 = as stored), limited to -96 .. +12.\n"
"-- instrument constants:\n"
"--   Kick Snare Rimshot Sidestick Hihat HihatOpen HihatGhost Wood Clap Shaker Tamb\n"
"--   Tom1 Tom2 Tom3 TomLow Timbale1 Timbale2 TimbaleDamp Ride Crash ClickLow ClickHigh\n"
"--\n"
"-- Calling SetBeatsPerBar, ClearPattern or AddInstrument replaces the Preset/Voicing sound.\n"
"-- Example:\n"
"--\n"
"-- ClearPattern()\n"
"-- for beat = 0, BarBeats - 1 do\n"
"--     AddInstrument(beat, Hihat, -6)\n"
"--     AddInstrument(beat + 0.5, Hihat, -12)\n"
"-- end\n"
"-- AddInstrument(0, Kick, 0)\n"
"-- AddInstrument(2, Kick, -2)\n"
"-- AddInstrument(1, Snare, -1)\n"
"-- AddInstrument(3, Snare, -1)\n";
    // clang-format on

    explicit DrumTrainerScriptEngine(size_t poolBytes = 256 * 1024);

    // Runs the script for a bar of barBeats beats; on failure result() is left empty.
    [[nodiscard]] bool loadPattern(std::string_view source, size_t barBeats);

    [[nodiscard]] const Result& result() const noexcept
    {
        return m_result;
    }

  private:
    friend class LuaScriptEngineBase<DrumTrainerScriptEngine>;
    void bindScriptFunctions() {}

    void bindPatternApi();
    void requireLoading(std::string_view function) const;
    void luaSetName(const std::string& name);
    void luaSetBeatsPerBar(double beats);
    void luaClearPattern();
    void luaAddInstrument(double position, long long instrument, double levelDb);
    [[nodiscard]] float checkedPosition(std::string_view function, double position) const;

    Result m_result{};
    size_t m_barBeats{0};
    std::atomic<bool> m_loading{false};
};

inline DrumTrainerScriptEngine::DrumTrainerScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<DrumTrainerScriptEngine>(poolBytes)
{
    bindPatternApi();
}

inline void DrumTrainerScriptEngine::bindPatternApi()
{
    m_lua.set_function("SetName", [this](const std::string& name) { luaSetName(name); });
    m_lua.set_function("SetBeatsPerBar", [this](const double beats) { luaSetBeatsPerBar(beats); });
    m_lua.set_function("ClearPattern", [this] { luaClearPattern(); });
    m_lua.set_function("AddInstrument", [this](const double position, const long long instrument, const double levelDb)
                       { luaAddInstrument(position, instrument, levelDb); });
    for (size_t i = 0; i < DrumPattern::kInstruments.size(); ++i)
    {
        m_lua[std::string{DrumPattern::kInstruments[i].luaName}] = static_cast<long long>(i);
    }
}

inline bool DrumTrainerScriptEngine::loadPattern(const std::string_view source, const size_t barBeats)
{
    m_result = Result{};
    m_barBeats = barBeats;
    m_lua["BarBeats"] = static_cast<long long>(barBeats);
    m_loading.store(true, std::memory_order_release);
    const bool loaded = loadScript(source);
    m_loading.store(false, std::memory_order_release);
    if (!loaded)
    {
        m_result = Result{};
    }
    return loaded;
}

inline void DrumTrainerScriptEngine::requireLoading(const std::string_view function) const
{
    if (!m_loading.load(std::memory_order_acquire))
    {
        throw std::runtime_error(std::string{function} + " can only be called at the top level of the script");
    }
}

inline float DrumTrainerScriptEngine::checkedPosition(const std::string_view function, const double position) const
{
    if (!std::isfinite(position) || position < 0.0 || position >= static_cast<double>(m_barBeats))
    {
        throw std::runtime_error(std::string{function} + ": position " + std::to_string(position) +
                                 " is outside the bar (0 <= position < " + std::to_string(m_barBeats) + ")");
    }
    return static_cast<float>(position);
}

inline void DrumTrainerScriptEngine::luaSetName(const std::string& name)
{
    requireLoading("SetName");
    const auto isControl = [](const char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; };
    if (name.empty() || name.size() > DrumPattern::kMaxNameBytes || std::ranges::any_of(name, isControl))
    {
        throw std::runtime_error("SetName: the name must be 1 to " + std::to_string(DrumPattern::kMaxNameBytes) +
                                 " bytes without control characters");
    }
    m_result.name = name;
}

inline void DrumTrainerScriptEngine::luaSetBeatsPerBar(const double beats)
{
    using namespace DrumPattern;
    requireLoading("SetBeatsPerBar");
    if (!std::isfinite(beats) || beats != std::floor(beats) || beats < 1.0 ||
        beats > static_cast<double>(kMaxBeatsPerBar))
    {
        throw std::runtime_error("SetBeatsPerBar: beats must be a whole number from 1 to " +
                                 std::to_string(kMaxBeatsPerBar));
    }
    const auto count = static_cast<size_t>(beats);
    const auto& hits = m_result.pattern.hits;
    if (m_result.pattern.hitCount > 0 && hits[m_result.pattern.hitCount - 1].positionBeats >= beats)
    {
        throw std::runtime_error("SetBeatsPerBar: positions already added lie outside the new bar length");
    }
    m_barBeats = count;
    m_lua["BarBeats"] = static_cast<long long>(count);
    m_result.pattern.beatsPerBar = count;
    m_result.pattern.active = true;
}

inline void DrumTrainerScriptEngine::luaClearPattern()
{
    requireLoading("ClearPattern");
    const size_t beatsPerBar = m_result.pattern.beatsPerBar;
    m_result.pattern = DrumPattern::HitPattern{};
    m_result.pattern.beatsPerBar = beatsPerBar;
    m_result.pattern.active = true;
}

inline void DrumTrainerScriptEngine::luaAddInstrument(const double position, const long long instrument,
                                                    const double levelDb)
{
    using namespace DrumPattern;
    requireLoading("AddInstrument");
    const float checkedPos = checkedPosition("AddInstrument", position);
    const auto kind = instrumentFromIndex(instrument);
    if (!kind)
    {
        throw std::runtime_error("AddInstrument: unknown instrument " + std::to_string(instrument));
    }
    if (!std::isfinite(levelDb))
    {
        throw std::runtime_error("AddInstrument: level must be a finite number of dB");
    }
    HitPattern& pattern = m_result.pattern;
    if (pattern.hitCount >= kMaxHits)
    {
        throw std::runtime_error("AddInstrument: a pattern holds at most " + std::to_string(kMaxHits) + " hits");
    }

    const float clampedDb = std::clamp(static_cast<float>(levelDb), kMinLevelDb, kMaxLevelDb);
    const Hit hit{checkedPos, *kind, std::pow(10.f, clampedDb / 20.f)};
    const std::span<Hit> hits{pattern.hits.data(), pattern.hitCount};
    const auto where = std::ranges::upper_bound(hits, checkedPos, {}, &Hit::positionBeats);
    std::ranges::move_backward(where, hits.end(), std::next(hits.end()));
    *where = hit;
    ++pattern.hitCount;
    pattern.active = true;
}
