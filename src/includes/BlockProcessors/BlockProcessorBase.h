#pragma once

#include <array>
#include <memory>

namespace AbacDsp
{

template <size_t BlockSize>
class BlockProcessorBase
{
  public:
    virtual ~BlockProcessorBase() = default;
    // must be safe for inplace transformation of sampledata
    virtual void process(std::array<float, BlockSize>& blk) noexcept = 0;
    virtual void reset() noexcept {}
};

template <typename T>
concept DelayCallback = requires(T t, float* data) {
    t.process(data);
    t.reset();
};

template <size_t ORDER, size_t BlockSize>
class CallbackManager
{
  public:
    using ProcessorPtr = std::shared_ptr<BlockProcessorBase<BlockSize>>;

    void setCallback(size_t delayIndex, ProcessorPtr processor)
    {
        if (delayIndex < ORDER)
        {
            m_processors[delayIndex] = std::move(processor);
        }
    }

    void removeCallback(size_t delayIndex)
    {
        if (delayIndex < ORDER)
        {
            m_processors[delayIndex].reset();
        }
    }

    void processCallbacks(std::array<std::array<float, BlockSize>, ORDER>& delayData)
    {
        for (size_t delayIdx = 0; delayIdx < ORDER; ++delayIdx)
        {
            if (m_processors[delayIdx])
            {
                m_processors[delayIdx]->process(delayData[delayIdx]);
            }
        }
    }

    void reset()
    {
        for (size_t delayIdx = 0; delayIdx < ORDER; ++delayIdx)
        {
            if (m_processors[delayIdx])
            {
                m_processors[delayIdx]->reset();
            }
        }
    }

    bool hasCallback(size_t delayIndex) const
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
