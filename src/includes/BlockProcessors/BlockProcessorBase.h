#pragma once

#include <array>
#include <memory>

namespace AbacDsp
{

/**
 * @ingroup blockprocessors
 * @brief Interface for a processor that transforms a block of samples in place.
 *
 * A block processor is a thin dispatch layer over a per-sample loop: the
 * arithmetic is unchanged, only the granularity at which it is invoked. The
 * block arrives as std::array<float, BlockSize>, so the loop bound is a
 * compile-time constant and the body stays vectorisable.
 *
 * process() is noexcept and must not allocate. reset() clears accumulated
 * state and defaults to a no-op, which is correct for stateless processors.
 */
template <size_t BlockSize>
class BlockProcessorBase
{
  public:
    virtual ~BlockProcessorBase() = default;
    virtual void process(std::array<float, BlockSize>& blk) noexcept = 0;
    virtual void reset() noexcept {}
};

/**
 * @ingroup blockprocessors
 * @brief Static counterpart to BlockProcessorBase: process(float*) and reset(), no base class.
 *
 * Same shape resolved at compile time, so there is no vtable and the process()
 * body can inline. Block length is not part of the concept and must be agreed
 * out of band, which is the price paid for dropping the std::array signature.
 */
template <typename T>
concept DelayCallback = requires(T t, float* data) {
    t.process(data);
    t.reset();
};

/**
 * @ingroup blockprocessors
 * @brief Fixed array of ORDER optional BlockProcessorBase slots, applied in index order.
 *
 * Slots are shared_ptr and may be empty; an empty slot costs one null check.
 * Shared ownership means one processor instance can occupy several slots, in
 * which case they share its state rather than each holding their own.
 *
 * Slot assignment is unsynchronised. Calling setCallback() or removeCallback()
 * concurrently with processCallbacks() is a data race, and the shared_ptr
 * refcount traffic makes it one that will not reliably show up in testing.
 */
/**
 * @ingroup blockprocessors
 * @brief Fixed array of ORDER optional BlockProcessorBase slots, applied in index order.
 *
 * Slots are shared_ptr and may be empty; an empty slot costs one null check.
 * Shared ownership means one processor instance can occupy several slots, in
 * which case those slots share its state rather than each holding their own.
 *
 * Slot assignment is unsynchronised. shared_ptr keeps its refcount atomic but
 * assignment of the handle itself is not, so calling setCallback() or
 * removeCallback() concurrently with processCallbacks() is a data race.
 */
template <size_t ORDER, size_t BlockSize>
class CallbackManager
{
  public:
    using ProcessorPtr = std::shared_ptr<BlockProcessorBase<BlockSize>>;

    void setCallback(const size_t delayIndex, ProcessorPtr processor) noexcept
    {
        if (delayIndex < ORDER)
        {
            m_processors[delayIndex] = std::move(processor);
        }
    }

    void removeCallback(const size_t delayIndex) noexcept
    {
        if (delayIndex < ORDER)
        {
            m_processors[delayIndex].reset();
        }
    }

    void processCallbacks(std::array<std::array<float, BlockSize>, ORDER>& delayData) noexcept
    {
        for (size_t delayIdx = 0; delayIdx < ORDER; ++delayIdx)
        {
            if (m_processors[delayIdx])
            {
                m_processors[delayIdx]->process(delayData[delayIdx]);
            }
        }
    }

    void reset() noexcept
    {
        for (size_t delayIdx = 0; delayIdx < ORDER; ++delayIdx)
        {
            if (m_processors[delayIdx])
            {
                m_processors[delayIdx]->reset();
            }
        }
    }

    [[nodiscard]] bool hasCallback(const size_t delayIndex) const noexcept
    {
        if (delayIndex >= ORDER)
        {
            return false;
        }
        return m_processors[delayIndex] != nullptr;
    }

  private:
    std::array<ProcessorPtr, ORDER> m_processors{};
};

}
