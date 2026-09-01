#pragma once

#include <cmath>
#include <concepts>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Blends a value toward its nearest integer by level (0: untouched, 1: fully snapped).
 */
template <std::floating_point FP>
[[nodiscard]] FP pitchQuantize(const FP in, const FP level) noexcept
{
    const FP slot = std::round(in);
    const FP dt = in - slot;
    return slot + dt * (FP{1} - level);
}

}
