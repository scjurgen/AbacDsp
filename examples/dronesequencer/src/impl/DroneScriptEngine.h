#pragma once

#define SOL_USING_CXX_LUA 1

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <memory>
#include <sol/sol.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "DroneScriptMemoryPool.h"

struct DroneNote
{
    float noteHeight{0.f};
    float velocity{0.f};
    size_t channel{0};
    float lengthMs{0.f};
    float delayMs{0.f};
};

enum class LuaUiParamType
{
    Knob,
    Drop,
    Switch
};

// One slot in the fixed UI-parameter pool a script can claim via UICreateParameterSet().
// Unclaimed (claimed == false) slots are not shown; range/items/etc. are only meaningful
// once claimed. "id" builds the script's per-parameter callback name (On<Id>Changed);
// "name" is the display label.
struct LuaUiParamSlot
{
    bool claimed{false};
    std::string id;
    std::string name;
    LuaUiParamType type{LuaUiParamType::Knob};
    float rangeMin{0.f};
    float rangeMax{1.f};
    float rangeStep{0.f};
    float rangeSkew{1.f};
    float defaultValue{0.f};
    std::string unit;
    std::string description;
    std::vector<std::string> items;

    // Detects "did the script's declared parameter set change since the last rebuild"
    // (LuaControlArea); exact float equality is intentional, not a tolerance check.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    bool operator==(const LuaUiParamSlot&) const = default;
#pragma GCC diagnostic pop
};

/**
 * Owns a pool-allocated Lua state and the sol2 bindings for a drone-sequencer script.
 * nextNotes() and notifyTiming() are the audio-thread-safe entry points: they read only
 * numeric fields out of Lua tables and never throw, so nothing beyond the fixed script
 * arena allocates. loadScript() recompiles the script in place and is not real-time
 * safe (it may touch the process heap) - callers apply a pending script off the audio
 * thread, or accept the one-off cost of doing so on it. notifyUiParameterChanged() is
 * likewise audio-thread-safe; a script claims UI parameter slots via the Lua-side
 * UICreateParameterSet() during loadScript(), not from the audio thread.
 */
class DroneScriptEngine
{
  public:
    static constexpr size_t kMaxNotesPerRequest{8};
    static constexpr size_t kMaxLuaParams{8};
    using UiParamSlots = std::array<LuaUiParamSlot, kMaxLuaParams>;

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

    // Shown by the popup editor's Reset button: every available hook, ready to fill in,
    // as opposed to kStubScript above (which is deliberately minimal - what a fresh
    // patch actually plays out of the box).
    // clang-format off
    static constexpr std::string_view kFullSkeletonScript =
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
"\n"
"-- Fires on any start/stop transition: the manual Play switch toggling, or the host\n"
"-- transport's play state when Host Sync is on. Useful for resetting your own state.\n"
"function OnStart()\n"
"end\n"
"\n"
"function OnStop()\n"
"end\n"
"\n"
"function OnNoteOn(channel, note, velocity)\n"
"end\n"
"\n"
"function OnNoteOff(channel, note, velocity)\n"
"end\n"
"\n"
"function OnCC(channel, ccNumber, value)\n"
"end\n"
"\n"
"function OnProgramChange(channel, program)\n"
"end\n"
"\n"
"function OnAftertouch(channel, value)\n"
"end\n"
"\n"
"function OnPolyPressure(channel, note, value)\n"
"end\n"
"\n"
"function OnPitchBend(channel, bendValue)\n"
"end\n";
    // clang-format on

    explicit DroneScriptEngine(size_t poolBytes = 512 * 1024);

    bool loadScript(std::string_view source);

    void notifyTiming(float bpm, int divisionIndex) noexcept;

    // Fires on any effective start/stop transition (see kFullSkeletonScript's comment).
    void notifyStart() noexcept;
    void notifyStop() noexcept;

