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
        gain        , // dial
        lowShelving , // dial
        highShelving, // dial
        latency      // dial
    };
float gain{0.0f};
float lowShelving{0.0f};
float highShelving{0.0f};
float latency{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "gain",
"lowShelving",
"highShelving",
"latency"
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
        if constexpr (ParamId == Id::gain) return gain;
        else if constexpr (ParamId == Id::lowShelving) return lowShelving;
        else if constexpr (ParamId == Id::highShelving) return highShelving;
        else if constexpr (ParamId == Id::latency) return latency;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::gain: if (!isEqual(get<Id::gain>(), value)) {get<Id::gain>() = value;m_modified = true;}
break;
 case Id::lowShelving: if (!isEqual(get<Id::lowShelving>(), value)) {get<Id::lowShelving>() = value;m_modified = true;}
break;
 case Id::highShelving: if (!isEqual(get<Id::highShelving>(), value)) {get<Id::highShelving>() = value;m_modified = true;}
break;
 case Id::latency: if (!isEqual(get<Id::latency>(), value)) {get<Id::latency>() = value;m_modified = true;}
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
        gain        , // dial
        lowShelving , // dial
        highShelving, // dial
        latency      // dial
)
