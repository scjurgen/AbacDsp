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
        numChains   , // dial
        dry         , // dial
        wet         , // dial
        lowFreq     , // dial
        highFreq    , // dial
        distribution, // drop
        decayMin    , // dial
        decayMax    , // dial
        gainMin     , // dial
        gainMax     , // dial
        delayMin    , // dial
        delayMax    , // dial
        q            // dial
    };
float numChains{48.0f};
float dry{0.0f};
float wet{-6.0f};
float lowFreq{80.0f};
float highFreq{6000.0f};
size_t distribution{1};
float decayMin{0.3f};
float decayMax{4.0f};
float gainMin{-18.0f};
float gainMax{0.0f};
float delayMin{0.0f};
float delayMax{300.0f};
float q{6.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "numChains",
"dry",
"wet",
"lowFreq",
"highFreq",
"distribution",
"decayMin",
"decayMax",
"gainMin",
"gainMax",
"delayMin",
"delayMax",
"q"
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
        if constexpr (ParamId == Id::numChains) return numChains;
        else if constexpr (ParamId == Id::dry) return dry;
        else if constexpr (ParamId == Id::wet) return wet;
        else if constexpr (ParamId == Id::lowFreq) return lowFreq;
        else if constexpr (ParamId == Id::highFreq) return highFreq;
        else if constexpr (ParamId == Id::distribution) return distribution;
        else if constexpr (ParamId == Id::decayMin) return decayMin;
        else if constexpr (ParamId == Id::decayMax) return decayMax;
        else if constexpr (ParamId == Id::gainMin) return gainMin;
        else if constexpr (ParamId == Id::gainMax) return gainMax;
        else if constexpr (ParamId == Id::delayMin) return delayMin;
        else if constexpr (ParamId == Id::delayMax) return delayMax;
        else if constexpr (ParamId == Id::q) return q;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::numChains: if (!isEqual(get<Id::numChains>(), value)) {get<Id::numChains>() = value;m_modified = true;}
break;
 case Id::dry: if (!isEqual(get<Id::dry>(), value)) {get<Id::dry>() = value;m_modified = true;}
break;
 case Id::wet: if (!isEqual(get<Id::wet>(), value)) {get<Id::wet>() = value;m_modified = true;}
break;
 case Id::lowFreq: if (!isEqual(get<Id::lowFreq>(), value)) {get<Id::lowFreq>() = value;m_modified = true;}
break;
 case Id::highFreq: if (!isEqual(get<Id::highFreq>(), value)) {get<Id::highFreq>() = value;m_modified = true;}
break;
 case Id::distribution: if (!isEqual(get<Id::distribution>(), value)) {get<Id::distribution>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::decayMin: if (!isEqual(get<Id::decayMin>(), value)) {get<Id::decayMin>() = value;m_modified = true;}
break;
 case Id::decayMax: if (!isEqual(get<Id::decayMax>(), value)) {get<Id::decayMax>() = value;m_modified = true;}
break;
 case Id::gainMin: if (!isEqual(get<Id::gainMin>(), value)) {get<Id::gainMin>() = value;m_modified = true;}
break;
 case Id::gainMax: if (!isEqual(get<Id::gainMax>(), value)) {get<Id::gainMax>() = value;m_modified = true;}
break;
 case Id::delayMin: if (!isEqual(get<Id::delayMin>(), value)) {get<Id::delayMin>() = value;m_modified = true;}
break;
 case Id::delayMax: if (!isEqual(get<Id::delayMax>(), value)) {get<Id::delayMax>() = value;m_modified = true;}
break;
 case Id::q: if (!isEqual(get<Id::q>(), value)) {get<Id::q>() = value;m_modified = true;}
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
        numChains   , // dial
        dry         , // dial
        wet         , // dial
        lowFreq     , // dial
        highFreq    , // dial
        distribution, // drop
        decayMin    , // dial
        decayMax    , // dial
        gainMin     , // dial
        gainMax     , // dial
        delayMin    , // dial
        delayMax    , // dial
        q            // dial
)
