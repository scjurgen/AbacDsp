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
        sync          , // switch
        type          , // drop
        solo          , // drop
        vol           , // dial
        reverbLevelWet, // dial
        reverbDecay   , // dial
        reverbShimmer , // dial
        vol1          , // dial
        vol2          , // dial
        vol3          , // dial
        vol4          , // dial
        vol5          , // dial
        vol6          , // dial
        vol7          , // dial
        vol8          , // dial
        vol9          , // dial
        vol10         , // dial
        revFeed1      , // dial
        revFeed2      , // dial
        revFeed3      , // dial
        revFeed4      , // dial
        revFeed5      , // dial
        revFeed6      , // dial
        revFeed7      , // dial
        revFeed8      , // dial
        revFeed9      , // dial
        revFeed10     , // dial
        pitch1        , // dial
        pitch2        , // dial
        pitch3        , // dial
        pitch4        , // dial
        pitch5        , // dial
        pitch6        , // dial
        pitch7        , // dial
        pitch8        , // dial
        pitch9        , // dial
        pitch10        // dial
    };
bool sync{};
size_t type{};
size_t solo{};
float vol{};
float reverbLevelWet{};
float reverbDecay{};
float reverbShimmer{};
float vol1{};
float vol2{};
float vol3{};
float vol4{};
float vol5{};
float vol6{};
float vol7{};
float vol8{};
float vol9{};
float vol10{};
float revFeed1{};
float revFeed2{};
float revFeed3{};
float revFeed4{};
float revFeed5{};
float revFeed6{};
float revFeed7{};
float revFeed8{};
float revFeed9{};
float revFeed10{};
float pitch1{};
float pitch2{};
float pitch3{};
float pitch4{};
float pitch5{};
float pitch6{};
float pitch7{};
float pitch8{};
float pitch9{};
float pitch10{};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "sync",
"type",
"solo",
"vol",
"reverbLevelWet",
"reverbDecay",
"reverbShimmer",
"vol1",
"vol2",
"vol3",
"vol4",
"vol5",
"vol6",
"vol7",
"vol8",
"vol9",
"vol10",
"revFeed1",
"revFeed2",
"revFeed3",
"revFeed4",
"revFeed5",
"revFeed6",
"revFeed7",
"revFeed8",
"revFeed9",
"revFeed10",
"pitch1",
"pitch2",
"pitch3",
"pitch4",
"pitch5",
"pitch6",
"pitch7",
"pitch8",
"pitch9",
"pitch10"
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
        if constexpr (ParamId == Id::sync) return sync;
        else if constexpr (ParamId == Id::type) return type;
        else if constexpr (ParamId == Id::solo) return solo;
        else if constexpr (ParamId == Id::vol) return vol;
        else if constexpr (ParamId == Id::reverbLevelWet) return reverbLevelWet;
        else if constexpr (ParamId == Id::reverbDecay) return reverbDecay;
        else if constexpr (ParamId == Id::reverbShimmer) return reverbShimmer;
        else if constexpr (ParamId == Id::vol1) return vol1;
        else if constexpr (ParamId == Id::vol2) return vol2;
        else if constexpr (ParamId == Id::vol3) return vol3;
        else if constexpr (ParamId == Id::vol4) return vol4;
        else if constexpr (ParamId == Id::vol5) return vol5;
        else if constexpr (ParamId == Id::vol6) return vol6;
        else if constexpr (ParamId == Id::vol7) return vol7;
        else if constexpr (ParamId == Id::vol8) return vol8;
        else if constexpr (ParamId == Id::vol9) return vol9;
        else if constexpr (ParamId == Id::vol10) return vol10;
        else if constexpr (ParamId == Id::revFeed1) return revFeed1;
        else if constexpr (ParamId == Id::revFeed2) return revFeed2;
        else if constexpr (ParamId == Id::revFeed3) return revFeed3;
        else if constexpr (ParamId == Id::revFeed4) return revFeed4;
        else if constexpr (ParamId == Id::revFeed5) return revFeed5;
        else if constexpr (ParamId == Id::revFeed6) return revFeed6;
        else if constexpr (ParamId == Id::revFeed7) return revFeed7;
        else if constexpr (ParamId == Id::revFeed8) return revFeed8;
        else if constexpr (ParamId == Id::revFeed9) return revFeed9;
        else if constexpr (ParamId == Id::revFeed10) return revFeed10;
        else if constexpr (ParamId == Id::pitch1) return pitch1;
        else if constexpr (ParamId == Id::pitch2) return pitch2;
        else if constexpr (ParamId == Id::pitch3) return pitch3;
        else if constexpr (ParamId == Id::pitch4) return pitch4;
        else if constexpr (ParamId == Id::pitch5) return pitch5;
        else if constexpr (ParamId == Id::pitch6) return pitch6;
        else if constexpr (ParamId == Id::pitch7) return pitch7;
        else if constexpr (ParamId == Id::pitch8) return pitch8;
        else if constexpr (ParamId == Id::pitch9) return pitch9;
        else if constexpr (ParamId == Id::pitch10) return pitch10;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::sync: if (!isEqual(get<Id::sync>(), value)) {get<Id::sync>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::type: if (!isEqual(get<Id::type>(), value)) {get<Id::type>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::solo: if (!isEqual(get<Id::solo>(), value)) {get<Id::solo>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::vol: if (!isEqual(get<Id::vol>(), value)) {get<Id::vol>() = value;m_modified = true;}
break;
 case Id::reverbLevelWet: if (!isEqual(get<Id::reverbLevelWet>(), value)) {get<Id::reverbLevelWet>() = value;m_modified = true;}
break;
 case Id::reverbDecay: if (!isEqual(get<Id::reverbDecay>(), value)) {get<Id::reverbDecay>() = value;m_modified = true;}
break;
 case Id::reverbShimmer: if (!isEqual(get<Id::reverbShimmer>(), value)) {get<Id::reverbShimmer>() = value;m_modified = true;}
break;
 case Id::vol1: if (!isEqual(get<Id::vol1>(), value)) {get<Id::vol1>() = value;m_modified = true;}
break;
 case Id::vol2: if (!isEqual(get<Id::vol2>(), value)) {get<Id::vol2>() = value;m_modified = true;}
break;
 case Id::vol3: if (!isEqual(get<Id::vol3>(), value)) {get<Id::vol3>() = value;m_modified = true;}
break;
 case Id::vol4: if (!isEqual(get<Id::vol4>(), value)) {get<Id::vol4>() = value;m_modified = true;}
break;
 case Id::vol5: if (!isEqual(get<Id::vol5>(), value)) {get<Id::vol5>() = value;m_modified = true;}
break;
 case Id::vol6: if (!isEqual(get<Id::vol6>(), value)) {get<Id::vol6>() = value;m_modified = true;}
break;
 case Id::vol7: if (!isEqual(get<Id::vol7>(), value)) {get<Id::vol7>() = value;m_modified = true;}
break;
 case Id::vol8: if (!isEqual(get<Id::vol8>(), value)) {get<Id::vol8>() = value;m_modified = true;}
break;
 case Id::vol9: if (!isEqual(get<Id::vol9>(), value)) {get<Id::vol9>() = value;m_modified = true;}
break;
 case Id::vol10: if (!isEqual(get<Id::vol10>(), value)) {get<Id::vol10>() = value;m_modified = true;}
break;
 case Id::revFeed1: if (!isEqual(get<Id::revFeed1>(), value)) {get<Id::revFeed1>() = value;m_modified = true;}
break;
 case Id::revFeed2: if (!isEqual(get<Id::revFeed2>(), value)) {get<Id::revFeed2>() = value;m_modified = true;}
break;
 case Id::revFeed3: if (!isEqual(get<Id::revFeed3>(), value)) {get<Id::revFeed3>() = value;m_modified = true;}
break;
 case Id::revFeed4: if (!isEqual(get<Id::revFeed4>(), value)) {get<Id::revFeed4>() = value;m_modified = true;}
break;
 case Id::revFeed5: if (!isEqual(get<Id::revFeed5>(), value)) {get<Id::revFeed5>() = value;m_modified = true;}
break;
 case Id::revFeed6: if (!isEqual(get<Id::revFeed6>(), value)) {get<Id::revFeed6>() = value;m_modified = true;}
break;
 case Id::revFeed7: if (!isEqual(get<Id::revFeed7>(), value)) {get<Id::revFeed7>() = value;m_modified = true;}
break;
 case Id::revFeed8: if (!isEqual(get<Id::revFeed8>(), value)) {get<Id::revFeed8>() = value;m_modified = true;}
break;
 case Id::revFeed9: if (!isEqual(get<Id::revFeed9>(), value)) {get<Id::revFeed9>() = value;m_modified = true;}
break;
 case Id::revFeed10: if (!isEqual(get<Id::revFeed10>(), value)) {get<Id::revFeed10>() = value;m_modified = true;}
break;
 case Id::pitch1: if (!isEqual(get<Id::pitch1>(), value)) {get<Id::pitch1>() = value;m_modified = true;}
break;
 case Id::pitch2: if (!isEqual(get<Id::pitch2>(), value)) {get<Id::pitch2>() = value;m_modified = true;}
break;
 case Id::pitch3: if (!isEqual(get<Id::pitch3>(), value)) {get<Id::pitch3>() = value;m_modified = true;}
break;
 case Id::pitch4: if (!isEqual(get<Id::pitch4>(), value)) {get<Id::pitch4>() = value;m_modified = true;}
break;
 case Id::pitch5: if (!isEqual(get<Id::pitch5>(), value)) {get<Id::pitch5>() = value;m_modified = true;}
break;
 case Id::pitch6: if (!isEqual(get<Id::pitch6>(), value)) {get<Id::pitch6>() = value;m_modified = true;}
break;
 case Id::pitch7: if (!isEqual(get<Id::pitch7>(), value)) {get<Id::pitch7>() = value;m_modified = true;}
break;
 case Id::pitch8: if (!isEqual(get<Id::pitch8>(), value)) {get<Id::pitch8>() = value;m_modified = true;}
break;
 case Id::pitch9: if (!isEqual(get<Id::pitch9>(), value)) {get<Id::pitch9>() = value;m_modified = true;}
break;
 case Id::pitch10: if (!isEqual(get<Id::pitch10>(), value)) {get<Id::pitch10>() = value;m_modified = true;}
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
        return a == 1==static_cast<int>(round(b));
    }
    bool m_modified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PatchParameters,
        sync          , // switch
        type          , // drop
        solo          , // drop
        vol           , // dial
        reverbLevelWet, // dial
        reverbDecay   , // dial
        reverbShimmer , // dial
        vol1          , // dial
        vol2          , // dial
        vol3          , // dial
        vol4          , // dial
        vol5          , // dial
        vol6          , // dial
        vol7          , // dial
        vol8          , // dial
        vol9          , // dial
        vol10         , // dial
        revFeed1      , // dial
        revFeed2      , // dial
        revFeed3      , // dial
        revFeed4      , // dial
        revFeed5      , // dial
        revFeed6      , // dial
        revFeed7      , // dial
        revFeed8      , // dial
        revFeed9      , // dial
        revFeed10     , // dial
        pitch1        , // dial
        pitch2        , // dial
        pitch3        , // dial
        pitch4        , // dial
        pitch5        , // dial
        pitch6        , // dial
        pitch7        , // dial
        pitch8        , // dial
        pitch9        , // dial
        pitch10        // dial
)
