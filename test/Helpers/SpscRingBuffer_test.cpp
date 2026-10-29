#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <span>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "Helpers/SpscRingBuffer.h"

namespace AbacDsp::Test
{

TEST(SpscRingBufferTest, CapacityRoundsUpToPowerOfTwo)
{
    EXPECT_EQ(SpscRingBuffer<float>(1000).capacity(), 1024u);
    EXPECT_EQ(SpscRingBuffer<float>(1024).capacity(), 1024u);
    EXPECT_EQ(SpscRingBuffer<float>(0).capacity(), 2u);
}

TEST(SpscRingBufferTest, PopReturnsPushedItemsInOrder)
{
    SpscRingBuffer<int> ring(8);
    const std::array<int, 3> in{1, 2, 3};
    EXPECT_EQ(ring.push(in), 3u);
    EXPECT_EQ(ring.size(), 3u);

    std::array<int, 8> out{};
    ASSERT_EQ(ring.pop(out), 3u);
    EXPECT_EQ(out[0], 1);
    EXPECT_EQ(out[1], 2);
    EXPECT_EQ(out[2], 3);
    EXPECT_EQ(ring.size(), 0u);
}

TEST(SpscRingBufferTest, PopOnEmptyReturnsZero)
{
    SpscRingBuffer<int> ring(8);
    std::array<int, 4> out{};
    EXPECT_EQ(ring.pop(out), 0u);
}

TEST(SpscRingBufferTest, PushDropsWhatDoesNotFit)
{
    SpscRingBuffer<int> ring(4);
    const std::array<int, 6> in{1, 2, 3, 4, 5, 6};
    EXPECT_EQ(ring.push(in), 4u);
    EXPECT_EQ(ring.push(in), 0u);

    std::array<int, 6> out{};
    ASSERT_EQ(ring.pop(out), 4u);
    EXPECT_EQ(out[3], 4);
}

TEST(SpscRingBufferTest, WrapsAroundTheEnd)
{
    SpscRingBuffer<int> ring(4);
    std::array<int, 4> out{};
    for (int round = 0; round < 10; ++round)
    {
        const std::array<int, 3> in{round, round + 1, round + 2};
        ASSERT_EQ(ring.push(in), 3u);
        ASSERT_EQ(ring.pop(out), 3u);
        EXPECT_EQ(out[0], round);
        EXPECT_EQ(out[2], round + 2);
    }
}

TEST(SpscRingBufferTest, TwoThreadsTransferEveryItemInOrder)
{
    constexpr int kTotal{200000};
    SpscRingBuffer<int> ring(256);
    std::vector<int> received;
    received.reserve(kTotal);
    {
        const std::jthread producer(
            [&ring]
            {
                std::array<int, 16> block{};
                int next = 0;
                while (next < kTotal)
                {
                    std::iota(block.begin(), block.end(), next);
                    const auto wanted = std::min<size_t>(block.size(), static_cast<size_t>(kTotal - next));
                    next += static_cast<int>(ring.push(std::span<const int>(block.data(), wanted)));
                }
            });
        std::array<int, 37> chunk{};
        while (received.size() < static_cast<size_t>(kTotal))
        {
            const auto count = ring.pop(chunk);
            received.insert(received.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
        }
    }
    for (int i = 0; i < kTotal; ++i)
    {
        ASSERT_EQ(received[static_cast<size_t>(i)], i);
    }
}

}
