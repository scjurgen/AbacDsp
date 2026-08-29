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
        tapeSpeed         , // dial
        bars              , // dial
        inputGain         , // dial
        grooveLevel       , // dial
        recordA           , // switch
        playA             , // switch
        clearA            , // switch
        recordB           , // switch
        playB             , // switch
        clearB            , // switch
        recordC           , // switch
        playC             , // switch
        clearC            , // switch
        groovePlay        , // switch
        bpm               , // dial
        grooveVariation   , // dial
        grooveHumanizePush, // dial
        grooveHumanizeLife, // dial
        trackGainA        , // dial
        trackGainB        , // dial
        trackGainC        , // dial
        luaParam1         , // dial
        luaParam2         , // dial
        luaParam3         , // dial
        luaParam4         , // dial
        luaParam5         , // dial
        luaParam6         , // dial
        luaParam7         , // dial
        luaParam8         , // dial
        script             // script
    };
float tapeSpeed{1.0f};
float bars{8.0f};
float inputGain{0.0f};
float grooveLevel{0.0f};
bool recordA{false};
bool playA{false};
bool clearA{false};
bool recordB{false};
bool playB{false};
bool clearB{false};
bool recordC{false};
bool playC{false};
bool clearC{false};
bool groovePlay{false};
float bpm{120.0f};
float grooveVariation{0.0f};
float grooveHumanizePush{0.0f};
float grooveHumanizeLife{100.0f};
float trackGainA{0.0f};
float trackGainB{0.0f};
float trackGainC{0.0f};
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
        "tapeSpeed",
"bars",
"inputGain",
"grooveLevel",
"recordA",
"playA",
"clearA",
"recordB",
"playB",
"clearB",
"recordC",
"playC",
"clearC",
"groovePlay",
"bpm",
"grooveVariation",
"grooveHumanizePush",
"grooveHumanizeLife",
"trackGainA",
"trackGainB",
"trackGainC",
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
        if constexpr (ParamId == Id::tapeSpeed) return tapeSpeed;
        else if constexpr (ParamId == Id::bars) return bars;
        else if constexpr (ParamId == Id::inputGain) return inputGain;
        else if constexpr (ParamId == Id::grooveLevel) return grooveLevel;
        else if constexpr (ParamId == Id::recordA) return recordA;
        else if constexpr (ParamId == Id::playA) return playA;
        else if constexpr (ParamId == Id::clearA) return clearA;
        else if constexpr (ParamId == Id::recordB) return recordB;
        else if constexpr (ParamId == Id::playB) return playB;
        else if constexpr (ParamId == Id::clearB) return clearB;
        else if constexpr (ParamId == Id::recordC) return recordC;
        else if constexpr (ParamId == Id::playC) return playC;
        else if constexpr (ParamId == Id::clearC) return clearC;
        else if constexpr (ParamId == Id::groovePlay) return groovePlay;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::grooveVariation) return grooveVariation;
        else if constexpr (ParamId == Id::grooveHumanizePush) return grooveHumanizePush;
        else if constexpr (ParamId == Id::grooveHumanizeLife) return grooveHumanizeLife;
        else if constexpr (ParamId == Id::trackGainA) return trackGainA;
        else if constexpr (ParamId == Id::trackGainB) return trackGainB;
        else if constexpr (ParamId == Id::trackGainC) return trackGainC;
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
 case Id::tapeSpeed: if (!isEqual(get<Id::tapeSpeed>(), value)) {get<Id::tapeSpeed>() = value;m_modified = true;}
break;
 case Id::bars: if (!isEqual(get<Id::bars>(), value)) {get<Id::bars>() = value;m_modified = true;}
break;
 case Id::inputGain: if (!isEqual(get<Id::inputGain>(), value)) {get<Id::inputGain>() = value;m_modified = true;}
break;
 case Id::grooveLevel: if (!isEqual(get<Id::grooveLevel>(), value)) {get<Id::grooveLevel>() = value;m_modified = true;}
break;
 case Id::recordA: if (!isEqual(get<Id::recordA>(), value)) {get<Id::recordA>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::playA: if (!isEqual(get<Id::playA>(), value)) {get<Id::playA>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearA: if (!isEqual(get<Id::clearA>(), value)) {get<Id::clearA>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::recordB: if (!isEqual(get<Id::recordB>(), value)) {get<Id::recordB>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::playB: if (!isEqual(get<Id::playB>(), value)) {get<Id::playB>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearB: if (!isEqual(get<Id::clearB>(), value)) {get<Id::clearB>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::recordC: if (!isEqual(get<Id::recordC>(), value)) {get<Id::recordC>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::playC: if (!isEqual(get<Id::playC>(), value)) {get<Id::playC>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearC: if (!isEqual(get<Id::clearC>(), value)) {get<Id::clearC>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::groovePlay: if (!isEqual(get<Id::groovePlay>(), value)) {get<Id::groovePlay>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::grooveVariation: if (!isEqual(get<Id::grooveVariation>(), value)) {get<Id::grooveVariation>() = value;m_modified = true;}
break;
 case Id::grooveHumanizePush: if (!isEqual(get<Id::grooveHumanizePush>(), value)) {get<Id::grooveHumanizePush>() = value;m_modified = true;}
break;
 case Id::grooveHumanizeLife: if (!isEqual(get<Id::grooveHumanizeLife>(), value)) {get<Id::grooveHumanizeLife>() = value;m_modified = true;}
break;
 case Id::trackGainA: if (!isEqual(get<Id::trackGainA>(), value)) {get<Id::trackGainA>() = value;m_modified = true;}
break;
 case Id::trackGainB: if (!isEqual(get<Id::trackGainB>(), value)) {get<Id::trackGainB>() = value;m_modified = true;}
break;
 case Id::trackGainC: if (!isEqual(get<Id::trackGainC>(), value)) {get<Id::trackGainC>() = value;m_modified = true;}
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
        tapeSpeed         , // dial
        bars              , // dial
        inputGain         , // dial
        grooveLevel       , // dial
        recordA           , // switch
        playA             , // switch
        clearA            , // switch
        recordB           , // switch
        playB             , // switch
        clearB            , // switch
        recordC           , // switch
        playC             , // switch
        clearC            , // switch
        groovePlay        , // switch
        bpm               , // dial
        grooveVariation   , // dial
        grooveHumanizePush, // dial
        grooveHumanizeLife, // dial
        trackGainA        , // dial
        trackGainB        , // dial
        trackGainC        , // dial
        luaParam1         , // dial
        luaParam2         , // dial
        luaParam3         , // dial
        luaParam4         , // dial
        luaParam5         , // dial
        luaParam6         , // dial
        luaParam7         , // dial
        luaParam8         , // dial
        script             // script
)
