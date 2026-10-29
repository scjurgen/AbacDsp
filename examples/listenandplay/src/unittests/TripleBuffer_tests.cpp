#include <atomic>
#include <gtest/gtest.h>
#include <thread>

#include "impl/TripleBuffer.h"

TEST(TripleBufferTest, NothingPublishedGivesNothing)
{
    TripleBuffer<int> buffer;
    EXPECT_EQ(buffer.acquireNewest(), nullptr);
}

TEST(TripleBufferTest, ConsumerSeesThePublishedValueOnce)
{
    TripleBuffer<int> buffer;
    buffer.backSlot() = 7;
    buffer.publish();
    const int* value = buffer.acquireNewest();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 7);
    EXPECT_EQ(buffer.acquireNewest(), nullptr);
}

TEST(TripleBufferTest, ANewerValueReplacesAnUnreadOne)
{
    TripleBuffer<int> buffer;
    for (const int v : {1, 2, 3})
    {
        buffer.backSlot() = v;
        buffer.publish();
    }
    const int* value = buffer.acquireNewest();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 3);
}

TEST(TripleBufferTest, ConsumerNeverSeesATornValueWhileTheProducerKeepsPublishing)
{
    struct Pair
    {
        int a{0};
        int b{0};
    };
    TripleBuffer<Pair> buffer;
    std::atomic<bool> done{false};
    std::thread producer{[&]
                         {
                             for (int i = 1; i <= 200000; ++i)
                             {
                                 buffer.backSlot() = {i, i};
                                 buffer.publish();
                             }
                             done = true;
                         }};
    int last = 0;
    bool torn = false;
    while (!done.load())
    {
        if (const Pair* value = buffer.acquireNewest())
        {
            torn = torn || value->a != value->b || value->a < last;
            last = value->a;
        }
    }
    producer.join();
    EXPECT_FALSE(torn);
}
