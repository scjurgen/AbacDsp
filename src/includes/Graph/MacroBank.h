#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace AbacDsp::Graph
{

/**
 * @ingroup graph
 * @brief A fixed set of macro values: any thread writes, the audio thread reads.
 *
 * Each slot is one normalized value, usually 0 to 1. Reads and writes are relaxed atomics, so a
 * control callback that may run on either thread never needs a lock and never touches a node.
 * A slot outside the bank reads as 0 and ignores writes. The bank must outlive every graph
 * whose MacroInput nodes point at it.
 */
class MacroBank
{
  public:
    static constexpr size_t kSlotCount{12};

    void set(const size_t slot, const float value) noexcept
    {
        if (slot < kSlotCount)
        {
            m_values[slot].store(value, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] float get(const size_t slot) const noexcept
    {
        return slot < kSlotCount ? m_values[slot].load(std::memory_order_relaxed) : 0.f;
    }

  private:
    std::array<std::atomic<float>, kSlotCount> m_values{};
};

}
