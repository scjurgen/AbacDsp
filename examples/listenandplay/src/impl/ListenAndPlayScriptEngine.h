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
#include "NotePattern.h"

/**
 * Runs a lesson script: its top-level code calls SetName(), SetBeatsPerBar(), SetPatternBars(),
 * SetPickup(), SetTempo(), SetFocus(), SetClickDefault(), ClearPattern() and AddNote(), which fill
 * a staging result that the caller publishes after a successful load. The pattern is the phrase
 * the user is expected to play back; positions are beats from the downbeat of bar 1, negative for
 * an anacrusis. The globals BarBeats, PatternBars, Seed, RangeLow and RangeHigh describe the
 * request the lesson has to generate for; math.random is seeded with Seed before the script runs,
 * so one seed always yields the same phrase. The functions only work while loadPattern() runs.
 */
class ListenAndPlayScriptEngine : public LuaScriptEngineBase<ListenAndPlayScriptEngine>
{
  public:
    struct Result
    {
        NotePattern::Pattern pattern;
        std::string name;
    };

    static constexpr size_t kDefaultBarBeats{4};
    static constexpr long long kDefaultRangeLow{48};
    static constexpr long long kDefaultRangeHigh{79};

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- ListenAndPlay lesson script: defines the phrase to repeat. See the Reset button for the\n"
"-- available functions.\n";

    static constexpr std::string_view kSkeletonScript =
"-- ListenAndPlay lesson script. Top-level code runs once per phrase and defines what the user\n"
"-- is expected to play back.\n"
"--\n"
"-- SetName(name)                        names the lesson (at most 64 bytes)\n"
"-- SetBeatsPerBar(beats)                bar length 1 .. 16\n"
"-- SetPatternBars(bars)                 phrase length 1 .. 8 bars\n"
"-- SetPickup(beats)                     anacrusis length 0 .. 8 beats; notes may start from -beats\n"
"-- SetTempo(bpm)                        suggested tempo 30 .. 240\n"
"-- SetFocus(focus)                      FocusPitch, FocusRhythm, FocusDynamics or FocusCombined\n"
"-- SetClickDefault(on)                  click state the lesson starts with\n"
"-- SetTonic(pitch)                      the key note the Reference button plays, MIDI 0 .. 127\n"
"-- SetTimingTolerance(good, zero)       onset error in ms: full credit up to good, none from zero on\n"
"-- SetDynamicsTolerance(good, zero)     relative level error in dB, same bands\n"
"-- SetPassScore(percent)                score from which the lesson counts as passed, 1 .. 100\n"
"-- SetOctaveFree(on)                    accept the phrase an octave or more away (default true)\n"
"-- ClearPattern()                       start the phrase from scratch\n"
"-- AddNote(position, pitch, duration)   one note, velocity 100\n"
"-- AddNote(position, pitch, duration, velocity)   velocity 1 .. 127\n"
"--\n"
"-- position: beats from the downbeat, 0 = beat 1 of bar 1, 0.5 = the 'and' of 1, negative = pickup.\n"
"-- pitch: MIDI note, 60 = C4. duration: beats. Notes may not overlap and must end inside the phrase.\n"
"-- Globals: BarBeats, PatternBars, Seed, RangeLow, RangeHigh (the playable range as MIDI notes).\n"
"-- math.random is seeded with Seed, so the same seed yields the same phrase.\n"
"--\n"
"-- import \"lessonlib\" adds the helpers for scales, melodies, rhythm cells and dynamics.\n"
"--\n"
"-- Example:\n"
"--\n"
"-- SetName('Up the triad')\n"
"-- ClearPattern()\n"
"-- AddNote(0, 60, 1)\n"
"-- AddNote(1, 64, 1)\n"
"-- AddNote(2, 67, 2)\n";
    // clang-format on

    explicit ListenAndPlayScriptEngine(size_t poolBytes = 256 * 1024);

    // The playable range handed to the next lessons as RangeLow / RangeHigh.
    void setRange(long long low, long long high);

    // Runs the script for a bar of barBeats beats; on failure result() is left empty.
    [[nodiscard]] bool loadPattern(std::string_view source, size_t barBeats = kDefaultBarBeats, long long seed = 1);

    [[nodiscard]] const Result& result() const noexcept
    {
        return m_result;
    }

  private:
    friend class LuaScriptEngineBase<ListenAndPlayScriptEngine>;
    void bindScriptFunctions() {}

