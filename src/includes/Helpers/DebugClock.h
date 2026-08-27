#pragma once

#include <chrono>
#include <cstdint>

namespace AbacDsp
{
/**
 * @ingroup helpers
 * @brief Shared, process-wide reference point for ad hoc debug timestamps.
 *
 * Not realtime-safe (not intended to be called from a hot path) and not "true"
 * process start - the epoch is the moment this function is first called from
 * anywhere in the process - but that is close enough, and shared across every
 * caller, for correlating two independent debug logs against one timeline.
 */
[[nodiscard]] inline int64_t debugElapsedMicroseconds() noexcept
{
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}
}
