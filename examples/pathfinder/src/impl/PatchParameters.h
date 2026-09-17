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
        depth       , // dial
        speed       , // dial
        aggressivity, // dial
        character    // dial
    };
float depth{35.0f};
float speed{0.8f};
float aggressivity{15.0f};
float character{50.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "depth",
"speed",
"aggressivity",
"character"
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
        if constexpr (ParamId == Id::depth) return depth;
        else if constexpr (ParamId == Id::speed) return speed;
        else if constexpr (ParamId == Id::aggressivity) return aggressivity;
        else if constexpr (ParamId == Id::character) return character;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::depth: if (!isEqual(get<Id::depth>(), value)) {get<Id::depth>() = value;m_modified = true;}
break;
 case Id::speed: if (!isEqual(get<Id::speed>(), value)) {get<Id::speed>() = value;m_modified = true;}
break;
 case Id::aggressivity: if (!isEqual(get<Id::aggressivity>(), value)) {get<Id::aggressivity>() = value;m_modified = true;}
break;
 case Id::character: if (!isEqual(get<Id::character>(), value)) {get<Id::character>() = value;m_modified = true;}
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
        depth       , // dial
        speed       , // dial
        aggressivity, // dial
        character    // dial
)
