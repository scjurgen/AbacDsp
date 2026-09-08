#pragma once

#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

using json = nlohmann::json;

// clang-format off
struct PatchParameters
{
    enum class Id : int
    {
        level           , // dial
        note            , // dial
        play            , // switch
        material        , // dial
        light           , // dial
        motion          , // dial
        breath          , // dial
        stability       , // dial
        bloom           , // dial
        hold            , // switch
        harmonyEnabled  , // switch
        harmonyHome     , // drop
        harmonyCharacter, // drop
        pedalNote       , // dial
        luaParam1       , // dial
        luaParam2       , // dial
        luaParam3       , // dial
        luaParam4       , // dial
        luaParam5       , // dial
        luaParam6       , // dial
        luaParam7       , // dial
        luaParam8       , // dial
        script           // script
    };
float level{-30.0f};
float note{69.0f};
bool play{false};
float material{0.5f};
float light{0.5f};
float motion{0.3f};
float breath{0.3f};
float stability{0.5f};
float bloom{0.3f};
bool hold{false};
bool harmonyEnabled{false};
size_t harmonyHome{4};
size_t harmonyCharacter{0};
float pedalNote{0.0f};
float luaParam1{0.0f};
float luaParam2{0.0f};
float luaParam3{0.0f};
float luaParam4{0.0f};
float luaParam5{0.0f};
float luaParam6{0.0f};
float luaParam7{0.0f};
float luaParam8{0.0f};
std::string script{};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "level",
"note",
"play",
"material",
"light",
"motion",
"breath",
"stability",
"bloom",
"hold",
"harmonyEnabled",
"harmonyHome",
"harmonyCharacter",
"pedalNote",
"luaParam1",
"luaParam2",
"luaParam3",
"luaParam4",
"luaParam5",
"luaParam6",
"luaParam7",
"luaParam8"
    });
