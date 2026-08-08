#pragma once

#define SOL_USING_CXX_LUA 1

#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <sol/sol.hpp>
#include <string>
#include <string_view>

#include "DroneScriptMemoryPool.h"

struct DroneNote
{
    float noteHeight{0.f};
    float velocity{0.f};
    size_t channel{0};
    float lengthMs{0.f};
    float delayMs{0.f};
};

/**
 * Owns a pool-allocated Lua state and the sol2 bindings for a drone-sequencer script.
 * nextNotes() and notifyTiming() are the audio-thread-safe entry points: they read only
 * numeric fields out of Lua tables and never throw, so nothing beyond the fixed script
 * arena allocates. loadScript() recompiles the script in place and is not real-time
 * safe (it may touch the process heap) - callers apply a pending script off the audio
 * thread, or accept the one-off cost of doing so on it.
 */
class DroneScriptEngine
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

    explicit DroneScriptEngine(size_t poolBytes = 512 * 1024);

    bool loadScript(std::string_view source);

    void notifyTiming(float bpm, int divisionIndex) noexcept;

    struct NextNotesResult
    {
        std::array<DroneNote, kMaxNotesPerRequest> notes{};
        size_t count{0};
    };
    [[nodiscard]] NextNotesResult nextNotes() noexcept;

    [[nodiscard]] bool hasError() const noexcept
    {
        return !m_lastError.empty();
    }

    [[nodiscard]] const std::string& lastError() const noexcept
    {
        return m_lastError;
    }

    [[nodiscard]] size_t poolBytesInUse() const noexcept
    {
        return m_pool.bytesInUse();
    }

    // Not real-time safe: runs a full GC cycle rather than Lua's normal incremental
    // pacing. Intended for a host to call off the audio thread at a safe idle moment,
    // not from nextNotes()'s caller.
    void collectGarbage() noexcept
    {
        m_lua.collect_garbage();
    }

  private:
    struct LuaStateDeleter
    {
        void operator()(lua_State* state) const noexcept
        {
            if (state != nullptr)
            {
                lua_close(state);
            }
        }
    };

    void bindFunctions();

    DroneScriptMemoryPool m_pool;
    std::unique_ptr<lua_State, LuaStateDeleter> m_state;
    sol::state_view m_lua;
    sol::protected_function m_nextNotesFn;
    sol::protected_function m_onTimingFn;
    std::string m_lastError;
};

inline DroneScriptEngine::DroneScriptEngine(const size_t poolBytes)
    : m_pool(poolBytes)
    , m_state(lua_newstate(&DroneScriptMemoryPool::luaAlloc, &m_pool))
    , m_lua(m_state.get())
{
    assert(m_state != nullptr);
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
    loadScript(kStubScript);
}

inline void DroneScriptEngine::bindFunctions()
{
    m_nextNotesFn = m_lua["NextNotes"];
    m_onTimingFn = m_lua["OnTiming"];
}

inline bool DroneScriptEngine::loadScript(const std::string_view source)
{
    try
    {
        sol::protected_function_result result = m_lua.safe_script(source, sol::script_pass_on_error);
        if (!result.valid())
        {
            const sol::error err = result;
            m_lastError = err.what();
            return false;
        }
    }
    catch (const std::exception& e)
    {
        m_lastError = e.what();
        return false;
    }
    m_lastError.clear();
    bindFunctions();
    return true;
}

inline void DroneScriptEngine::notifyTiming(const float bpm, const int divisionIndex) noexcept
{
    if (!m_onTimingFn.valid())
    {
        return;
    }
    try
    {
        const sol::protected_function_result result = m_onTimingFn(bpm, divisionIndex);
        if (!result.valid())
        {
            const sol::error err = result;
            m_lastError = err.what();
        }
    }
    catch (const std::exception& e)
    {
        m_lastError = e.what();
    }
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
    }
    catch (const std::exception& e)
    {
        m_lastError = e.what();
        return NextNotesResult{};
    }
    return out;
}
