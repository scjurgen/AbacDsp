#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <span>
#include <vector>

namespace AbacDsp
{

/**
 * @ingroup helpers
 * @brief Lock-free single-producer/single-consumer ring buffer.
 *
 * Exactly one thread may push and exactly one other thread may pop. Head and tail are
 * free-running counters masked by a power-of-two capacity, so full and empty are
 * distinguishable without a spare slot. Allocates only in the constructor; push() and pop()
 * are wait-free and never block, so the producer can be the audio thread.
 */
template <typename T>
class SpscRingBuffer
{
  public:
    explicit SpscRingBuffer(const size_t minimumCapacity)
        : m_buffer(std::bit_ceil(std::max<size_t>(minimumCapacity, 2)))
        , m_mask(m_buffer.size() - 1)
    {
    }

    [[nodiscard]] size_t capacity() const noexcept
    {
        return m_buffer.size();
    }

    // Returns how many items were written; the rest are dropped when the buffer is full.
    [[nodiscard]] size_t push(const std::span<const T> items) noexcept
    {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t tail = m_tail.load(std::memory_order_acquire);
        const size_t count = std::min(items.size(), capacity() - (head - tail));
        for (size_t i = 0; i < count; ++i)
        {
            m_buffer[(head + i) & m_mask] = items[i];
        }
        m_head.store(head + count, std::memory_order_release);
        return count;
    }

    [[nodiscard]] size_t pop(const std::span<T> items) noexcept
    {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t head = m_head.load(std::memory_order_acquire);
        const size_t count = std::min(items.size(), head - tail);
        for (size_t i = 0; i < count; ++i)
        {
            items[i] = m_buffer[(tail + i) & m_mask];
        }
        m_tail.store(tail + count, std::memory_order_release);
        return count;
    }

    // A snapshot: exact from either side's own thread, may lag the other side's progress.
    [[nodiscard]] size_t size() const noexcept
    {
        return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_acquire);
    }

  private:
    static constexpr size_t kCacheLine{64};

    std::vector<T> m_buffer;
    size_t m_mask;
    alignas(kCacheLine) std::atomic<size_t> m_head{0};
    alignas(kCacheLine) std::atomic<size_t> m_tail{0};
};

}
