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
        order          , // drop
        dry            , // dial
        wet            , // dial
        lowSize        , // dial
        highSize       , // dial
        uniqueDelay    , // switch
        bulge          , // dial
        decayLow       , // dial
        crossOver      , // dial
        decayHigh      , // dial
        allPassUp      , // dial
        allPassDown    , // dial
        allPassCount   , // drop
        lowPass        , // dial
        lowPassCount   , // drop
        highPass       , // dial
        highPassCount  , // drop
        modulationDepth, // dial
        modulationSpeed, // dial
        modulationCount, // drop
        reversePitch   , // switch
        pitchStrength  , // dial
        pitch1Inplace  , // dial
        pitch2Inplace  , // dial
        pitchSize      , // dial
        pitch1         , // dial
        pitch2         , // dial
        pitch3         , // dial
        pitch4          // dial
    };
size_t order{8};
float dry{0.0f};
float wet{0.0f};
float lowSize{10.0f};
float highSize{25.0f};
bool uniqueDelay{true};
float bulge{0.0f};
float decayLow{2000.0f};
float crossOver{800.0f};
float decayHigh{2000.0f};
float allPassUp{2000.0f};
float allPassDown{1000.0f};
size_t allPassCount{0};
float lowPass{3000.0f};
size_t lowPassCount{0};
float highPass{3000.0f};
size_t highPassCount{0};
float modulationDepth{0.02f};
float modulationSpeed{0.25f};
size_t modulationCount{0};
bool reversePitch{false};
float pitchStrength{0.5f};
float pitch1Inplace{0.0f};
float pitch2Inplace{0.0f};
float pitchSize{500.0f};
float pitch1{0.0f};
float pitch2{0.0f};
float pitch3{0.0f};
float pitch4{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "order",
"dry",
"wet",
"lowSize",
"highSize",
"uniqueDelay",
"bulge",
"decayLow",
"crossOver",
"decayHigh",
"allPassUp",
"allPassDown",
"allPassCount",
"lowPass",
"lowPassCount",
"highPass",
"highPassCount",
"modulationDepth",
"modulationSpeed",
"modulationCount",
"reversePitch",
"pitchStrength",
"pitch1Inplace",
"pitch2Inplace",
"pitchSize",
"pitch1",
"pitch2",
"pitch3",
"pitch4"
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
        if constexpr (ParamId == Id::order) return order;
        else if constexpr (ParamId == Id::dry) return dry;
        else if constexpr (ParamId == Id::wet) return wet;
        else if constexpr (ParamId == Id::lowSize) return lowSize;
        else if constexpr (ParamId == Id::highSize) return highSize;
        else if constexpr (ParamId == Id::uniqueDelay) return uniqueDelay;
        else if constexpr (ParamId == Id::bulge) return bulge;
        else if constexpr (ParamId == Id::decayLow) return decayLow;
        else if constexpr (ParamId == Id::crossOver) return crossOver;
        else if constexpr (ParamId == Id::decayHigh) return decayHigh;
        else if constexpr (ParamId == Id::allPassUp) return allPassUp;
        else if constexpr (ParamId == Id::allPassDown) return allPassDown;
        else if constexpr (ParamId == Id::allPassCount) return allPassCount;
        else if constexpr (ParamId == Id::lowPass) return lowPass;
        else if constexpr (ParamId == Id::lowPassCount) return lowPassCount;
        else if constexpr (ParamId == Id::highPass) return highPass;
        else if constexpr (ParamId == Id::highPassCount) return highPassCount;
        else if constexpr (ParamId == Id::modulationDepth) return modulationDepth;
        else if constexpr (ParamId == Id::modulationSpeed) return modulationSpeed;
        else if constexpr (ParamId == Id::modulationCount) return modulationCount;
        else if constexpr (ParamId == Id::reversePitch) return reversePitch;
        else if constexpr (ParamId == Id::pitchStrength) return pitchStrength;
        else if constexpr (ParamId == Id::pitch1Inplace) return pitch1Inplace;
        else if constexpr (ParamId == Id::pitch2Inplace) return pitch2Inplace;
        else if constexpr (ParamId == Id::pitchSize) return pitchSize;
        else if constexpr (ParamId == Id::pitch1) return pitch1;
        else if constexpr (ParamId == Id::pitch2) return pitch2;
        else if constexpr (ParamId == Id::pitch3) return pitch3;
        else if constexpr (ParamId == Id::pitch4) return pitch4;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::order: if (!isEqual(get<Id::order>(), value)) {get<Id::order>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::dry: if (!isEqual(get<Id::dry>(), value)) {get<Id::dry>() = value;m_modified = true;}
break;
 case Id::wet: if (!isEqual(get<Id::wet>(), value)) {get<Id::wet>() = value;m_modified = true;}
break;
 case Id::lowSize: if (!isEqual(get<Id::lowSize>(), value)) {get<Id::lowSize>() = value;m_modified = true;}
break;
 case Id::highSize: if (!isEqual(get<Id::highSize>(), value)) {get<Id::highSize>() = value;m_modified = true;}
break;
 case Id::uniqueDelay: if (!isEqual(get<Id::uniqueDelay>(), value)) {get<Id::uniqueDelay>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::bulge: if (!isEqual(get<Id::bulge>(), value)) {get<Id::bulge>() = value;m_modified = true;}
break;
 case Id::decayLow: if (!isEqual(get<Id::decayLow>(), value)) {get<Id::decayLow>() = value;m_modified = true;}
break;
 case Id::crossOver: if (!isEqual(get<Id::crossOver>(), value)) {get<Id::crossOver>() = value;m_modified = true;}
break;
 case Id::decayHigh: if (!isEqual(get<Id::decayHigh>(), value)) {get<Id::decayHigh>() = value;m_modified = true;}
break;
 case Id::allPassUp: if (!isEqual(get<Id::allPassUp>(), value)) {get<Id::allPassUp>() = value;m_modified = true;}
break;
 case Id::allPassDown: if (!isEqual(get<Id::allPassDown>(), value)) {get<Id::allPassDown>() = value;m_modified = true;}
break;
 case Id::allPassCount: if (!isEqual(get<Id::allPassCount>(), value)) {get<Id::allPassCount>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::lowPass: if (!isEqual(get<Id::lowPass>(), value)) {get<Id::lowPass>() = value;m_modified = true;}
break;
 case Id::lowPassCount: if (!isEqual(get<Id::lowPassCount>(), value)) {get<Id::lowPassCount>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::highPass: if (!isEqual(get<Id::highPass>(), value)) {get<Id::highPass>() = value;m_modified = true;}
break;
 case Id::highPassCount: if (!isEqual(get<Id::highPassCount>(), value)) {get<Id::highPassCount>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::modulationDepth: if (!isEqual(get<Id::modulationDepth>(), value)) {get<Id::modulationDepth>() = value;m_modified = true;}
break;
 case Id::modulationSpeed: if (!isEqual(get<Id::modulationSpeed>(), value)) {get<Id::modulationSpeed>() = value;m_modified = true;}
break;
 case Id::modulationCount: if (!isEqual(get<Id::modulationCount>(), value)) {get<Id::modulationCount>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::reversePitch: if (!isEqual(get<Id::reversePitch>(), value)) {get<Id::reversePitch>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::pitchStrength: if (!isEqual(get<Id::pitchStrength>(), value)) {get<Id::pitchStrength>() = value;m_modified = true;}
break;
 case Id::pitch1Inplace: if (!isEqual(get<Id::pitch1Inplace>(), value)) {get<Id::pitch1Inplace>() = value;m_modified = true;}
break;
 case Id::pitch2Inplace: if (!isEqual(get<Id::pitch2Inplace>(), value)) {get<Id::pitch2Inplace>() = value;m_modified = true;}
break;
 case Id::pitchSize: if (!isEqual(get<Id::pitchSize>(), value)) {get<Id::pitchSize>() = value;m_modified = true;}
break;
 case Id::pitch1: if (!isEqual(get<Id::pitch1>(), value)) {get<Id::pitch1>() = value;m_modified = true;}
break;
 case Id::pitch2: if (!isEqual(get<Id::pitch2>(), value)) {get<Id::pitch2>() = value;m_modified = true;}
break;
 case Id::pitch3: if (!isEqual(get<Id::pitch3>(), value)) {get<Id::pitch3>() = value;m_modified = true;}
break;
 case Id::pitch4: if (!isEqual(get<Id::pitch4>(), value)) {get<Id::pitch4>() = value;m_modified = true;}
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
        order          , // drop
        dry            , // dial
        wet            , // dial
        lowSize        , // dial
        highSize       , // dial
        uniqueDelay    , // switch
        bulge          , // dial
        decayLow       , // dial
        crossOver      , // dial
        decayHigh      , // dial
        allPassUp      , // dial
        allPassDown    , // dial
        allPassCount   , // drop
        lowPass        , // dial
        lowPassCount   , // drop
        highPass       , // dial
        highPassCount  , // drop
        modulationDepth, // dial
        modulationSpeed, // dial
        modulationCount, // drop
        reversePitch   , // switch
        pitchStrength  , // dial
        pitch1Inplace  , // dial
        pitch2Inplace  , // dial
        pitchSize      , // dial
        pitch1         , // dial
        pitch2         , // dial
        pitch3         , // dial
        pitch4          // dial
)
