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
        record           , // switch
        play             , // switch
        overdub          , // switch
        clear            , // switch
        threshRec        , // switch
        hostSync         , // switch
        freeRecord       , // switch
        countInBars      , // drop
        timeSignature    , // drop
        autoStop         , // switch
        recordBars       , // dial
        sliceDivision    , // drop
        bpm              , // dial
        clickVolume      , // dial
        clickRecordVolume, // dial
        loopVolume       , // dial
        recThreshold     , // dial
        freeze           , // switch
        seqPlay          , // switch
        clearSeq          // switch
    };
bool record{false};
bool play{false};
bool overdub{false};
bool clear{false};
bool threshRec{false};
bool hostSync{false};
bool freeRecord{false};
size_t countInBars{0};
size_t timeSignature{2};
bool autoStop{false};
float recordBars{4.0f};
size_t sliceDivision{1};
float bpm{120.0f};
float clickVolume{-12.0f};
float clickRecordVolume{-60.0f};
float loopVolume{0.0f};
float recThreshold{-36.0f};
bool freeze{false};
bool seqPlay{false};
bool clearSeq{false};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "record",
"play",
"overdub",
"clear",
"threshRec",
"hostSync",
"freeRecord",
"countInBars",
"timeSignature",
"autoStop",
"recordBars",
"sliceDivision",
"bpm",
"clickVolume",
"clickRecordVolume",
"loopVolume",
"recThreshold",
"freeze",
"seqPlay",
"clearSeq"
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
        else if constexpr (ParamId == Id::freeRecord) return freeRecord;
        else if constexpr (ParamId == Id::countInBars) return countInBars;
        else if constexpr (ParamId == Id::timeSignature) return timeSignature;
        else if constexpr (ParamId == Id::autoStop) return autoStop;
        else if constexpr (ParamId == Id::recordBars) return recordBars;
        else if constexpr (ParamId == Id::sliceDivision) return sliceDivision;
        else if constexpr (ParamId == Id::bpm) return bpm;
        else if constexpr (ParamId == Id::clickVolume) return clickVolume;
        else if constexpr (ParamId == Id::clickRecordVolume) return clickRecordVolume;
        else if constexpr (ParamId == Id::loopVolume) return loopVolume;
        else if constexpr (ParamId == Id::recThreshold) return recThreshold;
        else if constexpr (ParamId == Id::freeze) return freeze;
        else if constexpr (ParamId == Id::seqPlay) return seqPlay;
        else if constexpr (ParamId == Id::clearSeq) return clearSeq;

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
 case Id::freeRecord: if (!isEqual(get<Id::freeRecord>(), value)) {get<Id::freeRecord>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::countInBars: if (!isEqual(get<Id::countInBars>(), value)) {get<Id::countInBars>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::timeSignature: if (!isEqual(get<Id::timeSignature>(), value)) {get<Id::timeSignature>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::autoStop: if (!isEqual(get<Id::autoStop>(), value)) {get<Id::autoStop>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::recordBars: if (!isEqual(get<Id::recordBars>(), value)) {get<Id::recordBars>() = value;m_modified = true;}
break;
 case Id::sliceDivision: if (!isEqual(get<Id::sliceDivision>(), value)) {get<Id::sliceDivision>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::bpm: if (!isEqual(get<Id::bpm>(), value)) {get<Id::bpm>() = value;m_modified = true;}
break;
 case Id::clickVolume: if (!isEqual(get<Id::clickVolume>(), value)) {get<Id::clickVolume>() = value;m_modified = true;}
break;
 case Id::clickRecordVolume: if (!isEqual(get<Id::clickRecordVolume>(), value)) {get<Id::clickRecordVolume>() = value;m_modified = true;}
break;
 case Id::loopVolume: if (!isEqual(get<Id::loopVolume>(), value)) {get<Id::loopVolume>() = value;m_modified = true;}
break;
 case Id::recThreshold: if (!isEqual(get<Id::recThreshold>(), value)) {get<Id::recThreshold>() = value;m_modified = true;}
break;
 case Id::freeze: if (!isEqual(get<Id::freeze>(), value)) {get<Id::freeze>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::seqPlay: if (!isEqual(get<Id::seqPlay>(), value)) {get<Id::seqPlay>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::clearSeq: if (!isEqual(get<Id::clearSeq>(), value)) {get<Id::clearSeq>() = static_cast<bool>(value) ;m_modified = true;}
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
        record           , // switch
        play             , // switch
        overdub          , // switch
        clear            , // switch
        threshRec        , // switch
        hostSync         , // switch
        freeRecord       , // switch
        countInBars      , // drop
        timeSignature    , // drop
        autoStop         , // switch
        recordBars       , // dial
        sliceDivision    , // drop
        bpm              , // dial
        clickVolume      , // dial
        clickRecordVolume, // dial
        loopVolume       , // dial
        recThreshold     , // dial
        freeze           , // switch
        seqPlay          , // switch
        clearSeq          // switch
)