    void bindPatternApi();
    void bindConstants();
    void requireLoading(std::string_view function) const;
    void luaSetName(const std::string& name);
    void luaSetBeatsPerBar(double beats);
    void luaSetPatternBars(double bars);
    void luaSetPickup(double beats);
    void luaSetTempo(double bpm);
    void luaSetFocus(long long focus);
    void luaSetClickDefault(bool on);
    void luaSetTonic(double pitch);
    void luaSetTimingTolerance(double goodMs, double zeroMs);
    void luaSetDynamicsTolerance(double goodDb, double zeroDb);
    void luaSetPassScore(double percent);
    void luaSetOctaveFree(bool on);
    void luaClearPattern();
    void luaAddNote(double position, double pitch, double duration, double velocity);
    [[nodiscard]] static size_t checkedCount(std::string_view function, std::string_view what, double value,
                                             size_t maximum);
    [[nodiscard]] static uint8_t checkedMidiValue(std::string_view function, std::string_view what, double value,
                                                  double minimum);
    void requireNotesWithin(std::string_view function, float pickupBeats, float patternBeats) const;
    void requireRoom(float position, float duration, size_t insertAt) const;
    void updateGlobals();

    Result m_result{};
    size_t m_barBeats{kDefaultBarBeats};
    long long m_rangeLow{kDefaultRangeLow};
    long long m_rangeHigh{kDefaultRangeHigh};
    long long m_seed{1};
    std::atomic<bool> m_loading{false};
};

inline ListenAndPlayScriptEngine::ListenAndPlayScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<ListenAndPlayScriptEngine>(poolBytes)
{
    bindPatternApi();
    bindConstants();
}

inline void ListenAndPlayScriptEngine::bindPatternApi()
{
    m_lua.set_function("SetName", [this](const std::string& name) { luaSetName(name); });
    m_lua.set_function("SetBeatsPerBar", [this](const double beats) { luaSetBeatsPerBar(beats); });
    m_lua.set_function("SetPatternBars", [this](const double bars) { luaSetPatternBars(bars); });
    m_lua.set_function("SetPickup", [this](const double beats) { luaSetPickup(beats); });
    m_lua.set_function("SetTempo", [this](const double bpm) { luaSetTempo(bpm); });
    m_lua.set_function("SetFocus", [this](const long long focus) { luaSetFocus(focus); });
    m_lua.set_function("SetClickDefault", [this](const bool on) { luaSetClickDefault(on); });
    m_lua.set_function("SetTimingTolerance",
                       [this](const double good, const double zero) { luaSetTimingTolerance(good, zero); });
    m_lua.set_function("SetDynamicsTolerance",
                       [this](const double good, const double zero) { luaSetDynamicsTolerance(good, zero); });
    m_lua.set_function("SetPassScore", [this](const double percent) { luaSetPassScore(percent); });
    m_lua.set_function("SetOctaveFree", [this](const bool on) { luaSetOctaveFree(on); });
    m_lua.set_function("SetTonic", [this](const double pitch) { luaSetTonic(pitch); });
    m_lua.set_function("ClearPattern", [this] { luaClearPattern(); });
    m_lua.set_function(
        "AddNote", sol::overload([this](const double position, const double pitch, const double duration)
                                 { luaAddNote(position, pitch, duration, NotePattern::kDefaultVelocity); },
                                 [this](const double position, const double pitch, const double duration,
                                        const double velocity) { luaAddNote(position, pitch, duration, velocity); }));
}

inline void ListenAndPlayScriptEngine::bindConstants()
{
    for (size_t i = 0; i < NotePattern::kFocusNames.size(); ++i)
    {
        m_lua[std::string{NotePattern::kFocusNames[i]}] = static_cast<long long>(i);
    }
}

inline void ListenAndPlayScriptEngine::setRange(const long long low, const long long high)
{
    m_rangeLow = low;
    m_rangeHigh = high;
}

inline bool ListenAndPlayScriptEngine::loadPattern(const std::string_view source, const size_t barBeats,
                                                   const long long seed)
{
    m_result = Result{};
    m_barBeats = barBeats;
    m_result.pattern.beatsPerBar = barBeats;
    m_seed = seed;
    updateGlobals();
    m_lua["math"]["randomseed"](seed);
    m_loading.store(true, std::memory_order_release);
    const bool loaded = loadScript(source);
    m_loading.store(false, std::memory_order_release);
    if (!loaded)
    {
        m_result = Result{};
    }
    return loaded;
}

inline void ListenAndPlayScriptEngine::requireLoading(const std::string_view function) const
{
    if (!m_loading.load(std::memory_order_acquire))
    {
        throw std::runtime_error(std::string{function} + " can only be called at the top level of the script");
    }
}

inline void ListenAndPlayScriptEngine::updateGlobals()
{
    m_lua["BarBeats"] = static_cast<long long>(m_barBeats);
    m_lua["PatternBars"] = static_cast<long long>(m_result.pattern.patternBars);
    m_lua["Seed"] = m_seed;
    m_lua["RangeLow"] = m_rangeLow;
    m_lua["RangeHigh"] = m_rangeHigh;
}

