

#include <gtest/gtest.h>
#include <cmath>
#include <random>

#include "Delays/ParallelPlainDelay.h"

namespace AbacDsp::Test
{

TEST(ParallelPlainDelay, checkSumAndSingle)
{
    constexpr size_t BlockSize{4};
    constexpr size_t Channels{2};
    ParallelPlainDelay<BlockSize, Channels, 16> sut;
    sut.setSize(0, 10);
    sut.setSize(1, 7);
    std::array<std::array<float, BlockSize>, Channels> src{};
    std::array<std::array<float, BlockSize>, Channels> target{};
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    src[0][0] = 1.f;
    src[1][0] = 1.f;
    sut.processBlock(src, target);
    src[0][0] = 0.f;
    src[1][0] = 0.f;
    sut.processBlock(src, target);
    EXPECT_EQ(target[1][3], 1);    // channel 1 after 7 steps
    sut.processBlock(src, target); // we processed 3 * 4 items
    EXPECT_EQ(target[0][2], 1);    // channel 0 after 10 steps
}

TEST(ParallelPlainDelay, checkShortDelay)
{
    constexpr size_t BlockSize{4};
    constexpr size_t Channels{2};
    ParallelPlainDelay<BlockSize, Channels, 16> sut;
    sut.setSize(0, 1);
    sut.setSize(1, 2);
    std::array<std::array<float, BlockSize>, Channels> src{};
    std::array<std::array<float, BlockSize>, Channels> target{};
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    src[0][0] = 1.f;
    src[1][0] = 1.f;
    sut.processBlock(src, target);
    src[0][0] = 0.f;
    src[1][0] = 0.f;
    EXPECT_EQ(target[0][1], 1); // after 1 steps
    EXPECT_EQ(target[1][2], 1); // after 2 steps
}

TEST(ParallelPlainDelay, overflowProtected)
{
    constexpr size_t BlockSize{4};
    constexpr size_t Channels{2};
    ParallelPlainDelay<BlockSize, Channels, 16> sut;
    sut.setSize(0, 1);
    sut.setSize(1, 2);
    std::array<std::array<float, BlockSize>, Channels> src{};
    std::array<std::array<float, BlockSize>, Channels> target{};
    // after 4 steps we wrap around (16/4),
    // read heads will be at 15 and 14
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    sut.processBlock(src, target);
    src[0][0] = 1.f;
    src[1][0] = 1.f;
    sut.processBlock(src, target);
    src[0][0] = 0.f;
    src[1][0] = 0.f;
    EXPECT_EQ(target[0][1], 1); // after 1 steps
    EXPECT_EQ(target[1][2], 1); // after 2 steps
}
}
