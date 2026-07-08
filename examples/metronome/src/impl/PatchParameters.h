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
        bpm        , // dial
        dropBars   , // drop
        metroVolume, // dial
        inputVolume, // dial
        subVolume  , // dial
        onOff      , // switch
        preset     , // drop
        swingRatio  // dial
    };
float bpm{120.0f};
size_t dropBars{0};
float metroVolume{-6.0f};
float inputVolume{0.0f};
float subVolume{-15.0f};
bool onOff{false};
size_t preset{6};
float swingRatio{1.5f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "bpm",
"dropBars",
"metroVolume",
"inputVolume",
"subVolume",
"onOff",
"preset",
"swingRatio"
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
        else if constexpr (ParamId == Id::dropBars) return dropBars;
        else if constexpr (ParamId == Id::metroVolume) return metroVolume;
        else if constexpr (ParamId == Id::inputVolume) return inputVolume;
        else if constexpr (ParamId == Id::subVolume) return subVolume;
        else if constexpr (ParamId == Id::onOff) return onOff;
        else if constexpr (ParamId == Id::preset) return preset;
        else if constexpr (ParamId == Id::swingRatio) return swingRatio;

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
 case Id::dropBars: if (!isEqual(get<Id::dropBars>(), value)) {get<Id::dropBars>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::metroVolume: if (!isEqual(get<Id::metroVolume>(), value)) {get<Id::metroVolume>() = value;m_modified = true;}
break;
 case Id::inputVolume: if (!isEqual(get<Id::inputVolume>(), value)) {get<Id::inputVolume>() = value;m_modified = true;}
break;
 case Id::subVolume: if (!isEqual(get<Id::subVolume>(), value)) {get<Id::subVolume>() = value;m_modified = true;}
break;
 case Id::onOff: if (!isEqual(get<Id::onOff>(), value)) {get<Id::onOff>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::preset: if (!isEqual(get<Id::preset>(), value)) {get<Id::preset>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::swingRatio: if (!isEqual(get<Id::swingRatio>(), value)) {get<Id::swingRatio>() = value;m_modified = true;}
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
        bpm        , // dial
        dropBars   , // drop
        metroVolume, // dial
        inputVolume, // dial
        subVolume  , // dial
        onOff      , // switch
        preset     , // drop
        swingRatio  // dial
)
