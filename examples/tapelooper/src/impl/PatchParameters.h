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
        tapeSpeed      , // dial
        bars           , // dial
        inputGain      , // dial
        grooveLevel    , // dial
        recordA        , // switch
        playA          , // switch
        clearA         , // switch
        recordB        , // switch
        playB          , // switch
        clearB         , // switch
        recordC        , // switch
        playC          , // switch
        clearC         , // switch
        groovePlay     , // switch
        bpm            , // dial
        grooveVariation // dial
    };
float tapeSpeed{1.0f};
float bars{8.0f};
float inputGain{0.0f};
float grooveLevel{0.0f};
bool recordA{false};
bool playA{false};
bool clearA{false};
bool recordB{false};
bool playB{false};
bool clearB{false};
bool recordC{false};
bool playC{false};
bool clearC{false};
bool groovePlay{false};
float bpm{120.0f};
float grooveVariation{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "tapeSpeed",
"bars",
"inputGain",
"grooveLevel",
"recordA",
"playA",
"clearA",
"recordB",
"playB",
"clearB",
"recordC",
"playC",
"clearC",
"groovePlay",
"bpm",
"grooveVariation"
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
        if constexpr (ParamId == Id::tapeSpeed) return tapeSpeed;
        else if constexpr (ParamId == Id::bars) return bars;
        else if constexpr (ParamId == Id::inputGain) return inputGain;
        else if constexpr (ParamId == Id::grooveLevel) return grooveLevel;
        else if constexpr (ParamId == Id::recordA) return recordA;
        else if constexpr (ParamId == Id::playA) return playA;
        else if constexpr (ParamId == Id::clearA) return clearA;
        else if constexpr (ParamId == Id::recordB) return recordB;
        else if constexpr (ParamId == Id::playB) return playB;
        else if constexpr (ParamId == Id::clearB) return clearB;
        else if constexpr (ParamId == Id::recordC) return recordC;
        else if constexpr (ParamId == Id::playC) return playC;
        else if constexpr (ParamId == Id::clearC) return clearC;
        else if constexpr (ParamId == Id::groovePlay) return groovePlay;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::grooveVariation) return grooveVariation;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::tapeSpeed: if (!isEqual(get<Id::tapeSpeed>(), value)) {get<Id::tapeSpeed>() = value;m_modified = true;}
break;
 case Id::bars: if (!isEqual(get<Id::bars>(), value)) {get<Id::bars>() = value;m_modified = true;}
break;
 case Id::inputGain: if (!isEqual(get<Id::inputGain>(), value)) {get<Id::inputGain>() = value;m_modified = true;}
break;
 case Id::grooveLevel: if (!isEqual(get<Id::grooveLevel>(), value)) {get<Id::grooveLevel>() = value;m_modified = true;}
break;
 case Id::recordA: if (!isEqual(get<Id::recordA>(), value)) {get<Id::recordA>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::playA: if (!isEqual(get<Id::playA>(), value)) {get<Id::playA>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearA: if (!isEqual(get<Id::clearA>(), value)) {get<Id::clearA>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::recordB: if (!isEqual(get<Id::recordB>(), value)) {get<Id::recordB>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::playB: if (!isEqual(get<Id::playB>(), value)) {get<Id::playB>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearB: if (!isEqual(get<Id::clearB>(), value)) {get<Id::clearB>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::recordC: if (!isEqual(get<Id::recordC>(), value)) {get<Id::recordC>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::playC: if (!isEqual(get<Id::playC>(), value)) {get<Id::playC>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearC: if (!isEqual(get<Id::clearC>(), value)) {get<Id::clearC>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::groovePlay: if (!isEqual(get<Id::groovePlay>(), value)) {get<Id::groovePlay>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::grooveVariation: if (!isEqual(get<Id::grooveVariation>(), value)) {get<Id::grooveVariation>() = value;m_modified = true;}
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
        tapeSpeed      , // dial
        bars           , // dial
        inputGain      , // dial
        grooveLevel    , // dial
        recordA        , // switch
        playA          , // switch
        clearA         , // switch
        recordB        , // switch
        playB          , // switch
        clearB         , // switch
        recordC        , // switch
        playC          , // switch
        clearC         , // switch
        groovePlay     , // switch
        bpm            , // dial
        grooveVariation // dial
)
