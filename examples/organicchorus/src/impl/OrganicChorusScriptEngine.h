#pragma once

#include <string>
#include <string_view>

#include "../inc/LuaScriptEngineBase.h"

// Adds nothing on top of LuaScriptEngineBase's shared MIDI/UI-parameter machinery: the
// two extra live-tweak knobs this example exposes (Drift, Spread) are plain
// UICreateParameterSet slots feeding OrganicChorusImpl::setLuaParam1/2 directly, with no
// custom bound function needed.
class OrganicChorusScriptEngine : public LuaScriptEngineBase<OrganicChorusScriptEngine>
{
  public:
    // clang-format off
    static constexpr std::string_view kStubScript =
"-- Organic Chorus script - claims the two extra live-tweak knobs beyond the six main\n"
"-- macros. Unclaimed slots simply don't show up in the Lua Controls area.\n"
"UICreateParameterSet({\n"
"    { id = \"drift\", name = \"Drift\", type = \"knob\", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0.5,\n"
"      description = \"Real mechanical speed wander on the tape clock, independent of Depth\" },\n"
"    { id = \"spread\", name = \"Spread\", type = \"knob\", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0.5,\n"
"      description = \"Per-voice rate/delay detuning - 0 locks voices together, 1 is maximally independent\" },\n"
"})\n";
    // clang-format on

    static const std::string kFullSkeletonScript;

    explicit OrganicChorusScriptEngine(size_t poolBytes = 256 * 1024);

  private:
    friend class LuaScriptEngineBase<OrganicChorusScriptEngine>;
    void bindScriptFunctions() {}
};

inline const std::string OrganicChorusScriptEngine::kFullSkeletonScript = std::string(kCommonSkeletonScript);

inline OrganicChorusScriptEngine::OrganicChorusScriptEngine(const size_t poolBytes)
    : LuaScriptEngineBase<OrganicChorusScriptEngine>(poolBytes)
{
    loadScript(kStubScript);
}
