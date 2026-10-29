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
        bpm            , // dial
        clickOn        , // switch
        latencyComp    , // dial
        instrument     , // drop
        input          , // drop
        outputVolume   , // dial
        inputGain      , // dial
        instrumentLevel, // dial
        clickLevel     , // dial
        listen         , // switch
        play           , // switch
        replay         , // switch
        stop           , // switch
        next           , // switch
        reference      , // switch
        saveTakes      , // switch
        script          // script
    };
float bpm{100.0f};
bool clickOn{true};
float latencyComp{0.0f};
size_t instrument{0};
size_t input{0};
float outputVolume{0.0f};
float inputGain{0.0f};
float instrumentLevel{0.0f};
float clickLevel{0.0f};
bool listen{false};
bool play{false};
bool replay{false};
bool stop{false};
bool next{false};
bool reference{false};
bool saveTakes{false};
std::string script{};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "bpm",
"clickOn",
"latencyComp",
"instrument",
"input",
"outputVolume",
"inputGain",
"instrumentLevel",
"clickLevel",
"listen",
"play",
"replay",
"stop",
"next",
"reference",
"saveTakes"
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
        if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::clickOn) return clickOn;
        else if constexpr (ParamId == Id::latencyComp) return latencyComp;
        else if constexpr (ParamId == Id::instrument) return instrument;
        else if constexpr (ParamId == Id::input) return input;
        else if constexpr (ParamId == Id::outputVolume) return outputVolume;
        else if constexpr (ParamId == Id::inputGain) return inputGain;
        else if constexpr (ParamId == Id::instrumentLevel) return instrumentLevel;
        else if constexpr (ParamId == Id::clickLevel) return clickLevel;
        else if constexpr (ParamId == Id::listen) return listen;
        else if constexpr (ParamId == Id::play) return play;
        else if constexpr (ParamId == Id::replay) return replay;
        else if constexpr (ParamId == Id::stop) return stop;
        else if constexpr (ParamId == Id::next) return next;
        else if constexpr (ParamId == Id::reference) return reference;
        else if constexpr (ParamId == Id::saveTakes) return saveTakes;
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
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::clickOn: if (!isEqual(get<Id::clickOn>(), value)) {get<Id::clickOn>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::latencyComp: if (!isEqual(get<Id::latencyComp>(), value)) {get<Id::latencyComp>() = value;m_modified = true;}
break;
 case Id::instrument: if (!isEqual(get<Id::instrument>(), value)) {get<Id::instrument>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::input: if (!isEqual(get<Id::input>(), value)) {get<Id::input>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::outputVolume: if (!isEqual(get<Id::outputVolume>(), value)) {get<Id::outputVolume>() = value;m_modified = true;}
break;
 case Id::inputGain: if (!isEqual(get<Id::inputGain>(), value)) {get<Id::inputGain>() = value;m_modified = true;}
break;
 case Id::instrumentLevel: if (!isEqual(get<Id::instrumentLevel>(), value)) {get<Id::instrumentLevel>() = value;m_modified = true;}
break;
 case Id::clickLevel: if (!isEqual(get<Id::clickLevel>(), value)) {get<Id::clickLevel>() = value;m_modified = true;}
break;
 case Id::listen: if (!isEqual(get<Id::listen>(), value)) {get<Id::listen>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::play: if (!isEqual(get<Id::play>(), value)) {get<Id::play>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::replay: if (!isEqual(get<Id::replay>(), value)) {get<Id::replay>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::stop: if (!isEqual(get<Id::stop>(), value)) {get<Id::stop>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::next: if (!isEqual(get<Id::next>(), value)) {get<Id::next>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::reference: if (!isEqual(get<Id::reference>(), value)) {get<Id::reference>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::saveTakes: if (!isEqual(get<Id::saveTakes>(), value)) {get<Id::saveTakes>() = static_cast<bool>(value) ;m_modified = true;}
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
        bpm            , // dial
        clickOn        , // switch
        latencyComp    , // dial
        instrument     , // drop
        input          , // drop
        outputVolume   , // dial
        inputGain      , // dial
        instrumentLevel, // dial
        clickLevel     , // dial
        listen         , // switch
        play           , // switch
        replay         , // switch
        stop           , // switch
        next           , // switch
        reference      , // switch
        saveTakes      , // switch
        script          // script
)
