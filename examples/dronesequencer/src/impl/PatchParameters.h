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
        level             , // dial
        tuning            , // dial
        transpose         , // dial
        detune            , // dial
        script            , // script
        reverbDry         , // dial
        reverbWet         , // dial
        reverbSize        , // dial
        reverbDecay       , // dial
        reverbShelfLow    , // dial
        reverbShelfHigh   , // dial
        playStop          , // switch
        humanizeTiming    , // dial
        humanizeLevel     , // dial
        bpm               , // dial
        hostSync          , // switch
        division          , // drop
        attack            , // dial
        decay             , // dial
        decayOctave       , // dial
        damper            , // dial
        levelSustain      , // dial
        sustainHumanize   , // dial
        lfoDepth          , // dial
        lfoSpeed          , // dial
        lfoSpeedVariation , // dial
        attackFilter      , // dial
        decayFilter       , // dial
        levelSustainFilter, // dial
        filterCutoff      , // dial
        filterResonance   , // dial
        contourFilter     , // dial
        luaParam1         , // dial
        luaParam2         , // dial
        luaParam3         , // dial
        luaParam4         , // dial
        luaParam5         , // dial
        luaParam6         , // dial
        luaParam7         , // dial
        luaParam8          // dial
    };
float level{0.0f};
float tuning{440.0f};
float transpose{0.0f};
float detune{5.0f};
std::string script{};
float reverbDry{0.0f};
float reverbWet{-100.0f};
float reverbSize{30.0f};
float reverbDecay{2000.0f};
float reverbShelfLow{0.0f};
float reverbShelfHigh{0.0f};
bool playStop{false};
float humanizeTiming{0.0f};
float humanizeLevel{0.0f};
float bpm{120.0f};
bool hostSync{false};
size_t division{4};
float attack{10.0f};
float decay{30000.0f};
float decayOctave{1.0f};
float damper{0.0f};
float levelSustain{0.2f};
float sustainHumanize{0.0f};
float lfoDepth{0.5f};
float lfoSpeed{0.5f};
float lfoSpeedVariation{0.0f};
float attackFilter{10.0f};
float decayFilter{1000.0f};
float levelSustainFilter{0.0f};
float filterCutoff{0.0f};
float filterResonance{0.1f};
float contourFilter{0.0f};
float luaParam1{0.0f};
float luaParam2{0.0f};
float luaParam3{0.0f};
float luaParam4{0.0f};
float luaParam5{0.0f};
float luaParam6{0.0f};
float luaParam7{0.0f};
float luaParam8{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "level",
"tuning",
"transpose",
"detune",
"reverbDry",
"reverbWet",
"reverbSize",
"reverbDecay",
"reverbShelfLow",
"reverbShelfHigh",
"playStop",
"humanizeTiming",
"humanizeLevel",
"bpm",
"hostSync",
"division",
"attack",
"decay",
"decayOctave",
"damper",
"levelSustain",
"sustainHumanize",
"lfoDepth",
"lfoSpeed",
"lfoSpeedVariation",
"attackFilter",
"decayFilter",
"levelSustainFilter",
"filterCutoff",
"filterResonance",
"contourFilter",
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
        else if constexpr (ParamId == Id::tuning) return tuning;
        else if constexpr (ParamId == Id::transpose) return transpose;
        else if constexpr (ParamId == Id::detune) return detune;
        else if constexpr (ParamId == Id::script) return script;
        else if constexpr (ParamId == Id::reverbDry) return reverbDry;
        else if constexpr (ParamId == Id::reverbWet) return reverbWet;
        else if constexpr (ParamId == Id::reverbSize) return reverbSize;
        else if constexpr (ParamId == Id::reverbDecay) return reverbDecay;
        else if constexpr (ParamId == Id::reverbShelfLow) return reverbShelfLow;
        else if constexpr (ParamId == Id::reverbShelfHigh) return reverbShelfHigh;
        else if constexpr (ParamId == Id::playStop) return playStop;
        else if constexpr (ParamId == Id::humanizeTiming) return humanizeTiming;
        else if constexpr (ParamId == Id::humanizeLevel) return humanizeLevel;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::hostSync) return hostSync;
        else if constexpr (ParamId == Id::division) return division;
        else if constexpr (ParamId == Id::attack) return attack;
        else if constexpr (ParamId == Id::decay) return decay;
        else if constexpr (ParamId == Id::decayOctave) return decayOctave;
        else if constexpr (ParamId == Id::damper) return damper;
        else if constexpr (ParamId == Id::levelSustain) return levelSustain;
        else if constexpr (ParamId == Id::sustainHumanize) return sustainHumanize;
        else if constexpr (ParamId == Id::lfoDepth) return lfoDepth;
        else if constexpr (ParamId == Id::lfoSpeed) return lfoSpeed;
        else if constexpr (ParamId == Id::lfoSpeedVariation) return lfoSpeedVariation;
        else if constexpr (ParamId == Id::attackFilter) return attackFilter;
        else if constexpr (ParamId == Id::decayFilter) return decayFilter;
        else if constexpr (ParamId == Id::levelSustainFilter) return levelSustainFilter;
        else if constexpr (ParamId == Id::filterCutoff) return filterCutoff;
        else if constexpr (ParamId == Id::filterResonance) return filterResonance;
        else if constexpr (ParamId == Id::contourFilter) return contourFilter;
        else if constexpr (ParamId == Id::luaParam1) return luaParam1;
        else if constexpr (ParamId == Id::luaParam2) return luaParam2;
        else if constexpr (ParamId == Id::luaParam3) return luaParam3;
        else if constexpr (ParamId == Id::luaParam4) return luaParam4;
        else if constexpr (ParamId == Id::luaParam5) return luaParam5;
        else if constexpr (ParamId == Id::luaParam6) return luaParam6;
        else if constexpr (ParamId == Id::luaParam7) return luaParam7;
        else if constexpr (ParamId == Id::luaParam8) return luaParam8;

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
 case Id::tuning: if (!isEqual(get<Id::tuning>(), value)) {get<Id::tuning>() = value;m_modified = true;}
