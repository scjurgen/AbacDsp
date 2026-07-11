#include <gtest/gtest.h>

#include "BlockProcessors/BlockProcessorBase.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kBlockSize{8};
constexpr size_t kOrder{4};

class AddConstantProcessor final : public BlockProcessorBase<kBlockSize>
{
  public:
    explicit AddConstantProcessor(const float valueToAdd)
        : m_valueToAdd(valueToAdd)
    {
    }

    void process(std::array<float, kBlockSize>& blk) noexcept override
    {
        ++m_processCount;
        for (auto& sample : blk)
        {
            sample += m_valueToAdd;
        }
    }

    void reset() noexcept override
    {
        ++m_resetCount;
    }

    [[nodiscard]] size_t processCount() const noexcept
    {
        return m_processCount;
    }

    [[nodiscard]] size_t resetCount() const noexcept
    {
        return m_resetCount;
    }

  private:
    float m_valueToAdd;
    size_t m_processCount{0};
    size_t m_resetCount{0};
};

[[nodiscard]] std::array<std::array<float, kBlockSize>, kOrder> makeZeroedDelayData()
{
    std::array<std::array<float, kBlockSize>, kOrder> delayData{};
    for (auto& blk : delayData)
    {
        blk.fill(0.0f);
    }
    return delayData;
}
}

TEST(CallbackManagerTest, HasCallbackReflectsSetAndRemove)
{
    CallbackManager<kOrder, kBlockSize> manager;
    EXPECT_FALSE(manager.hasCallback(0));

    manager.setCallback(0, std::make_shared<AddConstantProcessor>(1.0f));
    EXPECT_TRUE(manager.hasCallback(0));
    EXPECT_FALSE(manager.hasCallback(1));

    manager.removeCallback(0);
    EXPECT_FALSE(manager.hasCallback(0));
}

TEST(CallbackManagerTest, OutOfRangeIndexIsSafeNoOp)
{
    CallbackManager<kOrder, kBlockSize> manager;

    EXPECT_FALSE(manager.hasCallback(kOrder));
    EXPECT_FALSE(manager.hasCallback(kOrder + 100));

    manager.setCallback(kOrder, std::make_shared<AddConstantProcessor>(1.0f));
    EXPECT_FALSE(manager.hasCallback(kOrder));

    manager.removeCallback(kOrder);

    auto delayData = makeZeroedDelayData();
    manager.processCallbacks(delayData);
    for (const auto& blk : delayData)
    {
        for (const auto sample : blk)
        {
            EXPECT_FLOAT_EQ(sample, 0.0f);
        }
    }
}

TEST(CallbackManagerTest, ProcessCallbacksDispatchesToRegisteredIndexOnly)
{
    CallbackManager<kOrder, kBlockSize> manager;
    manager.setCallback(1, std::make_shared<AddConstantProcessor>(10.0f));
    manager.setCallback(3, std::make_shared<AddConstantProcessor>(100.0f));

    auto delayData = makeZeroedDelayData();
    manager.processCallbacks(delayData);

    for (const auto sample : delayData[0])
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
    for (const auto sample : delayData[1])
    {
        EXPECT_FLOAT_EQ(sample, 10.0f);
    }
    for (const auto sample : delayData[2])
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
    for (const auto sample : delayData[3])
    {
        EXPECT_FLOAT_EQ(sample, 100.0f);
    }
}

TEST(CallbackManagerTest, ResetForwardsOnlyToRegisteredProcessors)
{
    CallbackManager<kOrder, kBlockSize> manager;
    auto first = std::make_shared<AddConstantProcessor>(1.0f);
    auto second = std::make_shared<AddConstantProcessor>(2.0f);
    manager.setCallback(0, first);
    manager.setCallback(2, second);

    manager.reset();
    manager.reset();

    EXPECT_EQ(first->resetCount(), 2u);
    EXPECT_EQ(second->resetCount(), 2u);
}

TEST(CallbackManagerTest, RemovedCallbackNoLongerReceivesProcessOrReset)
{
    CallbackManager<kOrder, kBlockSize> manager;
    auto processor = std::make_shared<AddConstantProcessor>(5.0f);
    manager.setCallback(0, processor);
    manager.removeCallback(0);

    auto delayData = makeZeroedDelayData();
    manager.processCallbacks(delayData);
    manager.reset();

    EXPECT_EQ(processor->processCount(), 0u);
    EXPECT_EQ(processor->resetCount(), 0u);
    for (const auto sample : delayData[0])
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

}
