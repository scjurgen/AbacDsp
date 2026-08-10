#pragma once

#define SOL_USING_CXX_LUA 1

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <memory>
#include <sol/sol.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "LuaParamRangeMath.h"
#include "LuaScriptMemoryPool.h"

// Fixed, always-copied shared component (CPP_SOURCE_FILES_FIXED) - must stay JUCE-free
// so any example can pull it in without depending on the GUI layer.

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
 * Owns a pool-allocated Lua state and the sol2 bindings shared by every Lua-scripted
 * example: MIDI/start-stop handler dispatch and the UI-parameter-slot system. notify*()
 * and notifyUiParameterChanged() are audio-thread-safe: they read only numeric fields
 * out of Lua tables and never throw, so nothing beyond the fixed script arena allocates.
 * loadScript() recompiles the script in place and is not real-time safe (it may touch
 * the process heap) - callers apply a pending script off the audio thread, or accept the
 * one-off cost of doing so on it.
 *
 * Derived must be crafted as `class Derived : public LuaScriptEngineBase<Derived>` and
 * provide a private `void bindScriptFunctions()`, called after every successful
 * loadScript() to bind whatever entry points it adds on top of the common set (e.g. a
 * sequencer's NextNotes/OnTiming); it must also grant this base a friend declaration.
 */
template <typename Derived>
class LuaScriptEngineBase
{
  public:
    static constexpr size_t kMaxLuaParams{8};
    using UiParamSlots = std::array<LuaUiParamSlot, kMaxLuaParams>;

    // The handler stubs shared by every Lua-scripted engine, meant to be concatenated
    // with a derived engine's own hooks (e.g. a sequencer's OnTiming/NextNotes) into that
    // engine's own "reset to full skeleton" script. Delete anything not needed; an
    // undefined function is simply never called.
    // clang-format off
    static constexpr std::string_view kCommonSkeletonScript =
"-- Register any dynamic UI parameters you want automation/host control over. Each entry\n"
"-- becomes a knob/dropdown/switch in the plugin UI; changing one fires the matching\n"
"-- On<Id>Changed(value) handler. Uncomment and adjust to try it:\n"
"-- UICreateParameterSet({\n"
"--     { id = \"depth\", name = \"Depth\", type = \"knob\", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0.5 },\n"
"--     { id = \"mode\", name = \"Mode\", type = \"drop\", items = { \"A\", \"B\", \"C\" }, default = 0 },\n"
"--     { id = \"enabled\", name = \"Enabled\", type = \"switch\", default = 0 },\n"
"-- })\n"
"--\n"
"-- function OnDepthChanged(value)\n"
"-- end\n"
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

    explicit LuaScriptEngineBase(size_t poolBytes);

    bool loadScript(std::string_view source);

    // Fires on any effective start/stop transition (manual toggle, or host transport
    // when synced).
    void notifyStart() noexcept;
    void notifyStop() noexcept;

    // MIDI event dispatch - each maps to an optional Lua handler (OnNoteOn, OnNoteOff,
    // OnCC, OnProgramChange, OnAftertouch, OnPolyPressure, OnPitchBend); a script that
    // doesn't define one simply never gets called for that event. channel is 0-based
    // (0..15).
    void notifyNoteOn(int channel, int noteHeight, int velocity) noexcept;
    void notifyNoteOff(int channel, int noteHeight, int velocity) noexcept;
    void notifyCC(int channel, int ccNumber, int value) noexcept;
    void notifyProgramChange(int channel, int program) noexcept;
    void notifyAftertouch(int channel, int value) noexcept;
    void notifyPolyPressure(int channel, int noteHeight, int value) noexcept;
    void notifyPitchBend(int channel, int bendValue) noexcept;

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
    // pacing. Intended for a host to call off the audio thread at a safe idle moment.
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

  protected:
    // Shared body for every optional-handler dispatch: no-op if the script didn't
    // define this handler, catches anything the call throws, and clears/sets
    // m_lastError to reflect this call only.
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

    LuaScriptMemoryPool m_pool;
    std::unique_ptr<lua_State, LuaStateDeleter> m_state;
    sol::state_view m_lua;
    std::string m_lastError;

  private:
    void bindFunctions();
    void bindApiFunctions();
    void registerUiParameterSet(const sol::table& descriptors);
    [[nodiscard]] static LuaUiParamSlot parseUiParamSlot(const sol::table& entry, size_t index);
    [[nodiscard]] static std::string capitalizeFirst(std::string_view text);
    // Maps raw 0..1 through a slot's display range (mirrors LuaControlArea.h's
    // juce::NormalisableRange use, reimplemented here since this file stays JUCE-free).
    [[nodiscard]] static float mapNormalizedToDisplay(const LuaUiParamSlot& slot, float normalized) noexcept;

    sol::protected_function m_onNoteOnFn;
    sol::protected_function m_onNoteOffFn;
    sol::protected_function m_onCcFn;
    sol::protected_function m_onProgramChangeFn;
    sol::protected_function m_onAftertouchFn;
    sol::protected_function m_onPolyPressureFn;
    sol::protected_function m_onPitchBendFn;
    sol::protected_function m_onStartFn;
    sol::protected_function m_onStopFn;

    UiParamSlots m_uiParamSlots{};
    UiParamSlots m_pendingUiParamSlots{};
    std::array<sol::protected_function, kMaxLuaParams> m_uiParamChangedFns{};
};

template <typename Derived>
LuaScriptEngineBase<Derived>::LuaScriptEngineBase(const size_t poolBytes)
    : m_pool(poolBytes)
    , m_state(lua_newstate(&LuaScriptMemoryPool::luaAlloc, &m_pool))
    , m_lua(m_state.get())
{
    assert(m_state != nullptr);
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
    bindApiFunctions();
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindApiFunctions()
{
    m_lua.set_function("UICreateParameterSet", &LuaScriptEngineBase::registerUiParameterSet, this);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindFunctions()
{
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

    static_cast<Derived*>(this)->bindScriptFunctions();
}

template <typename Derived>
bool LuaScriptEngineBase<Derived>::loadScript(const std::string_view source)
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

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyStart() noexcept
{
    callHandler(m_onStartFn);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyStop() noexcept
{
    callHandler(m_onStopFn);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyNoteOn(const int channel, const int noteHeight, const int velocity) noexcept
{
    callHandler(m_onNoteOnFn, channel, noteHeight, velocity);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyNoteOff(const int channel, const int noteHeight, const int velocity) noexcept
{
    callHandler(m_onNoteOffFn, channel, noteHeight, velocity);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyCC(const int channel, const int ccNumber, const int value) noexcept
{
    callHandler(m_onCcFn, channel, ccNumber, value);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyProgramChange(const int channel, const int program) noexcept
{
    callHandler(m_onProgramChangeFn, channel, program);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyAftertouch(const int channel, const int value) noexcept
{
    callHandler(m_onAftertouchFn, channel, value);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyPolyPressure(const int channel, const int noteHeight, const int value) noexcept
{
    callHandler(m_onPolyPressureFn, channel, noteHeight, value);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyPitchBend(const int channel, const int bendValue) noexcept
{
    callHandler(m_onPitchBendFn, channel, bendValue);
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyUiParameterChanged(const size_t slot, const float value) noexcept
{
    if (slot >= kMaxLuaParams)
    {
        return;
    }
    callHandler(m_uiParamChangedFns[slot], mapNormalizedToDisplay(m_uiParamSlots[slot], value));
}

template <typename Derived>
float LuaScriptEngineBase<Derived>::mapNormalizedToDisplay(const LuaUiParamSlot& slot, const float normalized) noexcept
{
    return luaParamNormalizedToDisplay(slot.rangeMin, slot.rangeMax, slot.rangeStep, slot.rangeSkew, normalized);
}

template <typename Derived>
std::string LuaScriptEngineBase<Derived>::capitalizeFirst(const std::string_view text)
{
    std::string result{text};
    if (!result.empty())
    {
        result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
    }
    return result;
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::registerUiParameterSet(const sol::table& descriptors)
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

template <typename Derived>
LuaUiParamSlot LuaScriptEngineBase<Derived>::parseUiParamSlot(const sol::table& entry, const size_t index)
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
