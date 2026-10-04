#pragma once

#define SOL_USING_CXX_LUA 1

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sol/sol.hpp>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "LuaMusicMathLib.h"
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

// Result of resolving one `import "name"` line. On failure, notFoundDetail is a
// human-readable description of where the resolver looked (e.g. the paths it checked),
// folded into the resulting compile error - free-standing (not nested in the template)
// so callers can name it without a template argument.
struct ImportLookup
{
    std::optional<std::string> source;
    std::string notFoundDetail;
};

// Strips an optional trailing ".lua" and validates what remains; nullopt if the resulting
// identifier is empty or contains anything but letters/digits/'_'/'-'. Free-standing (not
// a template member, even though it originates from import-line parsing) so any caller
// that needs "is this a legal library name" - e.g. FileIo's write path - can reuse the
// exact same rule import resolution itself uses, rather than drifting from a duplicate.
[[nodiscard]] inline std::optional<std::string> normalizeImportName(std::string_view rawName)
{
    constexpr std::string_view kLuaSuffix = ".lua";
    if (rawName.ends_with(kLuaSuffix))
    {
        rawName.remove_suffix(kLuaSuffix.size());
    }
    const bool validName =
        !rawName.empty() &&
        std::ranges::all_of(rawName, [](const char c)
                            { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-'; });
    return validName ? std::optional{std::string(rawName)} : std::nullopt;
}

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

    static constexpr float kDefaultPitchGranularityMs{100.f};
    static constexpr float kMinPitchGranularityMs{5.f};
    static constexpr float kMaxPitchGranularityMs{2000.f};

    // Resolves an `import "name"` line (see loadScript()) to that library's Lua source.
    // Left unset, every import fails to resolve - a Derived that never wires one up
    // simply never supports imports.
    using ImportResolver = std::function<ImportLookup(std::string_view)>;

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
"-- Feeds from your impl's feedPitchAnalysis() (YIN), firing every analysis hop - see\n"
"-- setPitchAnalysisGranularity(), default 100 ms. confidence is 0..1 (0 = no reliable\n"
"-- pitch). Pitch.Hz()/Pitch.Confidence() read the same values on demand. Uncomment to\n"
"-- try it:\n"
"-- function OnPitchDetected(hz, confidence)\n"
"-- end\n"
"\n"
"-- Timer.After(ms, fn) fires fn once; Timer.Every(ms, fn) repeats. Both return an id you\n"
"-- can pass to Timer.Cancel(id). Uncomment to try it:\n"
"-- Timer.Every(500, function()\n"
"-- end)\n"
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
"end\n"
"\n"
"-- Fire on a change in the host's own playhead/transport (Transport.* below), independent\n"
"-- of OnStart/OnStop above - which follow the plugin's Play switch or Host Sync setting,\n"
"-- not the host's raw transport state.\n"
"function OnTempoChanged(bpm)\n"
"end\n"
"\n"
"function OnTimeSignatureChanged(numerator, denominator)\n"
"end\n"
"\n"
"function OnPlayingStart()\n"
"end\n"
"\n"
"function OnPlayingStop()\n"
"end\n";
    // clang-format on

    explicit LuaScriptEngineBase(size_t poolBytes);

    void setImportResolver(ImportResolver resolver)
    {
        m_importResolver = std::move(resolver);
    }

    // Splices any leading `import "name"` lines (see LUA.md) into `source` via the
    // configured ImportResolver before compiling. An unresolvable import rejects the
    // whole script, the same as any other compile failure.
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

    // Used by Timer.After/Timer.Every and to (re)build the pitch detector; call once after
    // construction (and again if the sample rate changes). Not real-time safe - call
    // during setup, never mid-stream.
    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
        rebuildPitchDetector();
    }

    // Not real-time safe, same reason as setSampleRate() above: rebuilds the detector.
    // ms is clamped to [kMinPitchGranularityMs, kMaxPitchGranularityMs].
    void setPitchAnalysisGranularity(const float granularityMs)
    {
        m_pitchGranularityMs = std::clamp(granularityMs, kMinPitchGranularityMs, kMaxPitchGranularityMs);
        rebuildPitchDetector();
    }

    // Real-time safe: steps the YIN detector sample-by-sample and fires
    // OnPitchDetected(hz, confidence) through callHandler() on every hop it completes. A
    // no-op until setSampleRate() has run at least once.
    void feedPitchAnalysis(std::span<const float> block) noexcept;

    [[nodiscard]] float currentPitchHz() const noexcept
    {
        return m_currentPitchHz;
    }

    [[nodiscard]] float currentPitchConfidence() const noexcept
    {
        return m_currentPitchConfidence;
    }

    // Advances every active Timer.After/Timer.Every slot by numSamples and fires any that
    // come due, through the same callHandler() path as every other script callback -
    // realtime-safe, no allocation. Call once per audio block.
    void tickBlock(size_t numSamples) noexcept;

    // Updates the Transport.* snapshot and fires OnTempoChanged/OnTimeSignatureChanged/
    // OnPlayingStart/OnPlayingStop for whichever of these actually changed since the last
    // call. Realtime-safe, no allocation - intended to be called once per audio block from
    // the same place a host's playhead position is read.
    void notifyTransportSnapshot(double bpm, double timeInQuarterNotes, double timeInSeconds, int timeSigNumerator,
                                 int timeSigDenominator, bool isPlaying, bool isLooping, bool isRecording) noexcept;

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

    enum class StallGuardMode
    {
        Idle,
        WallClockBudget,
        InstructionBudget
    };

    // Wall-clock budget for a top-level loadScript() compile - guards against e.g.
    // `while true do end` at load time hanging the UI/host indefinitely.
    static constexpr std::chrono::milliseconds kLoadStallBudget{500};
    // VM instructions between wall-clock checks while guarding loadScript() - coarse
    // enough that the check's own overhead stays negligible.
    static constexpr int kLoadCheckInstructionInterval{100'000};
    // Pure VM-instruction ceiling for one audio-thread call - realtime-safe (no clock
    // reads); a first-cut default, not yet blueprint-configurable.
    static constexpr int kHandlerInstructionBudget{2'000'000};

    StallGuardMode m_stallGuardMode{StallGuardMode::Idle};
    std::chrono::steady_clock::time_point m_stallDeadline{};

    // Reads back the engine `this` pointer lua_sethook()'s hook stashed in the Lua state's
    // own per-state extra space (LUAI_EXTRASPACE, Lua 5.4) - the hook itself is a free
    // function and only ever receives a lua_State*, no user-data slot of its own.
    static void stallHookTrampoline(lua_State* L, lua_Debug*)
    {
        auto* self = *static_cast<LuaScriptEngineBase**>(lua_getextraspace(L));
        self->onStallHookTripped(L);
    }

    // Aborts the in-flight Lua call by raising a Lua error - unwinds as a C++ exception
    // (SOL_USING_CXX_LUA=1) into the caller's existing try/catch around the guarded call,
    // the same path any other script error already takes.
    void onStallHookTripped(lua_State* L) const
    {
        if (m_stallGuardMode == StallGuardMode::WallClockBudget)
        {
            if (std::chrono::steady_clock::now() < m_stallDeadline)
            {
                return;
            }
            luaL_error(L, "script exceeded %dms while loading (possible infinite loop)",
                       static_cast<int>(kLoadStallBudget.count()));
            return;
        }
        luaL_error(L, "script call exceeded %d Lua instructions (possible infinite loop)", kHandlerInstructionBudget);
    }

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
            const auto stallGuard = guardHandlerCall();
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

    // Bounds exactly one Lua call (top-level loadScript(), or one handler/per-tick
    // dispatch such as callHandler() or a Derived's own NextNotes()-style entry point)
    // against an infinite loop, via Lua's own debug-hook facility - not a redesign of
    // script execution, just a watchdog wrapped around the existing call. RAII: the guard
    // is cleared on scope exit whether the call returns normally or raises, so a tripped
    // guard never lingers into whatever runs next on this Lua state. Construct via
    // guardLoadCall()/guardHandlerCall() below rather than directly.
    class ScopedStallGuard
    {
      public:
        ScopedStallGuard(LuaScriptEngineBase& engine, const StallGuardMode mode, const int instructionCount) noexcept
            : m_engine(engine)
        {
            m_engine.m_stallGuardMode = mode;
            if (mode == StallGuardMode::WallClockBudget)
            {
                m_engine.m_stallDeadline = std::chrono::steady_clock::now() + kLoadStallBudget;
            }
            lua_sethook(m_engine.m_state.get(), &stallHookTrampoline, LUA_MASKCOUNT, instructionCount);
        }

        ~ScopedStallGuard()
        {
            lua_sethook(m_engine.m_state.get(), nullptr, 0, 0);
            m_engine.m_stallGuardMode = StallGuardMode::Idle;
        }

        ScopedStallGuard(const ScopedStallGuard&) = delete;
        ScopedStallGuard& operator=(const ScopedStallGuard&) = delete;

      private:
        LuaScriptEngineBase& m_engine;
    };

    // Message-thread guard for loadScript(): a generous wall-clock budget, checked every
    // kLoadCheckInstructionInterval VM instructions rather than every single one.
    [[nodiscard]] ScopedStallGuard guardLoadCall() noexcept
    {
        return ScopedStallGuard(*this, StallGuardMode::WallClockBudget, kLoadCheckInstructionInterval);
    }

    // Audio-thread guard for one handler/per-tick dispatch: a pure VM-instruction ceiling,
    // no clock reads, to stay realtime-safe.
    [[nodiscard]] ScopedStallGuard guardHandlerCall() noexcept
    {
        return ScopedStallGuard(*this, StallGuardMode::InstructionBudget, kHandlerInstructionBudget);
    }

    LuaScriptMemoryPool m_pool;
    std::unique_ptr<lua_State, LuaStateDeleter> m_state;
    sol::state_view m_lua;
    std::string m_lastError;

  private:
    static constexpr size_t kMaxLuaTimers{8};

    // One Timer.After/Timer.Every slot. callback is a closure the script itself passed in
    // (unlike every other handler here, which is looked up by name), so it is cleared on
    // every loadScript() rather than surviving a reload.
    struct LuaTimerSlot
    {
        sol::protected_function callback;
        size_t periodSamples{0};
        size_t remainingSamples{0};
        bool repeating{false};
        bool active{false};
    };

    struct LuaTransportSnapshot
    {
        double bpm{120.0};
        double timeInQuarterNotes{0.0};
        double timeInSeconds{0.0};
        int timeSigNumerator{4};
        int timeSigDenominator{4};
        bool isPlaying{false};
        bool isLooping{false};
        bool isRecording{false};
    };

    void bindFunctions();
    void bindApiFunctions();
    // Global names the outgoing script defined (diffed in loadScript()); nil'd out before
    // the next script runs, so one that doesn't redefine a handler doesn't inherit a stale one.
    [[nodiscard]] std::unordered_set<std::string> collectStringGlobalKeys() const;
    std::unordered_set<std::string> m_previousScriptGlobalKeys;
    void bindMusicMathLibrary();
    void bindTimerApi();
    void bindTransportApi();
    void bindPitchApi();
    // (Re)builds m_pitchDetector from the current sample rate and granularity; called by
    // setSampleRate() and setPitchAnalysisGranularity(), both documented not real-time safe.
    void rebuildPitchDetector();
    // Returns the spliced script ready to compile, or nullopt (with m_lastError set and
    // logged to std::cerr) if a leading import line names a library that fails to resolve.
    [[nodiscard]] std::optional<std::string> resolveImports(std::string_view source);
    // Extracts the quoted name from an already-trimmed `import "..."` line; nullopt if
    // the line isn't shaped like an import directive at all (left as literal script text).
    [[nodiscard]] static std::optional<std::string_view> extractImportDirectiveName(
        std::string_view trimmedLine) noexcept;
    [[nodiscard]] static std::string_view trimmed(std::string_view text) noexcept;
    void registerUiParameterSet(const sol::table& descriptors);
    [[nodiscard]] static LuaUiParamSlot parseUiParamSlot(const sol::table& entry, size_t index);
    [[nodiscard]] static std::string capitalizeFirst(std::string_view text);
    // Maps raw 0..1 through a slot's display range (mirrors LuaControlArea.h's
    // juce::NormalisableRange use, reimplemented here since this file stays JUCE-free).
    [[nodiscard]] static float mapNormalizedToDisplay(const LuaUiParamSlot& slot, float normalized) noexcept;

    // Claims a free timer slot for Timer.After/Timer.Every; returns a 1-based id for
    // Timer.Cancel, or 0 if every slot is already in use. Not noexcept: called only from a
    // sol2-bound Lua function, which already catches and converts any C++ exception into a
    // Lua error, the same as registerUiParameterSet() above.
    [[nodiscard]] int scheduleTimer(double ms, sol::protected_function callback, bool repeating);
    void cancelTimer(int id) noexcept;
    [[nodiscard]] size_t msToSamples(double ms) const noexcept;

    sol::protected_function m_onNoteOnFn;
    sol::protected_function m_onNoteOffFn;
    sol::protected_function m_onCcFn;
    sol::protected_function m_onProgramChangeFn;
    sol::protected_function m_onAftertouchFn;
    sol::protected_function m_onPolyPressureFn;
    sol::protected_function m_onPitchBendFn;
    sol::protected_function m_onStartFn;
    sol::protected_function m_onStopFn;
    sol::protected_function m_onTempoChangedFn;
    sol::protected_function m_onTimeSignatureChangedFn;
    sol::protected_function m_onPlayingStartFn;
    sol::protected_function m_onPlayingStopFn;
    sol::protected_function m_onPitchDetectedFn;

    UiParamSlots m_uiParamSlots{};
    UiParamSlots m_pendingUiParamSlots{};
    std::array<sol::protected_function, kMaxLuaParams> m_uiParamChangedFns{};

    float m_sampleRate{44100.f};
    std::array<LuaTimerSlot, kMaxLuaTimers> m_timerSlots{};
    LuaTransportSnapshot m_transport{};
    ImportResolver m_importResolver;

    // YinPitchDetector's own defaults - kept as named constants here since they bound
    // what "pitch" means for every script (see class doc caveats).
    static constexpr float kPitchMinFreqHz{80.f};
    static constexpr float kPitchMaxFreqHz{1000.f};
    float m_pitchGranularityMs{kDefaultPitchGranularityMs};
    float m_currentPitchHz{0.f};
    float m_currentPitchConfidence{0.f};
    std::optional<AbacDsp::YinPitchDetector> m_pitchDetector;
};

template <typename Derived>
LuaScriptEngineBase<Derived>::LuaScriptEngineBase(const size_t poolBytes)
    : m_pool(poolBytes)
    , m_state(lua_newstate(&LuaScriptMemoryPool::luaAlloc, &m_pool))
    , m_lua(m_state.get())
{
    assert(m_state != nullptr);
    *static_cast<LuaScriptEngineBase**>(lua_getextraspace(m_state.get())) = this;
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
    bindApiFunctions();
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindApiFunctions()
{
    m_lua.set_function("UICreateParameterSet", &LuaScriptEngineBase::registerUiParameterSet, this);
    bindMusicMathLibrary();
    bindTimerApi();
    bindTransportApi();
    bindPitchApi();
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindMusicMathLibrary()
{
    sol::table music = m_lua.create_table();
    music.set_function("NoteToHz", sol::overload([](const float note) { return LuaMusicMath::noteToHz(note); },
                                                 [](const float note, const float tuning)
                                                 { return LuaMusicMath::noteToHz(note, tuning); }));
    music.set_function("HzToNote", sol::overload([](const float hz) { return LuaMusicMath::hzToNote(hz); },
                                                 [](const float hz, const float tuning)
                                                 { return LuaMusicMath::hzToNote(hz, tuning); }));
    music.set_function("IntervalToRatio", &LuaMusicMath::intervalToRatio);
    music.set_function("RatioToInterval", &LuaMusicMath::ratioToInterval);

    music.set_function("Harmonics",
                       [this](const float fundamentalHz, const size_t count)
                       {
                           std::array<float, LuaMusicMath::kMaxHarmonics> buffer{};
                           const size_t written = LuaMusicMath::harmonicSeries(fundamentalHz, count, buffer);
                           sol::table table = m_lua.create_table(static_cast<int>(written), 0);
                           for (size_t i = 0; i < written; ++i)
                           {
                               table[i + 1] = buffer[i];
                           }
                           return table;
                       });

    // Linear scan over kScales (a handful of entries) is simpler than a name->index map
    // for a function only ever called from script logic, not the audio-thread hot path.
    music.set_function("HarmonizeToScale",
                       [](const float note, const float root, const std::string& scaleName)
                       {
                           for (const auto& entry : LuaMusicMath::kScales)
                           {
                               if (entry.name == scaleName)
                               {
                                   return LuaMusicMath::harmonizeToScale(note, root, entry.intervals);
                               }
                           }
                           return note;
                       });

    const auto intervalsToLuaTable = [this](const std::span<const int> intervals)
    {
        sol::table table = m_lua.create_table(static_cast<int>(intervals.size()), 0);
        for (size_t i = 0; i < intervals.size(); ++i)
        {
            table[i + 1] = intervals[i];
        }
        return table;
    };

    sol::table scales = m_lua.create_table();
    for (const auto& entry : LuaMusicMath::kScales)
    {
        scales[entry.name] = intervalsToLuaTable(entry.intervals);
    }
    music["Scales"] = scales;

    sol::table chords = m_lua.create_table();
    for (const auto& entry : LuaMusicMath::kChords)
    {
        chords[entry.name] = intervalsToLuaTable(entry.intervals);
    }
    music["Chords"] = chords;
    m_lua["Music"] = music;

    sol::table vel = m_lua.create_table();
    vel.set_function("Exponential",
                     sol::overload([](const float v) { return LuaMusicMath::velocityToGainExponential(v); },
                                   [](const float v, const float curve)
                                   { return LuaMusicMath::velocityToGainExponential(v, curve); }));
    vel.set_function("Cubic", &LuaMusicMath::velocityToGainCubic);
    m_lua["Vel"] = vel;

    sol::table rr = m_lua.create_table();
    rr.set_function("Next", &LuaMusicMath::toroidIncrement);
    rr.set_function("Advance", &LuaMusicMath::toroidAdvance);
    m_lua["Rr"] = rr;

    sol::table rhythm = m_lua.create_table();
    sol::table noteValues = m_lua.create_table();
    for (const auto& entry : LuaMusicMath::kNoteValues)
    {
        noteValues[entry.name] = entry.beats;
    }
    rhythm["NoteValues"] = noteValues;
    rhythm.set_function("BeatsToMs", &LuaMusicMath::beatsToMs);
    m_lua["Rhythm"] = rhythm;
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindTimerApi()
{
    sol::table timer = m_lua.create_table();
    timer.set_function("After", [this](const double ms, sol::protected_function callback)
                       { return scheduleTimer(ms, std::move(callback), false); });
    timer.set_function("Every", [this](const double ms, sol::protected_function callback)
                       { return scheduleTimer(ms, std::move(callback), true); });
    timer.set_function("Cancel", [this](const int id) { cancelTimer(id); });
    m_lua["Timer"] = timer;
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindTransportApi()
{
    sol::table transport = m_lua.create_table();
    transport.set_function("TimeInSeconds", [this] { return m_transport.timeInSeconds; });
    transport.set_function("TimeInQuarterNotes", [this] { return m_transport.timeInQuarterNotes; });
    transport.set_function("Tempo", [this] { return m_transport.bpm; });
    transport.set_function("TimeSignature", [this]
                           { return std::make_tuple(m_transport.timeSigNumerator, m_transport.timeSigDenominator); });
    transport.set_function("PlayingState", [this] { return m_transport.isPlaying; });
    transport.set_function("LoopingState", [this] { return m_transport.isLooping; });
    transport.set_function("RecordingState", [this] { return m_transport.isRecording; });
    m_lua["Transport"] = transport;
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::bindPitchApi()
{
    sol::table pitch = m_lua.create_table();
    pitch.set_function("Hz", [this] { return m_currentPitchHz; });
    pitch.set_function("Confidence", [this] { return m_currentPitchConfidence; });
    m_lua["Pitch"] = pitch;
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::rebuildPitchDetector()
{
    m_pitchDetector.emplace(m_sampleRate, kPitchMinFreqHz, kPitchMaxFreqHz, 1000.f / m_pitchGranularityMs);
}

template <typename Derived>
int LuaScriptEngineBase<Derived>::scheduleTimer(const double ms, sol::protected_function callback, const bool repeating)
{
    for (size_t i = 0; i < kMaxLuaTimers; ++i)
    {
        if (m_timerSlots[i].active)
        {
            continue;
        }
        const size_t period = msToSamples(ms);
        m_timerSlots[i] = LuaTimerSlot{std::move(callback), period, period, repeating, true};
        return static_cast<int>(i) + 1;
    }
    return 0;
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::cancelTimer(const int id) noexcept
{
    if (id < 1 || static_cast<size_t>(id) > kMaxLuaTimers)
    {
        return;
    }
    m_timerSlots[static_cast<size_t>(id) - 1].active = false;
}

template <typename Derived>
size_t LuaScriptEngineBase<Derived>::msToSamples(const double ms) const noexcept
{
    return std::max<size_t>(1, static_cast<size_t>(ms * static_cast<double>(m_sampleRate) * 0.001));
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::tickBlock(const size_t numSamples) noexcept
{
    for (auto& slot : m_timerSlots)
    {
        if (!slot.active)
        {
            continue;
        }
        if (slot.remainingSamples > numSamples)
        {
            slot.remainingSamples -= numSamples;
            continue;
        }
        callHandler(slot.callback);
        if (slot.repeating)
        {
            slot.remainingSamples = slot.periodSamples;
        }
        else
        {
            slot.active = false;
        }
    }
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::feedPitchAnalysis(const std::span<const float> block) noexcept
{
    if (!m_pitchDetector)
    {
        return;
    }
    for (const float sample : block)
    {
        const float pitch = m_pitchDetector->step(sample);
        if (m_pitchDetector->hasNewPitch())
        {
            m_currentPitchHz = pitch;
            m_currentPitchConfidence = m_pitchDetector->getLastConfidence();
            callHandler(m_onPitchDetectedFn, m_currentPitchHz, m_currentPitchConfidence);
        }
    }
}

template <typename Derived>
void LuaScriptEngineBase<Derived>::notifyTransportSnapshot(const double bpm, const double timeInQuarterNotes,
                                                           const double timeInSeconds, const int timeSigNumerator,
                                                           const int timeSigDenominator, const bool isPlaying,
                                                           const bool isLooping, const bool isRecording) noexcept
{
    m_transport.timeInQuarterNotes = timeInQuarterNotes;
    m_transport.timeInSeconds = timeInSeconds;
    m_transport.isLooping = isLooping;
    m_transport.isRecording = isRecording;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (bpm != m_transport.bpm)
    {
        m_transport.bpm = bpm;
        callHandler(m_onTempoChangedFn, bpm);
    }
#pragma GCC diagnostic pop

    if (timeSigNumerator != m_transport.timeSigNumerator || timeSigDenominator != m_transport.timeSigDenominator)
    {
        m_transport.timeSigNumerator = timeSigNumerator;
        m_transport.timeSigDenominator = timeSigDenominator;
        callHandler(m_onTimeSignatureChangedFn, timeSigNumerator, timeSigDenominator);
    }

    if (isPlaying != m_transport.isPlaying)
    {
        m_transport.isPlaying = isPlaying;
        if (isPlaying)
        {
            callHandler(m_onPlayingStartFn);
        }
        else
        {
            callHandler(m_onPlayingStopFn);
        }
    }
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
    m_onTempoChangedFn = m_lua["OnTempoChanged"];
    m_onTimeSignatureChangedFn = m_lua["OnTimeSignatureChanged"];
    m_onPlayingStartFn = m_lua["OnPlayingStart"];
    m_onPlayingStopFn = m_lua["OnPlayingStop"];
    m_onPitchDetectedFn = m_lua["OnPitchDetected"];

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
std::unordered_set<std::string> LuaScriptEngineBase<Derived>::collectStringGlobalKeys() const
{
    std::unordered_set<std::string> keys;
    for (const auto& entry : m_lua.globals())
    {
        if (entry.first.template is<std::string>())
        {
            keys.insert(entry.first.template as<std::string>());
        }
    }
    return keys;
}

template <typename Derived>
bool LuaScriptEngineBase<Derived>::loadScript(const std::string_view source)
{
    m_pendingUiParamSlots = UiParamSlots{};
    // Timer callbacks are closures over the outgoing script's environment (unlike the
    // named On* handlers, which bindFunctions() below simply re-looks-up) - they must not
    // keep firing into a script that no longer exists.
    m_timerSlots = std::array<LuaTimerSlot, kMaxLuaTimers>{};

    // m_lua is shared across every loadScript() call - a global the outgoing script defined
    // would otherwise survive into a script that never redefines it, e.g. a handler firing
    // from code that's no longer active.
    for (const auto& key : m_previousScriptGlobalKeys)
    {
        m_lua.globals()[key] = sol::lua_nil;
    }
    m_previousScriptGlobalKeys.clear();

    const std::optional<std::string> resolvedSource = resolveImports(source);
    if (!resolvedSource)
    {
        return false;
    }

    const auto globalsBefore = collectStringGlobalKeys();
    try
    {
        const auto stallGuard = guardLoadCall();
        sol::protected_function_result result = m_lua.safe_script(*resolvedSource, sol::script_pass_on_error);
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
    for (const auto& key : collectStringGlobalKeys())
    {
        if (!globalsBefore.contains(key))
        {
            m_previousScriptGlobalKeys.insert(key);
        }
    }
    m_lastError.clear();
    bindFunctions();
    return true;
}

template <typename Derived>
std::optional<std::string> LuaScriptEngineBase<Derived>::resolveImports(const std::string_view source)
{
    const auto fail = [this](std::string message) -> std::optional<std::string>
    {
        m_lastError = std::move(message);
        std::cerr << "LuaScriptEngine: ERROR - " << m_lastError << std::endl;
        return std::nullopt;
    };

    std::string spliced;
    size_t headerEnd = 0;
    while (headerEnd < source.size())
    {
        const size_t newline = source.find('\n', headerEnd);
        const size_t lineEnd = (newline == std::string_view::npos) ? source.size() : newline;
        const std::string_view line = trimmed(source.substr(headerEnd, lineEnd - headerEnd));
        const size_t nextHeaderEnd = (newline == std::string_view::npos) ? source.size() : newline + 1;

        if (line.empty() || line.starts_with("--"))
        {
            headerEnd = nextHeaderEnd;
            continue;
        }

        const std::optional<std::string_view> rawName = extractImportDirectiveName(line);
        if (!rawName)
        {
            break;
        }
        const std::optional<std::string> name = normalizeImportName(*rawName);
        if (!name)
        {
            return fail("import \"" + std::string(*rawName) +
                        "\": invalid library name - use letters, digits, '_' or '-', "
                        "optionally with a trailing .lua");
        }

        const ImportLookup lookup = m_importResolver ? m_importResolver(*name) : ImportLookup{};
        if (!lookup.source)
        {
            std::string message = "import \"" + *name + "\": library script not found";
            if (!lookup.notFoundDetail.empty())
            {
                message += " (" + lookup.notFoundDetail + ")";
            }
            return fail(std::move(message));
        }
        spliced += *lookup.source;
        spliced += '\n';
        headerEnd = nextHeaderEnd;
    }
    spliced += source.substr(headerEnd);
    return spliced;
}

template <typename Derived>
std::optional<std::string_view> LuaScriptEngineBase<Derived>::extractImportDirectiveName(
    const std::string_view trimmedLine) noexcept
{
    constexpr std::string_view kPrefix = "import \"";
    if (!trimmedLine.starts_with(kPrefix) || !trimmedLine.ends_with('"') || trimmedLine.size() <= kPrefix.size())
    {
        return std::nullopt;
    }
    return trimmedLine.substr(kPrefix.size(), trimmedLine.size() - kPrefix.size() - 1);
}

template <typename Derived>
std::string_view LuaScriptEngineBase<Derived>::trimmed(std::string_view text) noexcept
{
    const auto isSpace = [](const char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while (!text.empty() && isSpace(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
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
