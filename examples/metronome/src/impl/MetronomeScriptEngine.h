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
#include "MetronomePattern.h"

/**
 * Runs a pattern script: its top-level code calls SetBeatsPerBar(), ClearPattern(),
 * AddInstrument(), ClearAnalysis() and AddAnalysisPosition(), which fill a staging result
 * that the caller publishes after a successful load. Positions are beats from the bar
 * start, levels are dB; the global BarBeats holds the current bar length.
 * The functions only work while loadPattern() runs, never from a handler or timer.
 */
class MetronomeScriptEngine : public LuaScriptEngineBase<MetronomeScriptEngine>
{
  public:
    struct Result
    {
        MetronomePattern::HitPattern pattern;
        MetronomePattern::AnalysisPositions analysis;
    };

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- Metronome pattern script. Leave it empty to use the Preset, Voicing and Analysis Grid\n"
"-- controls; see the Reset button for the available functions.\n";

    static constexpr std::string_view kSkeletonScript =
"-- Metronome pattern script. Top-level code runs on every Apply and Preset change.\n"
"-- BarBeats is the bar length (beats): the selected Preset's, or the one set by SetBeatsPerBar.\n"
"--\n"
"-- SetBeatsPerBar(beats)                   bar length 1 .. 16; replaces the Preset's and\n"
"--                                         silences the Preset/Voicing sound\n"
"-- ClearPattern()                          start the played pattern from scratch\n"
"-- AddInstrument(position, instrument, db) one hit; several hits may share a position\n"
"-- ClearAnalysis()                         start the analysis positions from scratch\n"
"-- AddAnalysisPosition(position)           one position the played onsets are measured against\n"
"--\n"
"-- position: beats from the bar start, 0 = beat 1, 0.5 = the 'and' of 1, 1 = beat 2.\n"
"--           Must satisfy 0 <= position < BarBeats.\n"
"-- db:       level relative to the sample (0 = as stored), limited to -96 .. +12.\n"
"-- instrument constants:\n"
"--   Kick Snare Rimshot Sidestick Hihat HihatOpen HihatGhost Wood Clap Shaker Tamb\n"
"--   Tom1 Tom2 Tom3 TomLow Timbale1 Timbale2 TimbaleDamp Ride Crash ClickLow ClickHigh\n"
"--\n"
"-- Calling SetBeatsPerBar, ClearPattern or AddInstrument replaces the Preset/Voicing sound; calling\n"
"-- ClearAnalysis or AddAnalysisPosition replaces the Analysis Grid. Example:\n"
"--\n"
"-- ClearPattern()\n"
"-- for beat = 0, BarBeats - 1 do\n"
"--     AddInstrument(beat, Hihat, -6)\n"
"--     AddInstrument(beat + 0.5, Hihat, -12)\n"
"-- end\n"
"-- AddInstrument(0, Kick, 0)\n"
"-- AddInstrument(2, Kick, -2)\n"
"-- AddInstrument(1, Snare, -1)\n"
"-- AddInstrument(3, Snare, -1)\n"
"--\n"
"-- ClearAnalysis()\n"
"-- for i = 0, BarBeats * 4 - 1 do AddAnalysisPosition(i / 4) end\n";
    // clang-format on

    explicit MetronomeScriptEngine(size_t poolBytes = 256 * 1024);

    // Runs the script for a bar of barBeats beats; on failure result() is left empty.
    [[nodiscard]] bool loadPattern(std::string_view source, size_t barBeats);

    [[nodiscard]] const Result& result() const noexcept
    {
        return m_result;
    }

  private:
    friend class LuaScriptEngineBase<MetronomeScriptEngine>;
    void bindScriptFunctions() {}

    void bindPatternApi();
    void requireLoading(std::string_view function) const;
    void luaSetBeatsPerBar(double beats);
    void luaClearPattern();
    void luaAddInstrument(double position, long long instrument, double levelDb);
    void luaClearAnalysis();
    void luaAddAnalysisPosition(double position);
    [[nodiscard]] float checkedPosition(std::string_view function, double position) const;

    Result m_result{};
    size_t m_barBeats{0};
    std::atomic<bool> m_loading{false};
};

inline MetronomeScriptEngine::MetronomeScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<MetronomeScriptEngine>(poolBytes)
{
    bindPatternApi();
}

