#pragma once

#include <array>
#include <atomic>

// Wait-free single-producer, single-consumer hand-off: the producer always writes a slot the
// consumer cannot be reading, and the consumer sees only whole published values.
template <typename T>
class TripleBuffer
{
  public:
    [[nodiscard]] T& backSlot() noexcept
    {
        return m_slots[m_back];
    }

    void publish() noexcept
    {
        m_back = m_shared.exchange(m_back | kFreshBit, std::memory_order_acq_rel) & kIndexMask;
    }

    // The newest published value, or nullptr when nothing new arrived since the last call.
    [[nodiscard]] const T* acquireNewest() noexcept
    {
        if ((m_shared.load(std::memory_order_relaxed) & kFreshBit) == 0)
        {
            return nullptr;
        }
        m_front = m_shared.exchange(m_front, std::memory_order_acq_rel) & kIndexMask;
        return &m_slots[m_front];
    }

  private:
    static constexpr unsigned kFreshBit{4};
    static constexpr unsigned kIndexMask{3};

    std::array<T, 3> m_slots{};
    std::atomic<unsigned> m_shared{2};
    unsigned m_back{0};
    unsigned m_front{1};
};
