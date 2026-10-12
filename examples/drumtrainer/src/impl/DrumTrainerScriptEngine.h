#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"
#include "DrumPattern.h"

/**
 * Runs a pattern script: its top-level code calls SetName(), SetBeatsPerBar(), SetPatternBars(),
 * ClearPattern() and AddHit(), which fill a staging result that the caller publishes after a
 * successful load. The pattern is what the drummer is expected to play; positions are beats from
 * the pattern start. The globals BarBeats and PatternBars hold the current bar length and pattern
 * length. The functions only work while loadPattern() runs, never from a handler or timer.
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
"-- DrumTrainer pattern script: defines the pattern to practise. See the Reset button for the\n"
"-- available functions.\n";

    static constexpr std::string_view kSkeletonScript =
"-- DrumTrainer pattern script. Top-level code runs on every Apply and Preset change and defines\n"
"-- what you are expected to play; the click itself follows the Preset.\n"
"-- BarBeats is the bar length (beats): the selected Preset's, or the one set by SetBeatsPerBar.\n"
"-- PatternBars is the pattern length in bars (1 unless set by SetPatternBars).\n"
"--\n"
"-- SetName(name)                     names the script; shown in the window title (at most 64 bytes)\n"
"-- SetBeatsPerBar(beats)             bar length 1 .. 16; replaces the Preset's\n"
"-- SetPatternBars(bars)              pattern length 1 .. 8 bars\n"
"-- ClearPattern()                    start the pattern from scratch\n"
"-- AddHit(position, instrument)      one expected hit, velocity 100\n"
"-- AddHit(position, instrument, vel) one expected hit with velocity 1 .. 127\n"
"--\n"
"-- position: beats from the pattern start, 0 = beat 1 of bar 1, 0.5 = the 'and' of 1,\n"
"--           BarBeats = beat 1 of bar 2. Must satisfy 0 <= position < PatternBars * BarBeats.\n"
"-- instrument constants:\n"
"--   Kick Snare HihatClosed HihatOpen Crash TomHigh TomLow TomFloor\n"
"-- An instrument may appear only once per position, and so may the hihat (closed or open).\n"
"--\n"
"-- Example, a rock beat:\n"
"--\n"
"-- SetName('Rock beat')\n"
"-- ClearPattern()\n"
"-- for beat = 0, BarBeats - 1 do\n"
"--     AddHit(beat, HihatClosed, 100)\n"
"--     AddHit(beat + 0.5, HihatClosed, 70)\n"
"--     AddHit(beat, beat % 2 == 0 and Kick or Snare)\n"
"-- end\n";
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
    void luaSetPatternBars(double bars);
    void luaClearPattern();
    void luaAddHit(double position, long long instrument, double velocity);
    [[nodiscard]] static size_t checkedCount(std::string_view function, std::string_view what, double value,
                                             size_t maximum);
    [[nodiscard]] float checkedPosition(double position) const;
    void requireHitsWithin(std::string_view function, size_t beats) const;
    void updateLengthGlobals();

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
    m_lua.set_function("SetPatternBars", [this](const double bars) { luaSetPatternBars(bars); });
    m_lua.set_function("ClearPattern", [this] { luaClearPattern(); });
    m_lua.set_function("AddHit",
                       sol::overload([this](const double position, const long long instrument)
                                     { luaAddHit(position, instrument, DrumPattern::kDefaultVelocity); },
                                     [this](const double position, const long long instrument, const double velocity)
                                     { luaAddHit(position, instrument, velocity); }));
    for (size_t i = 0; i < DrumPattern::kInstrumentNames.size(); ++i)
    {
        m_lua[std::string{DrumPattern::kInstrumentNames[i]}] = static_cast<long long>(i);
    }
}

inline bool DrumTrainerScriptEngine::loadPattern(const std::string_view source, const size_t barBeats)
{
    m_result = Result{};
    m_barBeats = barBeats;
    updateLengthGlobals();
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

inline void DrumTrainerScriptEngine::updateLengthGlobals()
{
    m_lua["BarBeats"] = static_cast<long long>(m_barBeats);
    m_lua["PatternBars"] = static_cast<long long>(m_result.pattern.patternBars);
}

inline size_t DrumTrainerScriptEngine::checkedCount(const std::string_view function, const std::string_view what,
                                                    const double value, const size_t maximum)
{
    if (!std::isfinite(value) || value != std::floor(value) || value < 1.0 || value > static_cast<double>(maximum))
    {
        throw std::runtime_error(std::format("{}: {} must be a whole number from 1 to {}", function, what, maximum));
    }
    return static_cast<size_t>(value);
}

inline float DrumTrainerScriptEngine::checkedPosition(const double position) const
{
    const size_t patternBeats = m_barBeats * m_result.pattern.patternBars;
    if (!std::isfinite(position) || position < 0.0 || position >= static_cast<double>(patternBeats))
    {
        throw std::runtime_error(
            std::format("AddHit: position {} is outside the pattern (0 <= position < {})", position, patternBeats));
    }
    return static_cast<float>(position);
}

inline void DrumTrainerScriptEngine::requireHitsWithin(const std::string_view function, const size_t beats) const
{
    const auto& pattern = m_result.pattern;
    if (pattern.hitCount > 0 && pattern.hits[pattern.hitCount - 1].positionBeats >= static_cast<float>(beats))
    {
        throw std::runtime_error(std::string{function} + ": hits already added lie outside the new pattern length");
    }
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
    requireLoading("SetBeatsPerBar");
    const size_t count = checkedCount("SetBeatsPerBar", "beats", beats, DrumPattern::kMaxBeatsPerBar);
    requireHitsWithin("SetBeatsPerBar", count * m_result.pattern.patternBars);
    m_barBeats = count;
    m_result.pattern.beatsPerBar = count;
    m_result.pattern.active = true;
    updateLengthGlobals();
}

inline void DrumTrainerScriptEngine::luaSetPatternBars(const double bars)
{
    requireLoading("SetPatternBars");
    const size_t count = checkedCount("SetPatternBars", "bars", bars, DrumPattern::kMaxPatternBars);
    requireHitsWithin("SetPatternBars", m_barBeats * count);
    m_result.pattern.patternBars = count;
    m_result.pattern.active = true;
    updateLengthGlobals();
}

inline void DrumTrainerScriptEngine::luaClearPattern()
{
    requireLoading("ClearPattern");
    auto& pattern = m_result.pattern;
    pattern.hitCount = 0;
    pattern.active = true;
}

inline void DrumTrainerScriptEngine::luaAddHit(const double position, const long long instrument, const double velocity)
{
    using namespace DrumPattern;
    requireLoading("AddHit");
    const float checkedPos = checkedPosition(position);
    const auto kind = instrumentFromIndex(instrument);
    if (!kind)
    {
        throw std::runtime_error("AddHit: unknown instrument " + std::to_string(instrument));
    }
    if (!std::isfinite(velocity) || velocity != std::floor(velocity) || velocity < 1.0 || velocity > 127.0)
    {
        throw std::runtime_error("AddHit: velocity must be a whole number from 1 to 127");
    }
    HitPattern& pattern = m_result.pattern;
    const std::span<Hit> hits{pattern.hits.data(), pattern.hitCount};
    const auto clashes = [&](const Hit& other)
    { return std::abs(other.positionBeats - checkedPos) < kSamePositionBeats && sharesALimb(other.instrument, *kind); };
    if (const auto clash = std::ranges::find_if(hits, clashes); clash != hits.end())
    {
        throw std::runtime_error(std::format("AddHit: {} at position {} clashes with {} already there", luaName(*kind),
                                             position, luaName(clash->instrument)));
    }
    if (pattern.hitCount >= kMaxHits)
    {
        throw std::runtime_error("AddHit: a pattern holds at most " + std::to_string(kMaxHits) + " hits");
    }
    const Hit hit{checkedPos, *kind, static_cast<uint8_t>(velocity)};
    const auto where = std::ranges::upper_bound(hits, checkedPos, {}, &Hit::positionBeats);
    std::ranges::move_backward(where, hits.end(), std::next(hits.end()));
    *where = hit;
    ++pattern.hitCount;
    pattern.active = true;
}
