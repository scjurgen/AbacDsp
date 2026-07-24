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
        record       , // switch
        play         , // switch
        overdub      , // switch
        clear        , // switch
        threshRec    , // switch
        hostSync     , // switch
        sliceMode    , // drop
        sliceDivision, // drop
        bpm          , // dial
        swing        , // dial
        clickVolume  , // dial
        loopVolume   , // dial
        recThreshold  // dial
    };
bool record{false};
bool play{false};
bool overdub{false};
bool clear{false};
bool threshRec{false};
bool hostSync{false};
size_t sliceMode{0};
size_t sliceDivision{1};
float bpm{120.0f};
float swing{50.0f};
float clickVolume{-12.0f};
float loopVolume{0.0f};
float recThreshold{-36.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "record",
"play",
"overdub",
"clear",
"threshRec",
"hostSync",
"sliceMode",
"sliceDivision",
"bpm",
"swing",
"clickVolume",
"loopVolume",
"recThreshold"
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
        if constexpr (ParamId == Id::record) return record;
        else if constexpr (ParamId == Id::play) return play;
        else if constexpr (ParamId == Id::overdub) return overdub;
        else if constexpr (ParamId == Id::clear) return clear;
        else if constexpr (ParamId == Id::threshRec) return threshRec;
        else if constexpr (ParamId == Id::hostSync) return hostSync;
        else if constexpr (ParamId == Id::sliceMode) return sliceMode;
        else if constexpr (ParamId == Id::sliceDivision) return sliceDivision;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::swing) return swing;
        else if constexpr (ParamId == Id::clickVolume) return clickVolume;
        else if constexpr (ParamId == Id::loopVolume) return loopVolume;
        else if constexpr (ParamId == Id::recThreshold) return recThreshold;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::record: if (!isEqual(get<Id::record>(), value)) {get<Id::record>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::play: if (!isEqual(get<Id::play>(), value)) {get<Id::play>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::overdub: if (!isEqual(get<Id::overdub>(), value)) {get<Id::overdub>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clear: if (!isEqual(get<Id::clear>(), value)) {get<Id::clear>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::threshRec: if (!isEqual(get<Id::threshRec>(), value)) {get<Id::threshRec>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::hostSync: if (!isEqual(get<Id::hostSync>(), value)) {get<Id::hostSync>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::sliceMode: if (!isEqual(get<Id::sliceMode>(), value)) {get<Id::sliceMode>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::sliceDivision: if (!isEqual(get<Id::sliceDivision>(), value)) {get<Id::sliceDivision>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::swing: if (!isEqual(get<Id::swing>(), value)) {get<Id::swing>() = value;m_modified = true;}
break;
 case Id::clickVolume: if (!isEqual(get<Id::clickVolume>(), value)) {get<Id::clickVolume>() = value;m_modified = true;}
break;
 case Id::loopVolume: if (!isEqual(get<Id::loopVolume>(), value)) {get<Id::loopVolume>() = value;m_modified = true;}
break;
 case Id::recThreshold: if (!isEqual(get<Id::recThreshold>(), value)) {get<Id::recThreshold>() = value;m_modified = true;}
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
        record       , // switch
        play         , // switch
        overdub      , // switch
        clear        , // switch
        threshRec    , // switch
        hostSync     , // switch
        sliceMode    , // drop
        sliceDivision, // drop
        bpm          , // dial
        swing        , // dial
        clickVolume  , // dial
        loopVolume   , // dial
        recThreshold  // dial
)