    // MIDI event dispatch - each maps to an optional Lua handler (OnNoteOn, OnNoteOff,
    // OnCC, OnProgramChange, OnAftertouch, OnPolyPressure, OnPitchBend); a script that
    // doesn't define one simply never gets called for that event, same as OnTiming.
    // channel is 0-based (0..15); a Note On with velocity 0 is normalized to a Note Off
    // by the caller (DroneSequencerImpl), per standard MIDI running-status convention.
    void notifyNoteOn(int channel, int noteHeight, int velocity) noexcept;
    void notifyNoteOff(int channel, int noteHeight, int velocity) noexcept;
    void notifyCC(int channel, int ccNumber, int value) noexcept;
    void notifyProgramChange(int channel, int program) noexcept;
    void notifyAftertouch(int channel, int value) noexcept;
    void notifyPolyPressure(int channel, int noteHeight, int value) noexcept;
    void notifyPitchBend(int channel, int bendValue) noexcept;

    struct NextNotesResult
    {
        std::array<DroneNote, kMaxNotesPerRequest> notes{};
        size_t count{0};
    };
    [[nodiscard]] NextNotesResult nextNotes() noexcept;

    [[nodiscard]] const UiParamSlots& uiParamSlots() const noexcept
    {
        return m_uiParamSlots;
    }

    // Realtime-safe: dispatches to the cached On<Id>Changed handler for this slot, if
    // the script defined one. Silently ignored for an unclaimed or out-of-range slot.
    void notifyUiParameterChanged(size_t slot, float value) noexcept;

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
    void bindApiFunctions();
    void registerUiParameterSet(const sol::table& descriptors);
    [[nodiscard]] static LuaUiParamSlot parseUiParamSlot(const sol::table& entry, size_t index);
    [[nodiscard]] static std::string capitalizeFirst(std::string_view text);
    // Maps raw 0..1 through a slot's display range (mirrors LuaControlArea.h's
    // juce::NormalisableRange use, reimplemented here since this file stays JUCE-free).
    [[nodiscard]] static float mapNormalizedToDisplay(const LuaUiParamSlot& slot, float normalized) noexcept;

    // Shared body for every optional-handler dispatch (notifyTiming and all the MIDI
    // notify*() methods): no-op if the script didn't define this handler, catches
    // anything the call throws, and clears/sets m_lastError to reflect this call only.
    template <typename... Args>
    void callHandler(sol::protected_function& fn, Args&&... args) noexcept
    {
        if (!fn.valid())
        {
            return;
        }
        try
        {
            const sol::protected_function_result result = fn(std::forward<Args>(args)...);
            if (!result.valid())
            {
                const sol::error err = result;
                m_lastError = err.what();
                return;
            }
            m_lastError.clear();
        }
        catch (const std::exception& e)
        {
            m_lastError = e.what();
        }
    }

    DroneScriptMemoryPool m_pool;
    std::unique_ptr<lua_State, LuaStateDeleter> m_state;
    sol::state_view m_lua;
    sol::protected_function m_nextNotesFn;
    sol::protected_function m_onTimingFn;
    sol::protected_function m_onNoteOnFn;
    sol::protected_function m_onNoteOffFn;
    sol::protected_function m_onCcFn;
    sol::protected_function m_onProgramChangeFn;
    sol::protected_function m_onAftertouchFn;
    sol::protected_function m_onPolyPressureFn;
    sol::protected_function m_onPitchBendFn;
    sol::protected_function m_onStartFn;
    sol::protected_function m_onStopFn;
    std::string m_lastError;

    UiParamSlots m_uiParamSlots{};
    UiParamSlots m_pendingUiParamSlots{};
    std::array<sol::protected_function, kMaxLuaParams> m_uiParamChangedFns{};
};

inline DroneScriptEngine::DroneScriptEngine(const size_t poolBytes)
    : m_pool(poolBytes)
    , m_state(lua_newstate(&DroneScriptMemoryPool::luaAlloc, &m_pool))
    , m_lua(m_state.get())
{
    assert(m_state != nullptr);
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
    bindApiFunctions();
    loadScript(kStubScript);
}

inline void DroneScriptEngine::bindApiFunctions()
{
    m_lua.set_function("UICreateParameterSet", &DroneScriptEngine::registerUiParameterSet, this);
}