break;
 case Id::transpose: if (!isEqual(get<Id::transpose>(), value)) {get<Id::transpose>() = value;m_modified = true;}
break;
 case Id::detune: if (!isEqual(get<Id::detune>(), value)) {get<Id::detune>() = value;m_modified = true;}
break;
 case Id::script: break;
 case Id::reverbDry: if (!isEqual(get<Id::reverbDry>(), value)) {get<Id::reverbDry>() = value;m_modified = true;}
break;
 case Id::reverbWet: if (!isEqual(get<Id::reverbWet>(), value)) {get<Id::reverbWet>() = value;m_modified = true;}
break;
 case Id::reverbSize: if (!isEqual(get<Id::reverbSize>(), value)) {get<Id::reverbSize>() = value;m_modified = true;}
break;
 case Id::reverbDecay: if (!isEqual(get<Id::reverbDecay>(), value)) {get<Id::reverbDecay>() = value;m_modified = true;}
break;
 case Id::reverbShelfLow: if (!isEqual(get<Id::reverbShelfLow>(), value)) {get<Id::reverbShelfLow>() = value;m_modified = true;}
break;
 case Id::reverbShelfHigh: if (!isEqual(get<Id::reverbShelfHigh>(), value)) {get<Id::reverbShelfHigh>() = value;m_modified = true;}
