#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"
#include "Generators/ExcitationTechnique.h"

struct DroneNote
{
    float noteHeight{0.f};
    float velocity{0.f};
    size_t channel{0};
    float lengthMs{0.f};
    float delayMs{0.f};
    float slideSemitones{0.f}; // signed; 0 = straight pluck, unchanged from today
    float slideTimeMs{0.f};    // only meaningful when slideSemitones != 0
};

struct DroneExcitation
{
    size_t channel{0};
    AbacDsp::ExcitationEvent event;
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
    static constexpr size_t kMaxExcitationsPerRequest{8};

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
"\n"
"-- Excite(channel, params) fires a playing technique on a string at any time - inside\n"
"-- NextNotes(), a MIDI handler, a Timer.After callback - including mid-sustain on an\n"
"-- already-triggered string. params:\n"
"--   type     \"pluck\"/\"strike\"/\"mute\"/\"palmmute\"/\"bow\"/\"sympathetic\"/\"wind\"/\"rub\"\n"
"--   start    ms before the technique begins (measured from this call)\n"
"--   end      ms: window end for most types; for \"mute\", the fade-out duration instead\n"
"--   strength 0..1\n"
"--   harmonic \"sympathetic\" only: n-th harmonic of the string's own fundamental\n"
"-- Excite(0, { type = \"bow\", start = 0, [\"end\"] = 2000, strength = 0.5 })\n"
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

    struct PendingExcitationsResult
    {
        std::array<DroneExcitation, kMaxExcitationsPerRequest> excitations{};
        size_t count{0};
    };
    // Drains (returns and clears) whatever Excite() calls the script made since the last
    // drain - called once per block, same spirit as nextNotes() but not tied to the note
    // lookahead cycle: Excite() can be called from any script context.
    [[nodiscard]] PendingExcitationsResult drainExcitations() noexcept;

  private:
    friend class LuaScriptEngineBase<DroneScriptEngine>;
    void bindScriptFunctions();

    [[nodiscard]] static std::optional<AbacDsp::ExcitationType> parseExcitationType(std::string_view name) noexcept;
    void luaExcite(size_t channel, const sol::table& params);

    sol::protected_function m_nextNotesFn;
    sol::protected_function m_onTimingFn;
    std::array<DroneExcitation, kMaxExcitationsPerRequest> m_pendingExcitations{};
    size_t m_pendingExcitationCount{0};
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
    m_lua.set_function("Excite", &DroneScriptEngine::luaExcite, this);
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
            note.slideSemitones = entry->get_or("slide", 0.f);
            note.slideTimeMs = entry->get_or("slideTime", 0.f);
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

inline std::optional<AbacDsp::ExcitationType> DroneScriptEngine::parseExcitationType(
    const std::string_view name) noexcept
{
    if (name == "pluck")
    {
        return AbacDsp::ExcitationType::Pluck;
    }
    if (name == "strike")
    {
        return AbacDsp::ExcitationType::Strike;
    }
    if (name == "mute")
    {
        return AbacDsp::ExcitationType::Mute;
    }
    if (name == "palmmute")
    {
        return AbacDsp::ExcitationType::PalmMute;
    }
    if (name == "bow")
    {
        return AbacDsp::ExcitationType::Bow;
    }
    if (name == "sympathetic")
    {
        return AbacDsp::ExcitationType::Sympathetic;
    }
    if (name == "wind")
    {
        return AbacDsp::ExcitationType::Wind;
    }
    if (name == "rub")
    {
        return AbacDsp::ExcitationType::Rub;
    }
    return std::nullopt;
}

// Bound as Lua's Excite(channel, params); reads only get_or-defaulted fields, so it stays
// audio-thread-safe like nextNotes()'s own field reads. An unrecognized type, or a full
// pending buffer, drops the call silently rather than raising a script error.
inline void DroneScriptEngine::luaExcite(const size_t channel, const sol::table& params)
{
    if (m_pendingExcitationCount >= kMaxExcitationsPerRequest)
    {
        return;
    }
    const auto type = parseExcitationType(params.get_or<std::string>("type", "pluck"));
    if (!type)
    {
        return;
    }
    AbacDsp::ExcitationEvent event{};
    event.type = *type;
    event.startMs = params.get_or("start", 0.f);
    event.endMs = params.get_or("end", 0.f);
    event.strength = params.get_or("strength", 1.f);
    const sol::optional<float> harmonic = params["harmonic"];
    event.harmonic = harmonic ? std::optional<float>(*harmonic) : std::nullopt;

    m_pendingExcitations[m_pendingExcitationCount] = DroneExcitation{channel, event};
    ++m_pendingExcitationCount;
}

inline DroneScriptEngine::PendingExcitationsResult DroneScriptEngine::drainExcitations() noexcept
{
    PendingExcitationsResult out{};
    out.excitations = m_pendingExcitations;
    out.count = m_pendingExcitationCount;
    m_pendingExcitationCount = 0;
    return out;
}
