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
        vol   , // dial
        user1 , // dial
        user2 , // dial
        user3 , // dial
        user4 , // dial
        user5 , // dial
        user6 , // dial
        user7 , // dial
        user8 , // dial
        user9 , // dial
        user10, // dial
        user11, // dial
        user12 // dial
    };
float vol{0.0f};
float user1{0.0f};
float user2{0.0f};
float user3{0.0f};
float user4{0.0f};
float user5{0.0f};
float user6{0.0f};
float user7{0.0f};
float user8{0.0f};
float user9{0.0f};
float user10{0.0f};
float user11{0.0f};
float user12{0.0f};


    static constexpr auto paramNames = std::to_array<std::string_view>({
        "vol",
"user1",
"user2",
"user3",
"user4",
"user5",
"user6",
"user7",
"user8",
"user9",
"user10",
"user11",
"user12"
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
        if constexpr (ParamId == Id::vol) return vol;
        else if constexpr (ParamId == Id::user1) return user1;
        else if constexpr (ParamId == Id::user2) return user2;
        else if constexpr (ParamId == Id::user3) return user3;
        else if constexpr (ParamId == Id::user4) return user4;
        else if constexpr (ParamId == Id::user5) return user5;
        else if constexpr (ParamId == Id::user6) return user6;
        else if constexpr (ParamId == Id::user7) return user7;
        else if constexpr (ParamId == Id::user8) return user8;
        else if constexpr (ParamId == Id::user9) return user9;
        else if constexpr (ParamId == Id::user10) return user10;
        else if constexpr (ParamId == Id::user11) return user11;
        else if constexpr (ParamId == Id::user12) return user12;

    }

    static constexpr std::string_view getName(const Id id)
    {
        return paramNames[static_cast<size_t>(id)];
    }

    void updateById(const Id id, const float value)
    {
        switch (id)
        {
 case Id::vol: if (!isEqual(get<Id::vol>(), value)) {get<Id::vol>() = value;m_modified = true;}
break;
 case Id::user1: if (!isEqual(get<Id::user1>(), value)) {get<Id::user1>() = value;m_modified = true;}
break;
 case Id::user2: if (!isEqual(get<Id::user2>(), value)) {get<Id::user2>() = value;m_modified = true;}
break;
 case Id::user3: if (!isEqual(get<Id::user3>(), value)) {get<Id::user3>() = value;m_modified = true;}
break;
 case Id::user4: if (!isEqual(get<Id::user4>(), value)) {get<Id::user4>() = value;m_modified = true;}
break;
 case Id::user5: if (!isEqual(get<Id::user5>(), value)) {get<Id::user5>() = value;m_modified = true;}
break;
 case Id::user6: if (!isEqual(get<Id::user6>(), value)) {get<Id::user6>() = value;m_modified = true;}
break;
 case Id::user7: if (!isEqual(get<Id::user7>(), value)) {get<Id::user7>() = value;m_modified = true;}
break;
 case Id::user8: if (!isEqual(get<Id::user8>(), value)) {get<Id::user8>() = value;m_modified = true;}
break;
 case Id::user9: if (!isEqual(get<Id::user9>(), value)) {get<Id::user9>() = value;m_modified = true;}
break;
 case Id::user10: if (!isEqual(get<Id::user10>(), value)) {get<Id::user10>() = value;m_modified = true;}
break;
 case Id::user11: if (!isEqual(get<Id::user11>(), value)) {get<Id::user11>() = value;m_modified = true;}
break;
 case Id::user12: if (!isEqual(get<Id::user12>(), value)) {get<Id::user12>() = value;m_modified = true;}
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
        vol   , // dial
        user1 , // dial
        user2 , // dial
        user3 , // dial
        user4 , // dial
        user5 , // dial
        user6 , // dial
        user7 , // dial
        user8 , // dial
        user9 , // dial
        user10, // dial
        user11, // dial
        user12 // dial
)
