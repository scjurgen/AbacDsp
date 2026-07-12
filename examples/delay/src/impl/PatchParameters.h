#pragma once

#include <array>
#include <cmath>
#include <string_view>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// clang-format off
struct PatchParameters
{
    enum class Id : int
    {
        gain        , // dial
        dry         , // dial
        wet         , // dial
        timeInMs    , // dial
        hostSync    , // switch
        syncDivision, // drop
        feedback    , // dial
        lowPass     , // dial
        highPass    , // dial
        allPass     , // dial
        modDepth    , // dial
        modSpeed     // dial
    };
float gain{0.0f};
float dry{0.0f};
float wet{0.0f};
float timeInMs{200.0f};
bool hostSync{false};
size_t syncDivision{4};
float feedback{0.0f};
float lowPass{12000.0f};
float highPass{50.0f};
float allPass{1500.0f};
float modDepth{0.0f};
float modSpeed{0.25f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "gain",
"dry",
"wet",
"timeInMs",
"hostSync",
"syncDivision",
"feedback",
"lowPass",
"highPass",
"allPass",
"modDepth",
"modSpeed"
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
        if constexpr (ParamId == Id::gain) return gain;
        else if constexpr (ParamId == Id::dry) return dry;
        else if constexpr (ParamId == Id::wet) return wet;
        else if constexpr (ParamId == Id::timeInMs) return timeInMs;
        else if constexpr (ParamId == Id::hostSync) return hostSync;
        else if constexpr (ParamId == Id::syncDivision) return syncDivision;
        else if constexpr (ParamId == Id::feedback) return feedback;
        else if constexpr (ParamId == Id::lowPass) return lowPass;
        else if constexpr (ParamId == Id::highPass) return highPass;
        else if constexpr (ParamId == Id::allPass) return allPass;
        else if constexpr (ParamId == Id::modDepth) return modDepth;
        else if constexpr (ParamId == Id::modSpeed) return modSpeed;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::gain: if (!isEqual(get<Id::gain>(), value)) {get<Id::gain>() = value;m_modified = true;}
break;
 case Id::dry: if (!isEqual(get<Id::dry>(), value)) {get<Id::dry>() = value;m_modified = true;}
break;
 case Id::wet: if (!isEqual(get<Id::wet>(), value)) {get<Id::wet>() = value;m_modified = true;}
break;
 case Id::timeInMs: if (!isEqual(get<Id::timeInMs>(), value)) {get<Id::timeInMs>() = value;m_modified = true;}
break;
 case Id::hostSync: if (!isEqual(get<Id::hostSync>(), value)) {get<Id::hostSync>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::syncDivision: if (!isEqual(get<Id::syncDivision>(), value)) {get<Id::syncDivision>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::feedback: if (!isEqual(get<Id::feedback>(), value)) {get<Id::feedback>() = value;m_modified = true;}
break;
 case Id::lowPass: if (!isEqual(get<Id::lowPass>(), value)) {get<Id::lowPass>() = value;m_modified = true;}
break;
 case Id::highPass: if (!isEqual(get<Id::highPass>(), value)) {get<Id::highPass>() = value;m_modified = true;}
break;
 case Id::allPass: if (!isEqual(get<Id::allPass>(), value)) {get<Id::allPass>() = value;m_modified = true;}
break;
 case Id::modDepth: if (!isEqual(get<Id::modDepth>(), value)) {get<Id::modDepth>() = value;m_modified = true;}
break;
 case Id::modSpeed: if (!isEqual(get<Id::modSpeed>(), value)) {get<Id::modSpeed>() = value;m_modified = true;}
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
        gain        , // dial
        dry         , // dial
        wet         , // dial
        timeInMs    , // dial
        hostSync    , // switch
        syncDivision, // drop
        feedback    , // dial
        lowPass     , // dial
        highPass    , // dial
        allPass     , // dial
        modDepth    , // dial
        modSpeed     // dial
)
