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
        configuration, // drop
        tone         , // dial
        speed        , // dial
        tapeSpeed    , // dial
        depth        , // dial
        feedback     , // dial
        mix          , // dial
        luaParam1    , // dial
        luaParam2    , // dial
        luaParam3    , // dial
        luaParam4    , // dial
        luaParam5    , // dial
        luaParam6    , // dial
        luaParam7    , // dial
        luaParam8    , // dial
        script        // script
    };
size_t configuration{0};
float tone{50.0f};
float speed{0.8f};
float tapeSpeed{0.0f};
float depth{50.0f};
float feedback{0.0f};
float mix{50.0f};
float luaParam1{0.0f};
float luaParam2{0.0f};
float luaParam3{0.0f};
float luaParam4{0.0f};
float luaParam5{0.0f};
float luaParam6{0.0f};
float luaParam7{0.0f};
float luaParam8{0.0f};
std::string script{};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "configuration",
"tone",
"speed",
"tapeSpeed",
"depth",
"feedback",
"mix",
"luaParam1",
"luaParam2",
"luaParam3",
"luaParam4",
"luaParam5",
"luaParam6",
"luaParam7",
"luaParam8"
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
        if constexpr (ParamId == Id::configuration) return configuration;
        else if constexpr (ParamId == Id::tone) return tone;
        else if constexpr (ParamId == Id::speed) return speed;
        else if constexpr (ParamId == Id::tapeSpeed) return tapeSpeed;
        else if constexpr (ParamId == Id::depth) return depth;
        else if constexpr (ParamId == Id::feedback) return feedback;
        else if constexpr (ParamId == Id::mix) return mix;
        else if constexpr (ParamId == Id::luaParam1) return luaParam1;
        else if constexpr (ParamId == Id::luaParam2) return luaParam2;
        else if constexpr (ParamId == Id::luaParam3) return luaParam3;
        else if constexpr (ParamId == Id::luaParam4) return luaParam4;
        else if constexpr (ParamId == Id::luaParam5) return luaParam5;
        else if constexpr (ParamId == Id::luaParam6) return luaParam6;
        else if constexpr (ParamId == Id::luaParam7) return luaParam7;
        else if constexpr (ParamId == Id::luaParam8) return luaParam8;
        else if constexpr (ParamId == Id::script) return script;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::configuration: if (!isEqual(get<Id::configuration>(), value)) {get<Id::configuration>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::tone: if (!isEqual(get<Id::tone>(), value)) {get<Id::tone>() = value;m_modified = true;}
break;
 case Id::speed: if (!isEqual(get<Id::speed>(), value)) {get<Id::speed>() = value;m_modified = true;}
break;
 case Id::tapeSpeed: if (!isEqual(get<Id::tapeSpeed>(), value)) {get<Id::tapeSpeed>() = value;m_modified = true;}
break;
 case Id::depth: if (!isEqual(get<Id::depth>(), value)) {get<Id::depth>() = value;m_modified = true;}
break;
 case Id::feedback: if (!isEqual(get<Id::feedback>(), value)) {get<Id::feedback>() = value;m_modified = true;}
break;
 case Id::mix: if (!isEqual(get<Id::mix>(), value)) {get<Id::mix>() = value;m_modified = true;}
break;
 case Id::luaParam1: if (!isEqual(get<Id::luaParam1>(), value)) {get<Id::luaParam1>() = value;m_modified = true;}
break;
 case Id::luaParam2: if (!isEqual(get<Id::luaParam2>(), value)) {get<Id::luaParam2>() = value;m_modified = true;}
break;
 case Id::luaParam3: if (!isEqual(get<Id::luaParam3>(), value)) {get<Id::luaParam3>() = value;m_modified = true;}
break;
 case Id::luaParam4: if (!isEqual(get<Id::luaParam4>(), value)) {get<Id::luaParam4>() = value;m_modified = true;}
break;
 case Id::luaParam5: if (!isEqual(get<Id::luaParam5>(), value)) {get<Id::luaParam5>() = value;m_modified = true;}
break;
 case Id::luaParam6: if (!isEqual(get<Id::luaParam6>(), value)) {get<Id::luaParam6>() = value;m_modified = true;}
break;
 case Id::luaParam7: if (!isEqual(get<Id::luaParam7>(), value)) {get<Id::luaParam7>() = value;m_modified = true;}
break;
 case Id::luaParam8: if (!isEqual(get<Id::luaParam8>(), value)) {get<Id::luaParam8>() = value;m_modified = true;}
break;
 case Id::script: break;

            default:
                break;
        }
    }

void updateScript(const std::string& value) { if (script != value) { script = value; m_modified = true; } }


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
        return a == static_cast<int>(round(b));
    }
    bool m_modified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PatchParameters,
        configuration, // drop
        tone         , // dial
        speed        , // dial
        tapeSpeed    , // dial
        depth        , // dial
        feedback     , // dial
        mix          , // dial
        luaParam1    , // dial
        luaParam2    , // dial
        luaParam3    , // dial
        luaParam4    , // dial
        luaParam5    , // dial
        luaParam6    , // dial
        luaParam7    , // dial
        luaParam8    , // dial
        script        // script
)
