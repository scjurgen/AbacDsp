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
        onOff          , // switch
        input          , // dial
        modulationDepth, // dial
        mix            , // dial
        density        , // dial
        threshold      , // dial
        knee            // dial
    };
bool onOff{false};
float input{2.0f};
float modulationDepth{2.0f};
float mix{0.0f};
float density{1.0f};
float threshold{1.0f};
float knee{1.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "onOff",
"input",
"modulationDepth",
"mix",
"density",
"threshold",
"knee"
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
        if constexpr (ParamId == Id::onOff) return onOff;
        else if constexpr (ParamId == Id::input) return input;
        else if constexpr (ParamId == Id::modulationDepth) return modulationDepth;
        else if constexpr (ParamId == Id::mix) return mix;
        else if constexpr (ParamId == Id::density) return density;
        else if constexpr (ParamId == Id::threshold) return threshold;
        else if constexpr (ParamId == Id::knee) return knee;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::onOff: if (!isEqual(get<Id::onOff>(), value)) {get<Id::onOff>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::input: if (!isEqual(get<Id::input>(), value)) {get<Id::input>() = value;m_modified = true;}
break;
 case Id::modulationDepth: if (!isEqual(get<Id::modulationDepth>(), value)) {get<Id::modulationDepth>() = value;m_modified = true;}
break;
 case Id::mix: if (!isEqual(get<Id::mix>(), value)) {get<Id::mix>() = value;m_modified = true;}
break;
 case Id::density: if (!isEqual(get<Id::density>(), value)) {get<Id::density>() = value;m_modified = true;}
break;
 case Id::threshold: if (!isEqual(get<Id::threshold>(), value)) {get<Id::threshold>() = value;m_modified = true;}
break;
 case Id::knee: if (!isEqual(get<Id::knee>(), value)) {get<Id::knee>() = value;m_modified = true;}
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
        onOff          , // switch
        input          , // dial
        modulationDepth, // dial
        mix            , // dial
        density        , // dial
        threshold      , // dial
        knee            // dial
)
