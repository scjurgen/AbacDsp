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
        level       , // dial
        type        , // drop
        preboostLow , // dial
        preboostHigh, // dial
        crossOver   , // dial
        cut          // dial
    };
float level{0.0f};
size_t type{0};
float preboostLow{0.0f};
float preboostHigh{0.0f};
float crossOver{800.0f};
float cut{8000.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "level",
"type",
"preboostLow",
"preboostHigh",
"crossOver",
"cut"
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
        if constexpr (ParamId == Id::level) return level;
        else if constexpr (ParamId == Id::type) return type;
        else if constexpr (ParamId == Id::preboostLow) return preboostLow;
        else if constexpr (ParamId == Id::preboostHigh) return preboostHigh;
        else if constexpr (ParamId == Id::crossOver) return crossOver;
        else if constexpr (ParamId == Id::cut) return cut;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::level: if (!isEqual(get<Id::level>(), value)) {get<Id::level>() = value;m_modified = true;}
break;
 case Id::type: if (!isEqual(get<Id::type>(), value)) {get<Id::type>() = static_cast<size_t>(value) ;m_modified = true;}
break;
 case Id::preboostLow: if (!isEqual(get<Id::preboostLow>(), value)) {get<Id::preboostLow>() = value;m_modified = true;}
break;
 case Id::preboostHigh: if (!isEqual(get<Id::preboostHigh>(), value)) {get<Id::preboostHigh>() = value;m_modified = true;}
break;
 case Id::crossOver: if (!isEqual(get<Id::crossOver>(), value)) {get<Id::crossOver>() = value;m_modified = true;}
break;
 case Id::cut: if (!isEqual(get<Id::cut>(), value)) {get<Id::cut>() = value;m_modified = true;}
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
        level       , // dial
        type        , // drop
        preboostLow , // dial
        preboostHigh, // dial
        crossOver   , // dial
        cut          // dial
)
