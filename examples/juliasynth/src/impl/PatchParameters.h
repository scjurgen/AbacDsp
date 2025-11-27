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
        vol  , // dial
        power, // dial
        cx   , // dial
        cy   , // dial
        jx   , // dial
        jy   , // dial
        jrx  , // dial
        jry   // dial
    };
float vol{};
float power{};
float cx{};
float cy{};
float jx{};
float jy{};
float jrx{};
float jry{};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "vol",
"power",
"cx",
"cy",
"jx",
"jy",
"jrx",
"jry"
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
        if constexpr (ParamId == Id::vol) return vol;
        else if constexpr (ParamId == Id::power) return power;
        else if constexpr (ParamId == Id::cx) return cx;
        else if constexpr (ParamId == Id::cy) return cy;
        else if constexpr (ParamId == Id::jx) return jx;
        else if constexpr (ParamId == Id::jy) return jy;
        else if constexpr (ParamId == Id::jrx) return jrx;
        else if constexpr (ParamId == Id::jry) return jry;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::vol: if (!isEqual(get<Id::vol>(), value)) {get<Id::vol>() = value;m_modified = true;}
break;
 case Id::power: if (!isEqual(get<Id::power>(), value)) {get<Id::power>() = value;m_modified = true;}
break;
 case Id::cx: if (!isEqual(get<Id::cx>(), value)) {get<Id::cx>() = value;m_modified = true;}
break;
 case Id::cy: if (!isEqual(get<Id::cy>(), value)) {get<Id::cy>() = value;m_modified = true;}
break;
 case Id::jx: if (!isEqual(get<Id::jx>(), value)) {get<Id::jx>() = value;m_modified = true;}
break;
 case Id::jy: if (!isEqual(get<Id::jy>(), value)) {get<Id::jy>() = value;m_modified = true;}
break;
 case Id::jrx: if (!isEqual(get<Id::jrx>(), value)) {get<Id::jrx>() = value;m_modified = true;}
break;
 case Id::jry: if (!isEqual(get<Id::jry>(), value)) {get<Id::jry>() = value;m_modified = true;}
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
        vol  , // dial
        power, // dial
        cx   , // dial
        cy   , // dial
        jx   , // dial
        jy   , // dial
        jrx  , // dial
        jry   // dial
)