inline void MetronomeScriptEngine::bindPatternApi()
{
    m_lua.set_function("SetBeatsPerBar", [this](const double beats) { luaSetBeatsPerBar(beats); });
    m_lua.set_function("ClearPattern", [this] { luaClearPattern(); });
    m_lua.set_function("AddInstrument", [this](const double position, const long long instrument, const double levelDb)
                       { luaAddInstrument(position, instrument, levelDb); });
    m_lua.set_function("ClearAnalysis", [this] { luaClearAnalysis(); });
    m_lua.set_function("AddAnalysisPosition", [this](const double position) { luaAddAnalysisPosition(position); });
    for (size_t i = 0; i < MetronomePattern::kInstruments.size(); ++i)
    {
        m_lua[std::string{MetronomePattern::kInstruments[i].luaName}] = static_cast<long long>(i);
    }
}

inline bool MetronomeScriptEngine::loadPattern(const std::string_view source, const size_t barBeats)
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

inline void MetronomeScriptEngine::requireLoading(const std::string_view function) const
{
    if (!m_loading.load(std::memory_order_acquire))
    {
        throw std::runtime_error(std::string{function} + " can only be called at the top level of the script");
    }
}

inline float MetronomeScriptEngine::checkedPosition(const std::string_view function, const double position) const
{
    if (!std::isfinite(position) || position < 0.0 || position >= static_cast<double>(m_barBeats))
    {
        throw std::runtime_error(std::string{function} + ": position " + std::to_string(position) +
                                 " is outside the bar (0 <= position < " + std::to_string(m_barBeats) + ")");
    }
    return static_cast<float>(position);
}

inline void MetronomeScriptEngine::luaSetBeatsPerBar(const double beats)
{
    using namespace MetronomePattern;
    requireLoading("SetBeatsPerBar");
    if (!std::isfinite(beats) || beats != std::floor(beats) || beats < 1.0 ||
        beats > static_cast<double>(kMaxBeatsPerBar))
    {
        throw std::runtime_error("SetBeatsPerBar: beats must be a whole number from 1 to " +
                                 std::to_string(kMaxBeatsPerBar));
    }
    const auto count = static_cast<size_t>(beats);
    const auto& hits = m_result.pattern.hits;
    const auto& positions = m_result.analysis.positions;
    const bool hitOutside = m_result.pattern.hitCount > 0 && hits[m_result.pattern.hitCount - 1].positionBeats >= beats;
    const bool analysisOutside = m_result.analysis.count > 0 && positions[m_result.analysis.count - 1] >= beats;
    if (hitOutside || analysisOutside)
    {
        throw std::runtime_error("SetBeatsPerBar: positions already added lie outside the new bar length");
    }
    m_barBeats = count;
    m_lua["BarBeats"] = static_cast<long long>(count);
    m_result.pattern.beatsPerBar = count;
    m_result.pattern.active = true;
}

inline void MetronomeScriptEngine::luaClearPattern()
{
    requireLoading("ClearPattern");
    const size_t beatsPerBar = m_result.pattern.beatsPerBar;
    m_result.pattern = MetronomePattern::HitPattern{};
    m_result.pattern.beatsPerBar = beatsPerBar;
    m_result.pattern.active = true;
}

inline void MetronomeScriptEngine::luaAddInstrument(const double position, const long long instrument,
                                                    const double levelDb)
{
    using namespace MetronomePattern;
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

inline void MetronomeScriptEngine::luaClearAnalysis()
{
    requireLoading("ClearAnalysis");
    m_result.analysis = MetronomePattern::AnalysisPositions{};
    m_result.analysis.active = true;
}

inline void MetronomeScriptEngine::luaAddAnalysisPosition(const double position)
{
    using namespace MetronomePattern;
    requireLoading("AddAnalysisPosition");
    const float checkedPos = checkedPosition("AddAnalysisPosition", position);
    AnalysisPositions& analysis = m_result.analysis;
    analysis.active = true;

    const std::span<float> positions{analysis.positions.data(), analysis.count};
    const auto where = std::ranges::lower_bound(positions, checkedPos);
    if (where != positions.end() && !(checkedPos < *where))
    {
        return;
    }
    if (analysis.count >= kMaxAnalysisPositions)
    {
        throw std::runtime_error("AddAnalysisPosition: at most " + std::to_string(kMaxAnalysisPositions) +
                                 " positions are allowed");
    }
    std::ranges::move_backward(where, positions.end(), std::next(positions.end()));
    *where = checkedPos;
    ++analysis.count;
}
