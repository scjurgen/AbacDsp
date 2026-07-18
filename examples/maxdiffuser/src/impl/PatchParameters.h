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
        dry            , // dial
        wet            , // dial
        preDelay       , // dial
        elements       , // dial
        feedback       , // dial
        bulge          , // dial
        bottomSize     , // dial
        topSize        , // dial
        modulationDepth, // dial
        modulationSpeed, // dial
        lowPass        , // dial
        mix            , // dial
        pitch          , // dial
        psola          , // switch
        fdnMix         , // dial
        fdnSize        , // dial
        fdnDecay        // dial
    };
float dry{0.0f};
float wet{-6.0f};
float preDelay{0.0f};
float elements{6.0f};
float feedback{50.0f};
float bulge{0.46f};
float bottomSize{100.0f};
float topSize{1000.0f};
float modulationDepth{0.0f};
float modulationSpeed{0.5f};
float lowPass{12000.0f};
float mix{0.0f};
float pitch{0.0f};
bool psola{false};
float fdnMix{-100.0f};
float fdnSize{30.0f};
float fdnDecay{2000.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "dry",
"wet",
"preDelay",
"elements",
"feedback",
"bulge",
"bottomSize",
"topSize",
"modulationDepth",
"modulationSpeed",
"lowPass",
"mix",
"pitch",
"psola",
"fdnMix",
"fdnSize",
"fdnDecay"
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
        if constexpr (ParamId == Id::dry) return dry;
        else if constexpr (ParamId == Id::wet) return wet;
        else if constexpr (ParamId == Id::preDelay) return preDelay;
        else if constexpr (ParamId == Id::elements) return elements;
        else if constexpr (ParamId == Id::feedback) return feedback;
        else if constexpr (ParamId == Id::bulge) return bulge;
        else if constexpr (ParamId == Id::bottomSize) return bottomSize;
        else if constexpr (ParamId == Id::topSize) return topSize;
        else if constexpr (ParamId == Id::modulationDepth) return modulationDepth;
        else if constexpr (ParamId == Id::modulationSpeed) return modulationSpeed;
        else if constexpr (ParamId == Id::lowPass) return lowPass;
        else if constexpr (ParamId == Id::mix) return mix;
        else if constexpr (ParamId == Id::pitch) return pitch;
        else if constexpr (ParamId == Id::psola) return psola;
        else if constexpr (ParamId == Id::fdnMix) return fdnMix;
        else if constexpr (ParamId == Id::fdnSize) return fdnSize;
        else if constexpr (ParamId == Id::fdnDecay) return fdnDecay;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::dry: if (!isEqual(get<Id::dry>(), value)) {get<Id::dry>() = value;m_modified = true;}
break;
 case Id::wet: if (!isEqual(get<Id::wet>(), value)) {get<Id::wet>() = value;m_modified = true;}
break;
 case Id::preDelay: if (!isEqual(get<Id::preDelay>(), value)) {get<Id::preDelay>() = value;m_modified = true;}
break;
 case Id::elements: if (!isEqual(get<Id::elements>(), value)) {get<Id::elements>() = value;m_modified = true;}
break;
 case Id::feedback: if (!isEqual(get<Id::feedback>(), value)) {get<Id::feedback>() = value;m_modified = true;}
break;
 case Id::bulge: if (!isEqual(get<Id::bulge>(), value)) {get<Id::bulge>() = value;m_modified = true;}
break;
 case Id::bottomSize: if (!isEqual(get<Id::bottomSize>(), value)) {get<Id::bottomSize>() = value;m_modified = true;}
break;
 case Id::topSize: if (!isEqual(get<Id::topSize>(), value)) {get<Id::topSize>() = value;m_modified = true;}
break;
 case Id::modulationDepth: if (!isEqual(get<Id::modulationDepth>(), value)) {get<Id::modulationDepth>() = value;m_modified = true;}
break;
 case Id::modulationSpeed: if (!isEqual(get<Id::modulationSpeed>(), value)) {get<Id::modulationSpeed>() = value;m_modified = true;}
break;
 case Id::lowPass: if (!isEqual(get<Id::lowPass>(), value)) {get<Id::lowPass>() = value;m_modified = true;}
break;
 case Id::mix: if (!isEqual(get<Id::mix>(), value)) {get<Id::mix>() = value;m_modified = true;}
break;
 case Id::pitch: if (!isEqual(get<Id::pitch>(), value)) {get<Id::pitch>() = value;m_modified = true;}
break;
 case Id::psola: if (!isEqual(get<Id::psola>(), value)) {get<Id::psola>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::fdnMix: if (!isEqual(get<Id::fdnMix>(), value)) {get<Id::fdnMix>() = value;m_modified = true;}
break;
 case Id::fdnSize: if (!isEqual(get<Id::fdnSize>(), value)) {get<Id::fdnSize>() = value;m_modified = true;}
break;
 case Id::fdnDecay: if (!isEqual(get<Id::fdnDecay>(), value)) {get<Id::fdnDecay>() = value;m_modified = true;}
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
        dry            , // dial
        wet            , // dial
        preDelay       , // dial
        elements       , // dial
        feedback       , // dial
        bulge          , // dial
        bottomSize     , // dial
        topSize        , // dial
        modulationDepth, // dial
        modulationSpeed, // dial
        lowPass        , // dial
        mix            , // dial
        pitch          , // dial
        psola          , // switch
        fdnMix         , // dial
        fdnSize        , // dial
        fdnDecay        // dial
)
