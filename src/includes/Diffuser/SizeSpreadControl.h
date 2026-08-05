#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

/**
 * @file
 * @ingroup diffuser
 * @brief Per-element size offsets that decorrelate two channels of a diffuser.
 *
 * Building the second channel from the same base sizes with the sign pattern
 * inverted gives every element a different length on each side, so the two
 * channels do not share a comb structure and the result has width without any
 * added delay or phase trickery.
 */

namespace AbacDsp::SizeSpreadControl
{
inline constexpr float kDefaultSafetyFraction{0.5f};

/// @ingroup diffuser
/// @brief Caps requestedSpread at safetyFraction * baseSize.
/// Without the cap a large spread could drive an element to zero, negative, or onto a shared floor value.
[[nodiscard]] constexpr float effectiveMagnitude(const float baseSize, const float requestedSpread,
                                                 const float safetyFraction = kDefaultSafetyFraction) noexcept
{
    return std::min(requestedSpread, baseSize * safetyFraction);
}

/// @ingroup diffuser
/// @brief Fills offsets[0..count) with +/-effectiveMagnitude, alternating sign by index.
/// invertParity flips the whole pattern, which is how the opposite channel is derived from the same base sizes.
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