break;
 case Id::playStop: if (!isEqual(get<Id::playStop>(), value)) {get<Id::playStop>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::humanizeTiming: if (!isEqual(get<Id::humanizeTiming>(), value)) {get<Id::humanizeTiming>() = value;m_modified = true;}
break;
 case Id::humanizeLevel: if (!isEqual(get<Id::humanizeLevel>(), value)) {get<Id::humanizeLevel>() = value;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::hostSync: if (!isEqual(get<Id::hostSync>(), value)) {get<Id::hostSync>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::division: if (!isEqual(get<Id::division>(), value)) {get<Id::division>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::attack: if (!isEqual(get<Id::attack>(), value)) {get<Id::attack>() = value;m_modified = true;}
break;
 case Id::decay: if (!isEqual(get<Id::decay>(), value)) {get<Id::decay>() = value;m_modified = true;}
break;
 case Id::decayOctave: if (!isEqual(get<Id::decayOctave>(), value)) {get<Id::decayOctave>() = value;m_modified = true;}
break;
 case Id::damper: if (!isEqual(get<Id::damper>(), value)) {get<Id::damper>() = value;m_modified = true;}
break;
 case Id::levelSustain: if (!isEqual(get<Id::levelSustain>(), value)) {get<Id::levelSustain>() = value;m_modified = true;}
break;
 case Id::sustainHumanize: if (!isEqual(get<Id::sustainHumanize>(), value)) {get<Id::sustainHumanize>() = value;m_modified = true;}
break;
 case Id::lfoDepth: if (!isEqual(get<Id::lfoDepth>(), value)) {get<Id::lfoDepth>() = value;m_modified = true;}
break;
 case Id::lfoSpeed: if (!isEqual(get<Id::lfoSpeed>(), value)) {get<Id::lfoSpeed>() = value;m_modified = true;}
break;
 case Id::lfoSpeedVariation: if (!isEqual(get<Id::lfoSpeedVariation>(), value)) {get<Id::lfoSpeedVariation>() = value;m_modified = true;}
break;
 case Id::attackFilter: if (!isEqual(get<Id::attackFilter>(), value)) {get<Id::attackFilter>() = value;m_modified = true;}
break;
 case Id::decayFilter: if (!isEqual(get<Id::decayFilter>(), value)) {get<Id::decayFilter>() = value;m_modified = true;}
break;
 case Id::levelSustainFilter: if (!isEqual(get<Id::levelSustainFilter>(), value)) {get<Id::levelSustainFilter>() = value;m_modified = true;}
break;
 case Id::filterCutoff: if (!isEqual(get<Id::filterCutoff>(), value)) {get<Id::filterCutoff>() = value;m_modified = true;}
break;
 case Id::filterResonance: if (!isEqual(get<Id::filterResonance>(), value)) {get<Id::filterResonance>() = value;m_modified = true;}
break;
 case Id::contourFilter: if (!isEqual(get<Id::contourFilter>(), value)) {get<Id::contourFilter>() = value;m_modified = true;}
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
        return a == (static_cast<int>(round(b)))?false:true;
    }
    bool m_modified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PatchParameters,
        level             , // dial
        tuning            , // dial
        transpose         , // dial
        detune            , // dial
        script            , // script
        reverbDry         , // dial
        reverbWet         , // dial
        reverbSize        , // dial
        reverbDecay       , // dial
        reverbShelfLow    , // dial
        reverbShelfHigh   , // dial
        playStop          , // switch
        humanizeTiming    , // dial
        humanizeLevel     , // dial
        bpm               , // dial
        hostSync          , // switch
        division          , // drop
        attack            , // dial
        decay             , // dial
        decayOctave       , // dial
        damper            , // dial
        levelSustain      , // dial
        sustainHumanize   , // dial
        lfoDepth          , // dial
        lfoSpeed          , // dial
        lfoSpeedVariation , // dial
        attackFilter      , // dial
        decayFilter       , // dial
        levelSustainFilter, // dial
        filterCutoff      , // dial
        filterResonance   , // dial
        contourFilter     , // dial
        luaParam1         , // dial
        luaParam2         , // dial
        luaParam3         , // dial
        luaParam4         , // dial
        luaParam5         , // dial
        luaParam6         , // dial
        luaParam7         , // dial
        luaParam8          // dial
)
