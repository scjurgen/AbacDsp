#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <cstdint>

namespace Constants
{
namespace Colors
{
    constexpr uint32_t bg_App{0xff101010};
    constexpr uint32_t bg_DarkGrey{0xff505050};
    constexpr uint32_t bg_MidGrey{0xff404040};
    constexpr uint32_t bg_LightGrey{0xff202090};

    constexpr uint32_t gd_LightGreyStart{0xffeeeee};
    constexpr uint32_t gd_LightGreyEnd{0xffc4c4c4};
    constexpr uint32_t gd_DarkGreyStart{0xff101010};

    constexpr uint32_t statusOutline{0xffdddddd};
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
    constexpr auto WindowWidth{800};
    constexpr auto WindowHeight{500};
    constexpr auto TimerHertz = 60;
}

}
