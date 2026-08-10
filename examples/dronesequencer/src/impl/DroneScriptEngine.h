#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"

struct DroneNote
{
    float noteHeight{0.f};
    float velocity{0.f};
    size_t channel{0};
    float lengthMs{0.f};
    float delayMs{0.f};
};

/**
 * Adds the drone-sequencer's own scripted entry points - NextNotes() and OnTiming() -
 * on top of LuaScriptEngineBase's shared MIDI/UI-parameter machinery. nextNotes() and
 * notifyTiming() are audio-thread-safe on the same terms as the base's notify*()
 * methods: they read only numeric fields out of Lua tables and never throw.
 */
class DroneScriptEngine : public LuaScriptEngineBase<DroneScriptEngine>
{
  public:
    static constexpr size_t kMaxNotesPerRequest{8};

    // clang-format off
    static constexpr std::string_view kStubScript =
"-- DroneSequencer script\n"
"-- Called whenever BPM or the clock division changes; store what you need as globals.\n"
"function OnTiming(bpm, division)\n"
"    BPM = bpm\n"
"    DIVISION = division\n"
"end\n"
"\n"
"-- Called ahead of each beat. Return an array of notes to play this tick (or an\n"
"-- empty table to play nothing). Each note is a table:\n"
"--   note     MIDI-style note number (60 = middle C)\n"
"--   velocity 0..1 pluck strength\n"
"--   channel  which string to pluck (0-based)\n"
"--   length   ms before the string is muted; 0 lets it ring out naturally\n"
"--   delay    ms offset from the beat; may be negative (fires early)\n"
"function NextNotes()\n"
"    return {\n"
"        { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0 },\n"
"    }\n"
"end\n";
    // clang-format on

    // clang-format off
    static constexpr std::string_view kDroneSkeletonHooks =
"-- DroneSequencer script skeleton - every available hook, ready to fill in.\n"
"-- Delete anything you don't need; an undefined function is simply never called.\n"
"\n"
"function OnTiming(bpm, division)\n"
"    BPM = bpm\n"
"    DIVISION = division\n"
"end\n"
"\n"
"function NextNotes()\n"
"    return {}\n"
"end\n"
"\n";
    // clang-format on

    // Shown by the popup editor's Reset button: kDroneSkeletonHooks above followed by
    // LuaScriptEngineBase::kCommonSkeletonScript, as opposed to kStubScript (deliberately
    // minimal - what a fresh patch actually plays out of the box).
    static const std::string kFullSkeletonScript;

    explicit DroneScriptEngine(size_t poolBytes = 512 * 1024);

    void notifyTiming(float bpm, int divisionIndex) noexcept;

    struct NextNotesResult
    {
        std::array<DroneNote, kMaxNotesPerRequest> notes{};
        size_t count{0};
    };
    [[nodiscard]] NextNotesResult nextNotes() noexcept;

  private:
    friend class LuaScriptEngineBase<DroneScriptEngine>;
    void bindScriptFunctions();

    sol::protected_function m_nextNotesFn;
    sol::protected_function m_onTimingFn;
};

inline const std::string DroneScriptEngine::kFullSkeletonScript =
    std::string(kDroneSkeletonHooks) + std::string(kCommonSkeletonScript);

inline DroneScriptEngine::DroneScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<DroneScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}

inline void DroneScriptEngine::bindScriptFunctions()
{
    m_nextNotesFn = m_lua["NextNotes"];
    m_onTimingFn = m_lua["OnTiming"];
}

inline void DroneScriptEngine::notifyTiming(const float bpm, const int divisionIndex) noexcept
{
    callHandler(m_onTimingFn, bpm, divisionIndex);
}

inline DroneScriptEngine::NextNotesResult DroneScriptEngine::nextNotes() noexcept
{
    NextNotesResult out{};
    if (!m_nextNotesFn.valid())
    {
        return out;
    }
    try
    {
        const sol::protected_function_result result = m_nextNotesFn();
        if (!result.valid())
        {
            const sol::error err = result;
            m_lastError = err.what();
            return out;
        }
        const sol::optional<sol::table> notesTable = result;
        if (!notesTable)
        {
            return out;
        }
        const size_t luaCount = notesTable->size();
        for (size_t i = 1; i <= luaCount && out.count < kMaxNotesPerRequest; ++i)
        {
            const sol::optional<sol::table> entry = (*notesTable)[i];
            if (!entry)
            {
                continue;
            }
            DroneNote& note = out.notes[out.count];
            note.noteHeight = entry->get_or("note", 0.f);
            note.velocity = entry->get_or("velocity", 0.f);
            note.channel = static_cast<size_t>(entry->get_or("channel", 0.f));
            note.lengthMs = entry->get_or("length", 0.f);
            note.delayMs = entry->get_or("delay", 0.f);
            ++out.count;
        }
        m_lastError.clear();
    }
    catch (const std::exception& e)
    {
        m_lastError = e.what();
        return NextNotesResult{};
    }
    return out;
}
