#pragma once

#include <cstdint>

namespace Constants
{
namespace Colors
{
constexpr std::array<uint32_t, 10> cols{0xFFFFFFFF, 0xFFFFEEDD, 0xFFFFDDBB, 0xFFFFCC99, 0xFFEE9966,
                                        0xFFDD6633, 0xFFCC3300, 0xFFA31E00, 0xFF7A0A00, 0xFF4C0000};
constexpr uint32_t INV{0x00ffffff};
constexpr uint32_t bg_App{cols[1]};
constexpr uint32_t bg_DarkGrey{cols[2]};
constexpr uint32_t bg_MidGrey{cols[5]};
constexpr uint32_t bg_LightGrey{cols[3]};

constexpr uint32_t gd_LightGreyStart{cols[1]};
constexpr uint32_t gd_LightGreyEnd{cols[2]};
constexpr uint32_t gd_DarkGreyStart{cols[3]};

constexpr uint32_t statusOutline{cols[8]};
}

namespace Text
{
constexpr float labelHeight = 30.f;
constexpr float labelWidth = 90.f;
constexpr float fontHeight = 16.f;
}

namespace Margins
{
constexpr float small = 2.0f;
constexpr float medium = 4.0f;
constexpr float big = 8.0f;
}

namespace InitJuce
{
constexpr auto WindowWidth{1200};
constexpr auto WindowHeight{800};
constexpr auto TimerHertz = 60;
}

}
