#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace AbacDsp::SizeSpreadControl
{
inline constexpr float kDefaultSafetyFraction{0.5f};

// Caps requestedSpread to at most safetyFraction * baseSize, so an element can never be pushed
// to zero, negative, or a floor value shared with other elements regardless of how large
// requestedSpread is asked to be.
[[nodiscard]] constexpr float effectiveMagnitude(const float baseSize, const float requestedSpread,
                                                 const float safetyFraction = kDefaultSafetyFraction) noexcept
{
    return std::min(requestedSpread, baseSize * safetyFraction);
}

// Fills offsets[0..count) with a per-element +/-effectiveMagnitude, alternating sign by index
// (flipped by invertParity), so a caller building two channels from the same baseSizes with
// opposite invertParity gets a decorrelated size per element on each channel.
template <std::size_t N>
void fillAlternatingOffsets(const std::array<float, N>& baseSizes, const float requestedSpread, const bool invertParity,
                            std::array<float, N>& offsets, const std::size_t count,
                            const float safetyFraction = kDefaultSafetyFraction) noexcept
{
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto magnitude = effectiveMagnitude(baseSizes[i], requestedSpread, safetyFraction);
        const bool positive = (i % 2 == 0) != invertParity;
        offsets[i] = positive ? magnitude : -magnitude;
    }
}
}
