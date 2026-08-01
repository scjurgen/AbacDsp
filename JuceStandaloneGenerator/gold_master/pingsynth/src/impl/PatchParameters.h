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
        mode          , // drop
        vol           , // dial
        reverbLevel   , // dial
        x             , // dial
        y             , // dial
        attack        , // dial
        decay         , // dial
        decaySkew     , // dial
        spread        , // dial
        type          , // drop
        skew          , // dial
        Spread        , // dial
        randPower     , // dial
        randExcitation, // dial
        softExcitation, // dial
        sparkleTime   , // dial
        sparkleRand   , // dial
        minHarmonics  , // dial
        maxHarmonics  , // dial
        minPbNote     , // dial
        maxPbNote     , // dial
        rangePb        // dial
    };
size_t mode{1};
float vol{0.0f};
float reverbLevel{-24.0f};
float x{25.0f};
float y{25.0f};
float attack{1.0f};
float decay{2.0f};
float decaySkew{0.0f};
float spread{0.0f};
size_t type{0};
float skew{0.0f};
float Spread{0.0f};
float randPower{0.0f};
float randExcitation{0.0f};
float softExcitation{0.0f};
float sparkleTime{0.0f};
float sparkleRand{0.0f};
float minHarmonics{5.0f};
float maxHarmonics{10.0f};
float minPbNote{25.0f};
float maxPbNote{120.0f};
float rangePb{12.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "mode",
"vol",
"reverbLevel",
"x",
"y",
"attack",
"decay",
"decaySkew",
"spread",
"type",
"skew",
"Spread",
"randPower",
"randExcitation",
"softExcitation",
"sparkleTime",
"sparkleRand",
"minHarmonics",
"maxHarmonics",
"minPbNote",
"maxPbNote",
"rangePb"
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
        if constexpr (ParamId == Id::mode) return mode;
        else if constexpr (ParamId == Id::vol) return vol;
        else if constexpr (ParamId == Id::reverbLevel) return reverbLevel;
        else if constexpr (ParamId == Id::x) return x;
        else if constexpr (ParamId == Id::y) return y;
        else if constexpr (ParamId == Id::attack) return attack;
        else if constexpr (ParamId == Id::decay) return decay;
        else if constexpr (ParamId == Id::decaySkew) return decaySkew;
        else if constexpr (ParamId == Id::spread) return spread;
        else if constexpr (ParamId == Id::type) return type;
        else if constexpr (ParamId == Id::skew) return skew;
        else if constexpr (ParamId == Id::Spread) return Spread;
        else if constexpr (ParamId == Id::randPower) return randPower;
        else if constexpr (ParamId == Id::randExcitation) return randExcitation;
        else if constexpr (ParamId == Id::softExcitation) return softExcitation;
        else if constexpr (ParamId == Id::sparkleTime) return sparkleTime;
        else if constexpr (ParamId == Id::sparkleRand) return sparkleRand;
        else if constexpr (ParamId == Id::minHarmonics) return minHarmonics;
        else if constexpr (ParamId == Id::maxHarmonics) return maxHarmonics;
        else if constexpr (ParamId == Id::minPbNote) return minPbNote;
        else if constexpr (ParamId == Id::maxPbNote) return maxPbNote;
        else if constexpr (ParamId == Id::rangePb) return rangePb;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::mode: if (!isEqual(get<Id::mode>(), value)) {get<Id::mode>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::vol: if (!isEqual(get<Id::vol>(), value)) {get<Id::vol>() = value;m_modified = true;}
break;
 case Id::reverbLevel: if (!isEqual(get<Id::reverbLevel>(), value)) {get<Id::reverbLevel>() = value;m_modified = true;}
break;
 case Id::x: if (!isEqual(get<Id::x>(), value)) {get<Id::x>() = value;m_modified = true;}
break;
 case Id::y: if (!isEqual(get<Id::y>(), value)) {get<Id::y>() = value;m_modified = true;}
break;
 case Id::attack: if (!isEqual(get<Id::attack>(), value)) {get<Id::attack>() = value;m_modified = true;}
break;
 case Id::decay: if (!isEqual(get<Id::decay>(), value)) {get<Id::decay>() = value;m_modified = true;}
break;
 case Id::decaySkew: if (!isEqual(get<Id::decaySkew>(), value)) {get<Id::decaySkew>() = value;m_modified = true;}
break;
 case Id::spread: if (!isEqual(get<Id::spread>(), value)) {get<Id::spread>() = value;m_modified = true;}
break;
 case Id::type: if (!isEqual(get<Id::type>(), value)) {get<Id::type>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::skew: if (!isEqual(get<Id::skew>(), value)) {get<Id::skew>() = value;m_modified = true;}
break;
 case Id::Spread: if (!isEqual(get<Id::Spread>(), value)) {get<Id::Spread>() = value;m_modified = true;}
break;
 case Id::randPower: if (!isEqual(get<Id::randPower>(), value)) {get<Id::randPower>() = value;m_modified = true;}
break;
 case Id::randExcitation: if (!isEqual(get<Id::randExcitation>(), value)) {get<Id::randExcitation>() = value;m_modified = true;}
break;
 case Id::softExcitation: if (!isEqual(get<Id::softExcitation>(), value)) {get<Id::softExcitation>() = value;m_modified = true;}
break;
 case Id::sparkleTime: if (!isEqual(get<Id::sparkleTime>(), value)) {get<Id::sparkleTime>() = value;m_modified = true;}
break;
 case Id::sparkleRand: if (!isEqual(get<Id::sparkleRand>(), value)) {get<Id::sparkleRand>() = value;m_modified = true;}
break;
 case Id::minHarmonics: if (!isEqual(get<Id::minHarmonics>(), value)) {get<Id::minHarmonics>() = value;m_modified = true;}
break;
 case Id::maxHarmonics: if (!isEqual(get<Id::maxHarmonics>(), value)) {get<Id::maxHarmonics>() = value;m_modified = true;}
break;
 case Id::minPbNote: if (!isEqual(get<Id::minPbNote>(), value)) {get<Id::minPbNote>() = value;m_modified = true;}
break;
 case Id::maxPbNote: if (!isEqual(get<Id::maxPbNote>(), value)) {get<Id::maxPbNote>() = value;m_modified = true;}
break;
 case Id::rangePb: if (!isEqual(get<Id::rangePb>(), value)) {get<Id::rangePb>() = value;m_modified = true;}
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
        mode          , // drop
        vol           , // dial
        reverbLevel   , // dial
        x             , // dial
        y             , // dial
        attack        , // dial
        decay         , // dial
        decaySkew     , // dial
        spread        , // dial
        type          , // drop
        skew          , // dial
        Spread        , // dial
        randPower     , // dial
        randExcitation, // dial
        softExcitation, // dial
        sparkleTime   , // dial
        sparkleRand   , // dial
        minHarmonics  , // dial
        maxHarmonics  , // dial
        minPbNote     , // dial
        maxPbNote     , // dial
        rangePb        // dial
)
