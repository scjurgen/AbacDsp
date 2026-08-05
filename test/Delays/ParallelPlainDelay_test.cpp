

#include <cmath>
#include <gtest/gtest.h>
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

// Regression test: m_currentDelayWidth used to brace-init only element 0 to
// MAXSIZE/8, leaving every other channel at 0. setRelativeHead() reads that
// default when called before setSize(), so an aux head set up identically on
// two channels used to land at different delays depending on channel index.
// Channel 0 and channel 1 get an identical setRelativeHead() call and an
// identical input signal, so their aux outputs must match at every step;
// any divergence means their default delay widths differed.
TEST(ParallelPlainDelay, defaultDelayWidthIsUniformAcrossChannelsForAuxHead)
{
    constexpr size_t BlockSize{4};
    constexpr size_t Channels{2};
    constexpr size_t MaxSize{16};
    ParallelPlainDelay<BlockSize, Channels, MaxSize, 2> sut;

    sut.setRelativeHead(1, 0, 0);
    sut.setRelativeHead(1, 1, 0);

    std::array<std::array<float, BlockSize>, Channels> mainTarget{};
    std::array<std::array<float, BlockSize>, Channels> auxTarget{};

    float sample = 1.f;
    for (int block = 0; block < 10; ++block)
    {
        std::array<std::array<float, BlockSize>, Channels> src{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            src[0][i] = sample;
            src[1][i] = sample;
            sample += 1.f;
        }
        sut.processBlock(src, mainTarget);
        sut.processHead(1, auxTarget);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            EXPECT_FLOAT_EQ(auxTarget[0][i], auxTarget[1][i]) << "block " << block << " sample " << i;
        }
    }
}
}
