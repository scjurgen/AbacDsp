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
        dry          , // dial
        wet          , // dial
        bpm          , // dial
        hostSync     , // switch
        division     , // drop
        feedback     , // dial
        feedbackBeats, // dial
        reverbWet    , // dial
        reverbSize   , // dial
        reverbDecay  , // dial
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
float dry{0.0f};
float wet{-6.0f};
float bpm{120.0f};
bool hostSync{false};
size_t division{4};
float feedback{0.0f};
float feedbackBeats{1.0f};
float reverbWet{-100.0f};
float reverbSize{15.0f};
float reverbDecay{2000.0f};
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
        "dry",
"wet",
"bpm",
"hostSync",
"division",
"feedback",
"feedbackBeats",
"reverbWet",
"reverbSize",
"reverbDecay",
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
        if constexpr (ParamId == Id::dry) return dry;
        else if constexpr (ParamId == Id::wet) return wet;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::hostSync) return hostSync;
        else if constexpr (ParamId == Id::division) return division;
        else if constexpr (ParamId == Id::feedback) return feedback;
        else if constexpr (ParamId == Id::feedbackBeats) return feedbackBeats;
        else if constexpr (ParamId == Id::reverbWet) return reverbWet;
        else if constexpr (ParamId == Id::reverbSize) return reverbSize;
        else if constexpr (ParamId == Id::reverbDecay) return reverbDecay;
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
 case Id::dry: if (!isEqual(get<Id::dry>(), value)) {get<Id::dry>() = value;m_modified = true;}
break;
 case Id::wet: if (!isEqual(get<Id::wet>(), value)) {get<Id::wet>() = value;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::hostSync: if (!isEqual(get<Id::hostSync>(), value)) {get<Id::hostSync>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::division: if (!isEqual(get<Id::division>(), value)) {get<Id::division>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::feedback: if (!isEqual(get<Id::feedback>(), value)) {get<Id::feedback>() = value;m_modified = true;}
break;
 case Id::feedbackBeats: if (!isEqual(get<Id::feedbackBeats>(), value)) {get<Id::feedbackBeats>() = value;m_modified = true;}
break;
 case Id::reverbWet: if (!isEqual(get<Id::reverbWet>(), value)) {get<Id::reverbWet>() = value;m_modified = true;}
break;
 case Id::reverbSize: if (!isEqual(get<Id::reverbSize>(), value)) {get<Id::reverbSize>() = value;m_modified = true;}
break;
 case Id::reverbDecay: if (!isEqual(get<Id::reverbDecay>(), value)) {get<Id::reverbDecay>() = value;m_modified = true;}
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
        return a == (static_cast<int>(round(b)))?false:true;
    }
    bool m_modified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PatchParameters,
        dry          , // dial
        wet          , // dial
        bpm          , // dial
        hostSync     , // switch
        division     , // drop
        feedback     , // dial
        feedbackBeats, // dial
        reverbWet    , // dial
        reverbSize   , // dial
        reverbDecay  , // dial
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