//        "onOff", "patch", "input", "modulationDepth", "mix", "density", "threshold", "knee"});

    static size_t count()
    {
        return paramNames.size();
    }

    template<Id ParamId>
    [[nodiscard]] const auto& get() const
    {
        return const_cast<std::remove_const_t<decltype(std::as_const(*this).get<ParamId>())>&>(
            std::as_const(*this).get<ParamId>());
    }

    template<Id ParamId>
    auto& get()
    {
        if constexpr (ParamId == Id::level) return level;
        else if constexpr (ParamId == Id::note) return note;
        else if constexpr (ParamId == Id::play) return play;
        else if constexpr (ParamId == Id::material) return material;
        else if constexpr (ParamId == Id::light) return light;
        else if constexpr (ParamId == Id::motion) return motion;
        else if constexpr (ParamId == Id::breath) return breath;
        else if constexpr (ParamId == Id::stability) return stability;
        else if constexpr (ParamId == Id::bloom) return bloom;
        else if constexpr (ParamId == Id::hold) return hold;
        else if constexpr (ParamId == Id::harmonyEnabled) return harmonyEnabled;
        else if constexpr (ParamId == Id::harmonyHome) return harmonyHome;
        else if constexpr (ParamId == Id::harmonyCharacter) return harmonyCharacter;
        else if constexpr (ParamId == Id::pedalNote) return pedalNote;
        else if constexpr (ParamId == Id::luaParam1) return luaParam1;
        else if constexpr (ParamId == Id::luaParam2) return luaParam2;
        else if constexpr (ParamId == Id::luaParam3) return luaParam3;
        else if constexpr (ParamId == Id::luaParam4) return luaParam4;
        else if constexpr (ParamId == Id::luaParam5) return luaParam5;
        else if constexpr (ParamId == Id::luaParam6) return luaParam6;
        else if constexpr (ParamId == Id::luaParam7) return luaParam7;
        else if constexpr (ParamId == Id::luaParam8) return luaParam8;
        else if constexpr (ParamId == Id::script) return script;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::level: if (!isEqual(get<Id::level>(), value)) {get<Id::level>() = value;m_modified = true;}
break;
 case Id::note: if (!isEqual(get<Id::note>(), value)) {get<Id::note>() = value;m_modified = true;}
break;
 case Id::play: if (!isEqual(get<Id::play>(), value)) {get<Id::play>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::material: if (!isEqual(get<Id::material>(), value)) {get<Id::material>() = value;m_modified = true;}
break;
 case Id::light: if (!isEqual(get<Id::light>(), value)) {get<Id::light>() = value;m_modified = true;}
break;
 case Id::motion: if (!isEqual(get<Id::motion>(), value)) {get<Id::motion>() = value;m_modified = true;}
break;
 case Id::breath: if (!isEqual(get<Id::breath>(), value)) {get<Id::breath>() = value;m_modified = true;}
break;
 case Id::stability: if (!isEqual(get<Id::stability>(), value)) {get<Id::stability>() = value;m_modified = true;}
break;
 case Id::bloom: if (!isEqual(get<Id::bloom>(), value)) {get<Id::bloom>() = value;m_modified = true;}
break;
 case Id::hold: if (!isEqual(get<Id::hold>(), value)) {get<Id::hold>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::harmonyEnabled: if (!isEqual(get<Id::harmonyEnabled>(), value)) {get<Id::harmonyEnabled>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::harmonyHome: if (!isEqual(get<Id::harmonyHome>(), value)) {get<Id::harmonyHome>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::harmonyCharacter: if (!isEqual(get<Id::harmonyCharacter>(), value)) {get<Id::harmonyCharacter>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::pedalNote: if (!isEqual(get<Id::pedalNote>(), value)) {get<Id::pedalNote>() = value;m_modified = true;}
break;
 case Id::luaParam1: if (!isEqual(get<Id::luaParam1>(), value)) {get<Id::luaParam1>() = value;m_modified = true;}
break;
 case Id::luaParam2: if (!isEqual(get<Id::luaParam2>(), value)) {get<Id::luaParam2>() = value;m_modified = true;}
break;
 case Id::luaParam3: if (!isEqual(get<Id::luaParam3>(), value)) {get<Id::luaParam3>() = value;m_modified = true;}
break;
 case Id::luaParam4: if (!isEqual(get<Id::luaParam4>(), value)) {get<Id::luaParam4>() = value;m_modified = true;}
break;
 case Id::luaParam5: if (!isEqual(get<Id::luaParam5>(), value)) {get<Id::luaParam5>() = value;m_modified = true;}
break;
 case Id::luaParam6: if (!isEqual(get<Id::luaParam6>(), value)) {get<Id::luaParam6>() = value;m_modified = true;}
break;
 case Id::luaParam7: if (!isEqual(get<Id::luaParam7>(), value)) {get<Id::luaParam7>() = value;m_modified = true;}
break;
 case Id::luaParam8: if (!isEqual(get<Id::luaParam8>(), value)) {get<Id::luaParam8>() = value;m_modified = true;}
break;
 case Id::script: break;

            default:
                break;
        }
    }

void updateScript(const std::string& value) { if (script != value) { script = value; m_modified = true; } }


    [[nodiscard]] bool isModified() const
    {
        return m_modified;
    }

    void clearModified()
    {
        m_modified = false;
    }

private:
    static bool isEqual(const float a, const float b)
    {
        return std::abs(a - b) < 1E-6f;
    }
    static bool isEqual(const int a, const float b)
    {
        return a == static_cast<int>(round(b));
    }
    static bool isEqual(const size_t a, const float b)
    {
        return a == static_cast<size_t>(round(b));
    }
    static bool isEqual(const bool a, const float b)
    {
        return a == static_cast<int>(round(b));
    }
    bool m_modified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PatchParameters,
        level           , // dial
        note            , // dial
        play            , // switch
        material        , // dial
        light           , // dial
        motion          , // dial
        breath          , // dial
        stability       , // dial
        bloom           , // dial
        hold            , // switch
        harmonyEnabled  , // switch
        harmonyHome     , // drop
        harmonyCharacter, // drop
        pedalNote       , // dial
        luaParam1       , // dial
        luaParam2       , // dial
        luaParam3       , // dial
        luaParam4       , // dial
        luaParam5       , // dial
        luaParam6       , // dial
        luaParam7       , // dial
        luaParam8       , // dial
        script           // script
)