inline void DroneScriptEngine::bindFunctions()
{
    m_nextNotesFn = m_lua["NextNotes"];
    m_onTimingFn = m_lua["OnTiming"];
    m_onNoteOnFn = m_lua["OnNoteOn"];
    m_onNoteOffFn = m_lua["OnNoteOff"];
    m_onCcFn = m_lua["OnCC"];
    m_onProgramChangeFn = m_lua["OnProgramChange"];
    m_onAftertouchFn = m_lua["OnAftertouch"];
    m_onPolyPressureFn = m_lua["OnPolyPressure"];
    m_onPitchBendFn = m_lua["OnPitchBend"];
    m_onStartFn = m_lua["OnStart"];
    m_onStopFn = m_lua["OnStop"];

    m_uiParamSlots = m_pendingUiParamSlots;
    for (size_t i = 0; i < kMaxLuaParams; ++i)
    {
        if (!m_uiParamSlots[i].claimed)
        {
            m_uiParamChangedFns[i] = sol::protected_function{};
            continue;
        }
        m_uiParamChangedFns[i] = m_lua["On" + capitalizeFirst(m_uiParamSlots[i].id) + "Changed"];
    }
}

inline bool DroneScriptEngine::loadScript(const std::string_view source)
{
    m_pendingUiParamSlots = UiParamSlots{};
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
    callHandler(m_onTimingFn, bpm, divisionIndex);
}

inline void DroneScriptEngine::notifyStart() noexcept
{
    callHandler(m_onStartFn);
}

inline void DroneScriptEngine::notifyStop() noexcept
{
    callHandler(m_onStopFn);
}

inline void DroneScriptEngine::notifyNoteOn(const int channel, const int noteHeight, const int velocity) noexcept
{
    callHandler(m_onNoteOnFn, channel, noteHeight, velocity);
}

inline void DroneScriptEngine::notifyNoteOff(const int channel, const int noteHeight, const int velocity) noexcept
{
    callHandler(m_onNoteOffFn, channel, noteHeight, velocity);
}

inline void DroneScriptEngine::notifyCC(const int channel, const int ccNumber, const int value) noexcept
{
    callHandler(m_onCcFn, channel, ccNumber, value);
}

inline void DroneScriptEngine::notifyProgramChange(const int channel, const int program) noexcept
{
    callHandler(m_onProgramChangeFn, channel, program);
}

inline void DroneScriptEngine::notifyAftertouch(const int channel, const int value) noexcept
{
    callHandler(m_onAftertouchFn, channel, value);
}

inline void DroneScriptEngine::notifyPolyPressure(const int channel, const int noteHeight, const int value) noexcept
{
    callHandler(m_onPolyPressureFn, channel, noteHeight, value);
}

