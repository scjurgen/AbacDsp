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
        wave      , // drop
        vol       , // dial
        frequency , // dial
        deformType, // drop
        deform    , // dial
        gain      , // dial
        offset    , // dial
        clip      , // dial
        phase     , // dial
        smooth    , // dial
        outputType, // drop
        discrete   // dial
    };
size_t wave{0};
float vol{0.0f};
float frequency{1.0f};
size_t deformType{0};
float deform{0.0f};
float gain{1.0f};
float offset{0.0f};
float clip{1.0f};
float phase{0.0f};
float smooth{0.0f};
size_t outputType{0};
float discrete{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "wave",
"vol",
"frequency",
"deformType",
"deform",
"gain",
"offset",
"clip",
"phase",
"smooth",
"outputType",
"discrete"
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
        if constexpr (ParamId == Id::wave) return wave;
        else if constexpr (ParamId == Id::vol) return vol;
        else if constexpr (ParamId == Id::frequency) return frequency;
        else if constexpr (ParamId == Id::deformType) return deformType;
        else if constexpr (ParamId == Id::deform) return deform;
        else if constexpr (ParamId == Id::gain) return gain;
        else if constexpr (ParamId == Id::offset) return offset;
        else if constexpr (ParamId == Id::clip) return clip;
        else if constexpr (ParamId == Id::phase) return phase;
        else if constexpr (ParamId == Id::smooth) return smooth;
        else if constexpr (ParamId == Id::outputType) return outputType;
        else if constexpr (ParamId == Id::discrete) return discrete;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::wave: if (!isEqual(get<Id::wave>(), value)) {get<Id::wave>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::vol: if (!isEqual(get<Id::vol>(), value)) {get<Id::vol>() = value;m_modified = true;}
break;
 case Id::frequency: if (!isEqual(get<Id::frequency>(), value)) {get<Id::frequency>() = value;m_modified = true;}
break;
 case Id::deformType: if (!isEqual(get<Id::deformType>(), value)) {get<Id::deformType>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::deform: if (!isEqual(get<Id::deform>(), value)) {get<Id::deform>() = value;m_modified = true;}
break;
 case Id::gain: if (!isEqual(get<Id::gain>(), value)) {get<Id::gain>() = value;m_modified = true;}
break;
 case Id::offset: if (!isEqual(get<Id::offset>(), value)) {get<Id::offset>() = value;m_modified = true;}
break;
 case Id::clip: if (!isEqual(get<Id::clip>(), value)) {get<Id::clip>() = value;m_modified = true;}
break;
 case Id::phase: if (!isEqual(get<Id::phase>(), value)) {get<Id::phase>() = value;m_modified = true;}
break;
 case Id::smooth: if (!isEqual(get<Id::smooth>(), value)) {get<Id::smooth>() = value;m_modified = true;}
break;
 case Id::outputType: if (!isEqual(get<Id::outputType>(), value)) {get<Id::outputType>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::discrete: if (!isEqual(get<Id::discrete>(), value)) {get<Id::discrete>() = value;m_modified = true;}
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
        wave      , // drop
        vol       , // dial
        frequency , // dial
        deformType, // drop
        deform    , // dial
        gain      , // dial
        offset    , // dial
        clip      , // dial
        phase     , // dial
        smooth    , // dial
        outputType, // drop
        discrete   // dial
)