inline size_t ListenAndPlayScriptEngine::checkedCount(const std::string_view function, const std::string_view what,
                                                      const double value, const size_t maximum)
{
    if (!std::isfinite(value) || value != std::floor(value) || value < 1.0 || value > static_cast<double>(maximum))
    {
        throw std::runtime_error(std::format("{}: {} must be a whole number from 1 to {}", function, what, maximum));
    }
    return static_cast<size_t>(value);
}

inline uint8_t ListenAndPlayScriptEngine::checkedMidiValue(const std::string_view function, const std::string_view what,
                                                           const double value, const double minimum)
{
    if (!std::isfinite(value) || value != std::floor(value) || value < minimum || value > 127.0)
    {
        throw std::runtime_error(
            std::format("{}: {} must be a whole number from {} to 127", function, what, static_cast<int>(minimum)));
    }
    return static_cast<uint8_t>(value);
}

inline void ListenAndPlayScriptEngine::requireNotesWithin(const std::string_view function, const float pickupBeats,
                                                          const float patternBeats) const
{
    const auto& pattern = m_result.pattern;
    if (pattern.noteCount == 0)
    {
        return;
    }
    const auto& first = pattern.notes.front();
    const auto& last = pattern.notes[pattern.noteCount - 1];
    const bool startsInside = first.positionBeats >= -pickupBeats - NotePattern::kEpsilonBeats;
    const bool endsInside = last.positionBeats + last.durationBeats <= patternBeats + NotePattern::kEpsilonBeats;
    if (!startsInside || !endsInside)
    {
        throw std::runtime_error(std::string{function} + ": notes already added lie outside the new phrase length");
    }
}

inline void ListenAndPlayScriptEngine::requireRoom(const float position, const float duration,
                                                   const size_t insertAt) const
{
    const auto& pattern = m_result.pattern;
    const bool clashesBefore =
        insertAt > 0 && pattern.notes[insertAt - 1].positionBeats + pattern.notes[insertAt - 1].durationBeats >
                            position + NotePattern::kEpsilonBeats;
    const bool clashesAfter = insertAt < pattern.noteCount &&
                              position + duration > pattern.notes[insertAt].positionBeats + NotePattern::kEpsilonBeats;
    if (clashesBefore || clashesAfter)
    {
        throw std::runtime_error(
            std::format("AddNote: the note at {} overlaps a note already there (the phrase is monophonic)", position));
    }
}

inline void ListenAndPlayScriptEngine::luaSetName(const std::string& name)
{
    requireLoading("SetName");
    const auto isControl = [](const char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; };
    if (name.empty() || name.size() > NotePattern::kMaxNameBytes || std::ranges::any_of(name, isControl))
    {
        throw std::runtime_error("SetName: the name must be 1 to " + std::to_string(NotePattern::kMaxNameBytes) +
                                 " bytes without control characters");
    }
    m_result.name = name;
}

inline void ListenAndPlayScriptEngine::luaSetBeatsPerBar(const double beats)
{
    requireLoading("SetBeatsPerBar");
    const size_t count = checkedCount("SetBeatsPerBar", "beats", beats, NotePattern::kMaxBeatsPerBar);
    auto& pattern = m_result.pattern;
    requireNotesWithin("SetBeatsPerBar", pattern.pickupBeats, static_cast<float>(count * pattern.patternBars));
    m_barBeats = count;
    pattern.beatsPerBar = count;
    pattern.active = true;
    updateGlobals();
}

inline void ListenAndPlayScriptEngine::luaSetPatternBars(const double bars)
{
    requireLoading("SetPatternBars");
    const size_t count = checkedCount("SetPatternBars", "bars", bars, NotePattern::kMaxPatternBars);
    auto& pattern = m_result.pattern;
    requireNotesWithin("SetPatternBars", pattern.pickupBeats, static_cast<float>(pattern.beatsPerBar * count));
    pattern.patternBars = count;
    pattern.active = true;
    updateGlobals();
}

