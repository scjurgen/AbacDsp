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
        feedGain         , // dial
        tapeSpeed        , // dial
        wow              , // dial
        hysteresis       , // dial
        saturation       , // dial
        noiseFloor       , // dial
        noiseDistribution, // dial
        delayTime1       , // dial
        delayTime2       , // dial
        delayTime3       , // dial
        delayTime4       , // dial
        delayLevel1      , // dial
        delayLevel2      , // dial
        delayLevel3      , // dial
        delayLevel4      , // dial
        feedback1        , // dial
        feedback2        , // dial
        feedback3        , // dial
        feedback4         // dial
    };
float feedGain{0.0f};
float tapeSpeed{7.5f};
float wow{0.0f};
float hysteresis{0.0f};
float saturation{0.0f};
float noiseFloor{0.0f};
float noiseDistribution{0.0f};
float delayTime1{200.0f};
float delayTime2{100.0f};
float delayTime3{100.0f};
float delayTime4{2000.0f};
float delayLevel1{0.0f};
float delayLevel2{-6.0f};
float delayLevel3{-12.0f};
float delayLevel4{-18.0f};
float feedback1{0.0f};
float feedback2{0.0f};
float feedback3{0.0f};
float feedback4{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "feedGain",
"tapeSpeed",
"wow",
"hysteresis",
"saturation",
"noiseFloor",
"noiseDistribution",
"delayTime1",
"delayTime2",
"delayTime3",
"delayTime4",
"delayLevel1",
"delayLevel2",
"delayLevel3",
"delayLevel4",
"feedback1",
"feedback2",
"feedback3",
"feedback4"
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
        if constexpr (ParamId == Id::feedGain) return feedGain;
        else if constexpr (ParamId == Id::tapeSpeed) return tapeSpeed;
        else if constexpr (ParamId == Id::wow) return wow;
        else if constexpr (ParamId == Id::hysteresis) return hysteresis;
        else if constexpr (ParamId == Id::saturation) return saturation;
        else if constexpr (ParamId == Id::noiseFloor) return noiseFloor;
        else if constexpr (ParamId == Id::noiseDistribution) return noiseDistribution;
        else if constexpr (ParamId == Id::delayTime1) return delayTime1;
        else if constexpr (ParamId == Id::delayTime2) return delayTime2;
        else if constexpr (ParamId == Id::delayTime3) return delayTime3;
        else if constexpr (ParamId == Id::delayTime4) return delayTime4;
        else if constexpr (ParamId == Id::delayLevel1) return delayLevel1;
        else if constexpr (ParamId == Id::delayLevel2) return delayLevel2;
        else if constexpr (ParamId == Id::delayLevel3) return delayLevel3;
        else if constexpr (ParamId == Id::delayLevel4) return delayLevel4;
        else if constexpr (ParamId == Id::feedback1) return feedback1;
        else if constexpr (ParamId == Id::feedback2) return feedback2;
        else if constexpr (ParamId == Id::feedback3) return feedback3;
        else if constexpr (ParamId == Id::feedback4) return feedback4;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::feedGain: if (!isEqual(get<Id::feedGain>(), value)) {get<Id::feedGain>() = value;m_modified = true;}
break;
 case Id::tapeSpeed: if (!isEqual(get<Id::tapeSpeed>(), value)) {get<Id::tapeSpeed>() = value;m_modified = true;}
break;
 case Id::wow: if (!isEqual(get<Id::wow>(), value)) {get<Id::wow>() = value;m_modified = true;}
break;
 case Id::hysteresis: if (!isEqual(get<Id::hysteresis>(), value)) {get<Id::hysteresis>() = value;m_modified = true;}
break;
 case Id::saturation: if (!isEqual(get<Id::saturation>(), value)) {get<Id::saturation>() = value;m_modified = true;}
break;
 case Id::noiseFloor: if (!isEqual(get<Id::noiseFloor>(), value)) {get<Id::noiseFloor>() = value;m_modified = true;}
break;
 case Id::noiseDistribution: if (!isEqual(get<Id::noiseDistribution>(), value)) {get<Id::noiseDistribution>() = value;m_modified = true;}
break;
 case Id::delayTime1: if (!isEqual(get<Id::delayTime1>(), value)) {get<Id::delayTime1>() = value;m_modified = true;}
break;
 case Id::delayTime2: if (!isEqual(get<Id::delayTime2>(), value)) {get<Id::delayTime2>() = value;m_modified = true;}
break;
 case Id::delayTime3: if (!isEqual(get<Id::delayTime3>(), value)) {get<Id::delayTime3>() = value;m_modified = true;}
break;
 case Id::delayTime4: if (!isEqual(get<Id::delayTime4>(), value)) {get<Id::delayTime4>() = value;m_modified = true;}
break;
 case Id::delayLevel1: if (!isEqual(get<Id::delayLevel1>(), value)) {get<Id::delayLevel1>() = value;m_modified = true;}
break;
 case Id::delayLevel2: if (!isEqual(get<Id::delayLevel2>(), value)) {get<Id::delayLevel2>() = value;m_modified = true;}
break;
 case Id::delayLevel3: if (!isEqual(get<Id::delayLevel3>(), value)) {get<Id::delayLevel3>() = value;m_modified = true;}
break;
 case Id::delayLevel4: if (!isEqual(get<Id::delayLevel4>(), value)) {get<Id::delayLevel4>() = value;m_modified = true;}
break;
 case Id::feedback1: if (!isEqual(get<Id::feedback1>(), value)) {get<Id::feedback1>() = value;m_modified = true;}
break;
 case Id::feedback2: if (!isEqual(get<Id::feedback2>(), value)) {get<Id::feedback2>() = value;m_modified = true;}
break;
 case Id::feedback3: if (!isEqual(get<Id::feedback3>(), value)) {get<Id::feedback3>() = value;m_modified = true;}
break;
 case Id::feedback4: if (!isEqual(get<Id::feedback4>(), value)) {get<Id::feedback4>() = value;m_modified = true;}
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
        feedGain         , // dial
        tapeSpeed        , // dial
        wow              , // dial
        hysteresis       , // dial
        saturation       , // dial
        noiseFloor       , // dial
        noiseDistribution, // dial
        delayTime1       , // dial
        delayTime2       , // dial
        delayTime3       , // dial
        delayTime4       , // dial
        delayLevel1      , // dial
        delayLevel2      , // dial
        delayLevel3      , // dial
        delayLevel4      , // dial
        feedback1        , // dial
        feedback2        , // dial
        feedback3        , // dial
        feedback4         // dial
)
