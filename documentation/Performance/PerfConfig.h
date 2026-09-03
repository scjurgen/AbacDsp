#pragma once

#include <cstddef>

namespace AbacDsp::Perf
{

/// @brief One audio-callback block: shared across every benchmarked category so results compare directly.
inline constexpr float kSampleRate = 48000.f;
inline constexpr size_t kBlockSize = 128;

/// @brief Safety backstop for the max-instances search, not a claim about real hardware limits.
inline constexpr size_t kDefaultInstanceCap = 100'000;

}