inline void ListenAndPlayScriptEngine::luaSetPickup(const double beats)
{
    requireLoading("SetPickup");
    if (!std::isfinite(beats) || beats < 0.0 || beats > static_cast<double>(NotePattern::kMaxPickupBeats))
    {
        throw std::runtime_error(std::format("SetPickup: beats must be from 0 to {}", NotePattern::kMaxPickupBeats));
    }
    auto& pattern = m_result.pattern;
    requireNotesWithin("SetPickup", static_cast<float>(beats), pattern.patternBeats());
    pattern.pickupBeats = static_cast<float>(beats);
    pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetTempo(const double bpm)
{
    requireLoading("SetTempo");
    if (!std::isfinite(bpm) || bpm < static_cast<double>(NotePattern::kMinBpm) ||
        bpm > static_cast<double>(NotePattern::kMaxBpm))
    {
        throw std::runtime_error(
            std::format("SetTempo: bpm must be from {} to {}", NotePattern::kMinBpm, NotePattern::kMaxBpm));
    }
    m_result.pattern.bpm = static_cast<float>(bpm);
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetFocus(const long long focus)
{
    requireLoading("SetFocus");
    const auto kind = NotePattern::focusFromIndex(focus);
    if (!kind)
    {
        throw std::runtime_error("SetFocus: unknown focus " + std::to_string(focus));
    }
    m_result.pattern.focus = *kind;
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetClickDefault(const bool on)
{
    requireLoading("SetClickDefault");
    m_result.pattern.clickDefault = on;
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetTimingTolerance(const double goodMs, const double zeroMs)
{
    requireLoading("SetTimingTolerance");
    if (!std::isfinite(goodMs) || !std::isfinite(zeroMs) || goodMs < 0.0 || zeroMs <= goodMs ||
        zeroMs > NotePattern::kMaxTimingToleranceMs)
    {
        throw std::runtime_error(
            std::format("SetTimingTolerance: need 0 <= good < zero <= {} ms", NotePattern::kMaxTimingToleranceMs));
    }
    m_result.pattern.scoring.timingGoodMs = static_cast<float>(goodMs);
    m_result.pattern.scoring.timingZeroMs = static_cast<float>(zeroMs);
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetDynamicsTolerance(const double goodDb, const double zeroDb)
{
    requireLoading("SetDynamicsTolerance");
    if (!std::isfinite(goodDb) || !std::isfinite(zeroDb) || goodDb < 0.0 || zeroDb <= goodDb ||
        zeroDb > NotePattern::kMaxDynamicsToleranceDb)
    {
        throw std::runtime_error(
            std::format("SetDynamicsTolerance: need 0 <= good < zero <= {} dB", NotePattern::kMaxDynamicsToleranceDb));
    }
    m_result.pattern.scoring.dynamicsGoodDb = static_cast<float>(goodDb);
    m_result.pattern.scoring.dynamicsZeroDb = static_cast<float>(zeroDb);
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetPassScore(const double percent)
{
    requireLoading("SetPassScore");
    if (!std::isfinite(percent) || percent < 1.0 || percent > 100.0)
    {
        throw std::runtime_error("SetPassScore: percent must be from 1 to 100");
    }
    m_result.pattern.scoring.passScore = static_cast<float>(percent);
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetOctaveFree(const bool on)
{
    requireLoading("SetOctaveFree");
    m_result.pattern.scoring.octaveFree = on;
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaSetTonic(const double pitch)
{
    requireLoading("SetTonic");
    m_result.pattern.tonic = checkedMidiValue("SetTonic", "pitch", pitch, 0.0);
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaClearPattern()
{
    requireLoading("ClearPattern");
    m_result.pattern.noteCount = 0;
    m_result.pattern.active = true;
}

inline void ListenAndPlayScriptEngine::luaAddNote(const double position, const double pitch, const double duration,
                                                  const double velocity)
{
    using namespace NotePattern;
    requireLoading("AddNote");
    auto& pattern = m_result.pattern;
    const double patternBeats = static_cast<double>(pattern.patternBeats());
    if (!std::isfinite(position) || position < -static_cast<double>(pattern.pickupBeats) - kEpsilonBeats ||
        position >= patternBeats)
    {
        throw std::runtime_error(std::format("AddNote: position {} is outside the phrase ({} <= position < {})",
                                             position, -pattern.pickupBeats, patternBeats));
    }
    if (!std::isfinite(duration) || duration <= 0.0 || position + duration > patternBeats + kEpsilonBeats)
    {
        throw std::runtime_error(std::format(
            "AddNote: duration {} must be positive and end inside the phrase (at {} beats)", duration, patternBeats));
    }
    const uint8_t checkedPitch = checkedMidiValue("AddNote", "pitch", pitch, 0.0);
    const uint8_t checkedVelocity = checkedMidiValue("AddNote", "velocity", velocity, 1.0);
    if (pattern.noteCount >= kMaxNotes)
    {
        throw std::runtime_error("AddNote: a phrase holds at most " + std::to_string(kMaxNotes) + " notes");
    }
    const std::span<Note> notes{pattern.notes.data(), pattern.noteCount};
    const auto checkedPosition = static_cast<float>(position);
    const auto where = std::ranges::upper_bound(notes, checkedPosition, {}, &Note::positionBeats);
    requireRoom(checkedPosition, static_cast<float>(duration),
                static_cast<size_t>(std::distance(notes.begin(), where)));
    std::ranges::move_backward(where, notes.end(), std::next(notes.end()));
    *where = Note{checkedPosition, static_cast<float>(duration), checkedPitch, checkedVelocity};
    ++pattern.noteCount;
    pattern.active = true;
}
