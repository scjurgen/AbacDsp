#pragma once

namespace AbacDsp
{

// tag type: request an immediate parameter change instead of the smoothed transition
struct SkipSmoothing_t
{
};

inline constexpr auto skipSmoothing = SkipSmoothing_t{};

}
