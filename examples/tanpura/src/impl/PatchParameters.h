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
        detune            , // dial
        reverbDry         , // dial
        reverbWet         , // dial
        reverbSize        , // dial
        reverbDecay       , // dial
        reverbShelfLow    , // dial
        reverbShelfHigh   , // dial
        pattern           , // drop
        slide             , // dial
        slideTime         , // dial
        harmonicFirst     , // drop
        harmonicSecond    , // drop
        playStop          , // switch
        humanizeTiming    , // dial
        humanizeLevel     , // dial
        bpm               , // dial
        hostSync          , // switch
        pluckDivision     , // drop
        pauseDivision     , // drop
        attack            , // dial
        decay             , // dial
        decayOctave       , // dial
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
        contourFilter      // dial
    };
size_t key{12};
float level{0.0f};
float tuning{440.0f};
float detune{5.0f};
float reverbDry{0.0f};
float reverbWet{-100.0f};
float reverbSize{30.0f};
float reverbDecay{2000.0f};
float reverbShelfLow{0.0f};
float reverbShelfHigh{0.0f};
size_t pattern{0};
float slide{0.0f};
float slideTime{150.0f};
size_t harmonicFirst{7};
size_t harmonicSecond{0};
bool playStop{false};
float humanizeTiming{0.0f};
float humanizeLevel{0.0f};
float bpm{120.0f};
bool hostSync{false};
size_t pluckDivision{4};
size_t pauseDivision{4};
float attack{10.0f};
float decay{10.0f};
float decayOctave{1.0f};
float levelSustain{0.2f};
float sustainHumanize{0.0f};
float lfoDepth{0.5f};
float lfoSpeed{0.5f};
float lfoSpeedVariation{0.0f};
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
"detune",
"reverbDry",
"reverbWet",
"reverbSize",
"reverbDecay",
"reverbShelfLow",
"reverbShelfHigh",
"pattern",
"slide",
"slideTime",
"harmonicFirst",
"harmonicSecond",
"playStop",
"humanizeTiming",
"humanizeLevel",
"bpm",
"hostSync",
"pluckDivision",
"pauseDivision",
"attack",
"decay",
"decayOctave",
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
        else if constexpr (ParamId == Id::detune) return detune;
        else if constexpr (ParamId == Id::reverbDry) return reverbDry;
        else if constexpr (ParamId == Id::reverbWet) return reverbWet;
        else if constexpr (ParamId == Id::reverbSize) return reverbSize;
        else if constexpr (ParamId == Id::reverbDecay) return reverbDecay;
        else if constexpr (ParamId == Id::reverbShelfLow) return reverbShelfLow;
        else if constexpr (ParamId == Id::reverbShelfHigh) return reverbShelfHigh;
        else if constexpr (ParamId == Id::pattern) return pattern;
        else if constexpr (ParamId == Id::slide) return slide;
        else if constexpr (ParamId == Id::slideTime) return slideTime;
        else if constexpr (ParamId == Id::harmonicFirst) return harmonicFirst;
        else if constexpr (ParamId == Id::harmonicSecond) return harmonicSecond;
        else if constexpr (ParamId == Id::playStop) return playStop;
        else if constexpr (ParamId == Id::humanizeTiming) return humanizeTiming;
        else if constexpr (ParamId == Id::humanizeLevel) return humanizeLevel;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::hostSync) return hostSync;
        else if constexpr (ParamId == Id::pluckDivision) return pluckDivision;
        else if constexpr (ParamId == Id::pauseDivision) return pauseDivision;
        else if constexpr (ParamId == Id::attack) return attack;
        else if constexpr (ParamId == Id::decay) return decay;
        else if constexpr (ParamId == Id::decayOctave) return decayOctave;
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
 case Id::detune: if (!isEqual(get<Id::detune>(), value)) {get<Id::detune>() = value;m_modified = true;}
break;
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
 case Id::pattern: if (!isEqual(get<Id::pattern>(), value)) {get<Id::pattern>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::slide: if (!isEqual(get<Id::slide>(), value)) {get<Id::slide>() = value;m_modified = true;}
break;
 case Id::slideTime: if (!isEqual(get<Id::slideTime>(), value)) {get<Id::slideTime>() = value;m_modified = true;}
break;
 case Id::harmonicFirst: if (!isEqual(get<Id::harmonicFirst>(), value)) {get<Id::harmonicFirst>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::harmonicSecond: if (!isEqual(get<Id::harmonicSecond>(), value)) {get<Id::harmonicSecond>() = static_cast<size_t>(value) ;m_modified = true;}
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
 case Id::pluckDivision: if (!isEqual(get<Id::pluckDivision>(), value)) {get<Id::pluckDivision>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::pauseDivision: if (!isEqual(get<Id::pauseDivision>(), value)) {get<Id::pauseDivision>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::attack: if (!isEqual(get<Id::attack>(), value)) {get<Id::attack>() = value;m_modified = true;}
break;
 case Id::decay: if (!isEqual(get<Id::decay>(), value)) {get<Id::decay>() = value;m_modified = true;}
break;
 case Id::decayOctave: if (!isEqual(get<Id::decayOctave>(), value)) {get<Id::decayOctave>() = value;m_modified = true;}
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
        detune            , // dial
        reverbDry         , // dial
        reverbWet         , // dial
        reverbSize        , // dial
        reverbDecay       , // dial
        reverbShelfLow    , // dial
        reverbShelfHigh   , // dial
        pattern           , // drop
        slide             , // dial
        slideTime         , // dial
        harmonicFirst     , // drop
        harmonicSecond    , // drop
        playStop          , // switch
        humanizeTiming    , // dial
        humanizeLevel     , // dial
        bpm               , // dial
        hostSync          , // switch
        pluckDivision     , // drop
        pauseDivision     , // drop
        attack            , // dial
        decay             , // dial
        decayOctave       , // dial
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
        contourFilter      // dial
)
