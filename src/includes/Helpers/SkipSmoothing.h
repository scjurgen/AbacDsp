#pragma once

namespace AbacDsp
{

/// @ingroup helpers
/// @brief Tag requesting an immediate parameter change instead of a smoothed one.
/// A tag rather than a bool so the intent is readable at the call site and the branch resolves at compile time.
struct SkipSmoothing_t
{
};

inline constexpr auto skipSmoothing = SkipSmoothing_t{};

}
