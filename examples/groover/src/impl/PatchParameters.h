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
        play           , // switch
        hostSync       , // switch
        bpm            , // dial
        grooveVariation, // dial
        outputLevel    , // dial
        inputGain      , // dial
        push           , // dial
        life            // dial
    };
bool play{false};
bool hostSync{false};
float bpm{120.0f};
float grooveVariation{0.0f};
float outputLevel{0.0f};
float inputGain{0.0f};
float push{0.0f};
float life{100.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "play",
"hostSync",
"bpm",
"grooveVariation",
"outputLevel",
"inputGain",
"push",
"life"
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
        if constexpr (ParamId == Id::play) return play;
        else if constexpr (ParamId == Id::hostSync) return hostSync;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::grooveVariation) return grooveVariation;
        else if constexpr (ParamId == Id::outputLevel) return outputLevel;
        else if constexpr (ParamId == Id::inputGain) return inputGain;
        else if constexpr (ParamId == Id::push) return push;
        else if constexpr (ParamId == Id::life) return life;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::play: if (!isEqual(get<Id::play>(), value)) {get<Id::play>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::hostSync: if (!isEqual(get<Id::hostSync>(), value)) {get<Id::hostSync>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::grooveVariation: if (!isEqual(get<Id::grooveVariation>(), value)) {get<Id::grooveVariation>() = value;m_modified = true;}
break;
 case Id::outputLevel: if (!isEqual(get<Id::outputLevel>(), value)) {get<Id::outputLevel>() = value;m_modified = true;}
break;
 case Id::inputGain: if (!isEqual(get<Id::inputGain>(), value)) {get<Id::inputGain>() = value;m_modified = true;}
break;
 case Id::push: if (!isEqual(get<Id::push>(), value)) {get<Id::push>() = value;m_modified = true;}
break;
 case Id::life: if (!isEqual(get<Id::life>(), value)) {get<Id::life>() = value;m_modified = true;}
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
        play           , // switch
        hostSync       , // switch
        bpm            , // dial
        grooveVariation, // dial
        outputLevel    , // dial
        inputGain      , // dial
        push           , // dial
        life            // dial
)