inline void DroneScriptEngine::notifyPitchBend(const int channel, const int bendValue) noexcept
{
    callHandler(m_onPitchBendFn, channel, bendValue);
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

inline void DroneScriptEngine::notifyUiParameterChanged(const size_t slot, const float value) noexcept
{
    if (slot >= kMaxLuaParams)
    {
        return;
    }
    callHandler(m_uiParamChangedFns[slot], mapNormalizedToDisplay(m_uiParamSlots[slot], value));
}

inline float DroneScriptEngine::mapNormalizedToDisplay(const LuaUiParamSlot& slot, float normalized) noexcept
{
    normalized = std::clamp(normalized, 0.f, 1.f);
    const float span = slot.rangeMax - slot.rangeMin;
    float display = slot.rangeSkew > 0.f && slot.rangeSkew != 1.f
                        ? slot.rangeMin + span * std::pow(normalized, slot.rangeSkew)
                        : slot.rangeMin + span * normalized;
    if (slot.rangeStep > 0.f)
    {
        display = slot.rangeMin + std::round((display - slot.rangeMin) / slot.rangeStep) * slot.rangeStep;
    }
    return display;
}

inline std::string DroneScriptEngine::capitalizeFirst(const std::string_view text)
{
    std::string result{text};
    if (!result.empty())
    {
        result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
    }
    return result;
}

inline void DroneScriptEngine::registerUiParameterSet(const sol::table& descriptors)
{
    const size_t count = descriptors.size();
    if (count > kMaxLuaParams)
    {
        throw sol::error("UICreateParameterSet: " + std::to_string(count) + " parameters requested, only " +
                         std::to_string(kMaxLuaParams) + " slots available");
    }

    UiParamSlots fresh{};
    for (size_t i = 1; i <= count; ++i)
    {
        const sol::optional<sol::table> entry = descriptors[i];
        if (!entry)
        {
            throw sol::error("UICreateParameterSet: parameter " + std::to_string(i) + " is not a table");
        }
        fresh[i - 1] = parseUiParamSlot(*entry, i);
        for (size_t j = 0; j + 1 < i; ++j)
        {
            if (fresh[j].id == fresh[i - 1].id)
            {
                throw sol::error("UICreateParameterSet: duplicate parameter id \"" + fresh[i - 1].id + "\"");
            }
        }
    }
    m_pendingUiParamSlots = std::move(fresh);
}

inline LuaUiParamSlot DroneScriptEngine::parseUiParamSlot(const sol::table& entry, const size_t index)
{
    const auto requireString = [&](const char* key) -> std::string
    {
        const sol::optional<std::string> value = entry[key];
        if (!value)
        {
            throw sol::error("UICreateParameterSet: parameter " + std::to_string(index) + " missing \"" + key + "\"");
        }
        return *value;
    };

    LuaUiParamSlot slot{};
    slot.id = requireString("id");
    const bool validId = !slot.id.empty() && (std::isalpha(static_cast<unsigned char>(slot.id.front())) != 0) &&
                         std::ranges::all_of(slot.id, [](const char c)
                                             { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; });
    if (!validId)
    {
        throw sol::error("UICreateParameterSet: parameter " + std::to_string(index) + " has an invalid id \"" +
                         slot.id + "\" - must start with a letter and contain only letters, digits, underscores");
    }
    slot.name = entry.get_or("name", slot.id);

    const std::string typeStr = requireString("type");
    if (typeStr == "knob")
    {
        slot.type = LuaUiParamType::Knob;
    }
    else if (typeStr == "drop")
    {
        slot.type = LuaUiParamType::Drop;
    }
    else if (typeStr == "switch")
    {
        slot.type = LuaUiParamType::Switch;
    }
    else
    {
        throw sol::error("UICreateParameterSet: parameter \"" + slot.id + "\" has unknown type \"" + typeStr + "\"");
    }

    slot.unit = entry.get_or("unit", std::string{});
    slot.description = entry.get_or("description", std::string{});

    const sol::optional<sol::table> itemsOpt = entry["items"];
    if (slot.type == LuaUiParamType::Switch)
    {
        slot.rangeMin = 0.f;
        slot.rangeMax = 1.f;
        slot.rangeStep = 1.f;
        slot.rangeSkew = 1.f;
    }
    else if (slot.type == LuaUiParamType::Drop && itemsOpt)
    {
        for (size_t i = 1; i <= itemsOpt->size(); ++i)
        {
            const sol::optional<std::string> item = (*itemsOpt)[i];
            slot.items.push_back(item.value_or(std::string{}));
        }
        slot.rangeMin = 0.f;
        slot.rangeMax = static_cast<float>(slot.items.size()) - 1.f;
        slot.rangeStep = 1.f;
        slot.rangeSkew = 1.f;
    }
    else
    {
        const sol::optional<sol::table> rangeOpt = entry["range"];
        if (!rangeOpt)
        {
            throw sol::error("UICreateParameterSet: parameter \"" + slot.id + "\" missing \"range\"");
        }
        slot.rangeMin = rangeOpt->get_or("min", 0.f);
        slot.rangeMax = rangeOpt->get_or("max", 1.f);
        slot.rangeStep = rangeOpt->get_or("step", 0.f);
        slot.rangeSkew = rangeOpt->get_or("skew", 1.f);
        if (!(slot.rangeMax > slot.rangeMin))
        {
            throw sol::error("UICreateParameterSet: parameter \"" + slot.id + "\" has an empty or inverted range");
        }
    }

    slot.defaultValue = entry.get_or("default", slot.rangeMin);
    if (slot.defaultValue < slot.rangeMin || slot.defaultValue > slot.rangeMax)
    {
        throw sol::error("UICreateParameterSet: parameter \"" + slot.id + "\" default value is outside its range");
    }
    slot.claimed = true;
    return slot;
}
