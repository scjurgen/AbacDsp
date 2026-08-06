#pragma once

#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <string_view>

using json = nlohmann::json;

// clang-format off
struct PatchParameters
{
    enum class Id : int
    {
        key               , // drop
        level             , // dial
        tuning            , // dial
        detuneString1     , // dial
        detuneString2     , // dial
        detuneString3     , // dial
        detuneString4     , // dial
        detuneString5     , // dial
        pattern           , // drop
        slide             , // dial
        harmonicFirst     , // drop
        harmonicSecond    , // drop
        playStop          , // switch
        picksPerMinute    , // dial
        pauseLength       , // dial
        attack            , // dial
        decay             , // dial
        levelSustain      , // dial
        lfoDepth          , // dial
        attackFilter      , // dial
        decayFilter       , // dial
        levelSustainFilter, // dial
        filterCutoff      , // dial
        filterResonance   , // dial
        contourFilter      // dial
    };
size_t key{12};
float level{0.0f};
float tuning{440.0f};
float detuneString1{0.0f};
float detuneString2{5.0f};
float detuneString3{-5.0f};
float detuneString4{7.0f};
float detuneString5{-2.0f};
size_t pattern{0};
float slide{0.0f};
size_t harmonicFirst{7};
size_t harmonicSecond{0};
bool playStop{false};
float picksPerMinute{100.0f};
float pauseLength{10.0f};
float attack{10.0f};
float decay{10.0f};
float levelSustain{0.2f};
float lfoDepth{0.5f};
float attackFilter{10.0f};
float decayFilter{10.0f};
float levelSustainFilter{0.0f};
float filterCutoff{0.0f};
float filterResonance{0.1f};
float contourFilter{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "key",
"level",
"tuning",
"detuneString1",
"detuneString2",
"detuneString3",
"detuneString4",
"detuneString5",
"pattern",
"slide",
"harmonicFirst",
"harmonicSecond",
"playStop",
"picksPerMinute",
"pauseLength",
"attack",
"decay",
"levelSustain",
"lfoDepth",
"attackFilter",
"decayFilter",
"levelSustainFilter",
"filterCutoff",
"filterResonance",
"contourFilter"
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
        if constexpr (ParamId == Id::key) return key;
        else if constexpr (ParamId == Id::level) return level;
        else if constexpr (ParamId == Id::tuning) return tuning;
        else if constexpr (ParamId == Id::detuneString1) return detuneString1;
        else if constexpr (ParamId == Id::detuneString2) return detuneString2;
        else if constexpr (ParamId == Id::detuneString3) return detuneString3;
        else if constexpr (ParamId == Id::detuneString4) return detuneString4;
        else if constexpr (ParamId == Id::detuneString5) return detuneString5;
        else if constexpr (ParamId == Id::pattern) return pattern;
        else if constexpr (ParamId == Id::slide) return slide;
        else if constexpr (ParamId == Id::harmonicFirst) return harmonicFirst;
        else if constexpr (ParamId == Id::harmonicSecond) return harmonicSecond;
        else if constexpr (ParamId == Id::playStop) return playStop;
        else if constexpr (ParamId == Id::picksPerMinute) return picksPerMinute;
        else if constexpr (ParamId == Id::pauseLength) return pauseLength;
        else if constexpr (ParamId == Id::attack) return attack;
        else if constexpr (ParamId == Id::decay) return decay;
        else if constexpr (ParamId == Id::levelSustain) return levelSustain;
        else if constexpr (ParamId == Id::lfoDepth) return lfoDepth;
        else if constexpr (ParamId == Id::attackFilter) return attackFilter;
        else if constexpr (ParamId == Id::decayFilter) return decayFilter;
        else if constexpr (ParamId == Id::levelSustainFilter) return levelSustainFilter;
        else if constexpr (ParamId == Id::filterCutoff) return filterCutoff;
        else if constexpr (ParamId == Id::filterResonance) return filterResonance;
        else if constexpr (ParamId == Id::contourFilter) return contourFilter;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::key: if (!isEqual(get<Id::key>(), value)) {get<Id::key>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::level: if (!isEqual(get<Id::level>(), value)) {get<Id::level>() = value;m_modified = true;}
break;
 case Id::tuning: if (!isEqual(get<Id::tuning>(), value)) {get<Id::tuning>() = value;m_modified = true;}
break;
 case Id::detuneString1: if (!isEqual(get<Id::detuneString1>(), value)) {get<Id::detuneString1>() = value;m_modified = true;}
break;
 case Id::detuneString2: if (!isEqual(get<Id::detuneString2>(), value)) {get<Id::detuneString2>() = value;m_modified = true;}
break;
 case Id::detuneString3: if (!isEqual(get<Id::detuneString3>(), value)) {get<Id::detuneString3>() = value;m_modified = true;}
break;
 case Id::detuneString4: if (!isEqual(get<Id::detuneString4>(), value)) {get<Id::detuneString4>() = value;m_modified = true;}
break;
 case Id::detuneString5: if (!isEqual(get<Id::detuneString5>(), value)) {get<Id::detuneString5>() = value;m_modified = true;}
break;
 case Id::pattern: if (!isEqual(get<Id::pattern>(), value)) {get<Id::pattern>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::slide: if (!isEqual(get<Id::slide>(), value)) {get<Id::slide>() = value;m_modified = true;}
break;
 case Id::harmonicFirst: if (!isEqual(get<Id::harmonicFirst>(), value)) {get<Id::harmonicFirst>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::harmonicSecond: if (!isEqual(get<Id::harmonicSecond>(), value)) {get<Id::harmonicSecond>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::playStop: if (!isEqual(get<Id::playStop>(), value)) {get<Id::playStop>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::picksPerMinute: if (!isEqual(get<Id::picksPerMinute>(), value)) {get<Id::picksPerMinute>() = value;m_modified = true;}
break;
 case Id::pauseLength: if (!isEqual(get<Id::pauseLength>(), value)) {get<Id::pauseLength>() = value;m_modified = true;}
break;
 case Id::attack: if (!isEqual(get<Id::attack>(), value)) {get<Id::attack>() = value;m_modified = true;}
break;
 case Id::decay: if (!isEqual(get<Id::decay>(), value)) {get<Id::decay>() = value;m_modified = true;}
break;
 case Id::levelSustain: if (!isEqual(get<Id::levelSustain>(), value)) {get<Id::levelSustain>() = value;m_modified = true;}
break;
 case Id::lfoDepth: if (!isEqual(get<Id::lfoDepth>(), value)) {get<Id::lfoDepth>() = value;m_modified = true;}
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

            default:
                break;
        }
    }

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
        key               , // drop
        level             , // dial
        tuning            , // dial
        detuneString1     , // dial
        detuneString2     , // dial
        detuneString3     , // dial
        detuneString4     , // dial
        detuneString5     , // dial
        pattern           , // drop
        slide             , // dial
        harmonicFirst     , // drop
        harmonicSecond    , // drop
        playStop          , // switch
        picksPerMinute    , // dial
        pauseLength       , // dial
        attack            , // dial
        decay             , // dial
        levelSustain      , // dial
        lfoDepth          , // dial
        attackFilter      , // dial
        decayFilter       , // dial
        levelSustainFilter, // dial
        filterCutoff      , // dial
        filterResonance   , // dial
        contourFilter      // dial
)
