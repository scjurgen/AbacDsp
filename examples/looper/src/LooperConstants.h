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
constexpr uint32_t background{0xff101010};
constexpr uint32_t backgroundDark{0xff505050};
constexpr uint32_t backgroundMid{0xff404040};
constexpr uint32_t backgroundLight{0xff202090};

constexpr uint32_t gradientStart{0xffeeeee};
constexpr uint32_t gradientEnd{0xffc4c4c4};
constexpr uint32_t gradientDark{0xff101010};

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

namespace Dial
{
constexpr float minSize = 80.0f;
constexpr float maxSize = 180.0f;
}

namespace InitJuce
{
constexpr auto WindowWidth{1400};
constexpr auto WindowHeight{800};
constexpr auto TimerHertz = 60;
}

}
