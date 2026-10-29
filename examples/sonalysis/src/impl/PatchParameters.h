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
        peakFall        , // drop
        rmsWindow       , // drop
        peakHold        , // drop
        holdReset       , // switch
        fftSize         , // drop
        fftFall         , // drop
        spectrumPeakHold, // switch
        referencePitch  , // dial
        pitchSource     , // drop
        beatReset       , // switch
        thru             // switch
    };
size_t peakFall{1};
size_t rmsWindow{1};
size_t peakHold{2};
bool holdReset{false};
size_t fftSize{2};
size_t fftFall{2};
bool spectrumPeakHold{false};
float referencePitch{440.0f};
size_t pitchSource{0};
bool beatReset{false};
bool thru{false};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "peakFall",
"rmsWindow",
"peakHold",
"holdReset",
"fftSize",
"fftFall",
"spectrumPeakHold",
"referencePitch",
"pitchSource",
"beatReset",
"thru"
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
        if constexpr (ParamId == Id::peakFall) return peakFall;
        else if constexpr (ParamId == Id::rmsWindow) return rmsWindow;
        else if constexpr (ParamId == Id::peakHold) return peakHold;
        else if constexpr (ParamId == Id::holdReset) return holdReset;
        else if constexpr (ParamId == Id::fftSize) return fftSize;
        else if constexpr (ParamId == Id::fftFall) return fftFall;
        else if constexpr (ParamId == Id::spectrumPeakHold) return spectrumPeakHold;
        else if constexpr (ParamId == Id::referencePitch) return referencePitch;
        else if constexpr (ParamId == Id::pitchSource) return pitchSource;
        else if constexpr (ParamId == Id::beatReset) return beatReset;
        else if constexpr (ParamId == Id::thru) return thru;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::peakFall: if (!isEqual(get<Id::peakFall>(), value)) {get<Id::peakFall>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::rmsWindow: if (!isEqual(get<Id::rmsWindow>(), value)) {get<Id::rmsWindow>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::peakHold: if (!isEqual(get<Id::peakHold>(), value)) {get<Id::peakHold>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::holdReset: if (!isEqual(get<Id::holdReset>(), value)) {get<Id::holdReset>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::fftSize: if (!isEqual(get<Id::fftSize>(), value)) {get<Id::fftSize>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::fftFall: if (!isEqual(get<Id::fftFall>(), value)) {get<Id::fftFall>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::spectrumPeakHold: if (!isEqual(get<Id::spectrumPeakHold>(), value)) {get<Id::spectrumPeakHold>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::referencePitch: if (!isEqual(get<Id::referencePitch>(), value)) {get<Id::referencePitch>() = value;m_modified = true;}
break;
 case Id::pitchSource: if (!isEqual(get<Id::pitchSource>(), value)) {get<Id::pitchSource>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::beatReset: if (!isEqual(get<Id::beatReset>(), value)) {get<Id::beatReset>() = static_cast<bool>(value) ;m_modified = true;}
break;
 case Id::thru: if (!isEqual(get<Id::thru>(), value)) {get<Id::thru>() = static_cast<bool>(value) ;m_modified = true;}
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
        return a == static_cast<int>(round(b));
    }
    bool m_modified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PatchParameters,
        peakFall        , // drop
        rmsWindow       , // drop
        peakHold        , // drop
        holdReset       , // switch
        fftSize         , // drop
        fftFall         , // drop
        spectrumPeakHold, // switch
        referencePitch  , // dial
        pitchSource     , // drop
        beatReset       , // switch
        thru             // switch
)
