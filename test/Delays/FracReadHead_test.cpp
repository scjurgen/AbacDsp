#include "Delays/FracReadHead.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <cmath>
#include <vector>

namespace AbacDsp::Test
{


constexpr size_t BufferSize{2048};
constexpr float SampleRate{48000.f};

TEST(FracReadHeadTest, advancesLinearWhenNotAdjusting)
{
    FracReadHead<BufferSize> sut(SampleRate);
    size_t hd = 0;
    for (size_t i = 0; i < 100; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_FLOAT_EQ(p, static_cast<float>(i + 1));
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::Idle);
    }
}

TEST(FracReadHeadTest, adjustNearing)
{
    FracReadHead<BufferSize> sut(SampleRate);
    size_t hd = 0;
    sut.setNewDelta(1000, 2.f, 10);
    for (size_t i = 0; i < 2048; ++i)
    {
        sut.step((hd++) % BufferSize);
        if (!sut.isAdjusting())
        {
            break;
        }
    }
    float previous = sut.step((hd++) % BufferSize);
    sut.setNewDelta(500, 2.f, 10);
    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LT(p - previous, 2);

        previous = p;
        EXPECT_TRUE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::RampIn) << "failed at " << i;
    }
    for (size_t i = 0; i < 495; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LE(p - previous, 2);
        previous = p;
        EXPECT_TRUE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::Constant) << "failed at " << i;
    }
    for (size_t i = 0; i < 10; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LE(p - previous, 2);
        previous = p;
        EXPECT_TRUE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::RampOut) << "failed at " << i;
    }
    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_LE(p - previous, 1);
        previous = p;
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::Idle) << "failed at " << i;
    }
}

TEST(FracReadHeadTest, adjustDistancing)
{
    FracReadHead<BufferSize> sut(SampleRate);
    size_t hd = 0;
    float previous = 0;
    sut.setNewDelta(500, 2.f, 10);
    for (size_t i = 0; i < 2048; ++i)
    {
        sut.step((hd++) % BufferSize);
        if (!sut.isAdjusting())
        {
            break;
        }
    }
    previous = sut.step((hd++) % BufferSize);
    sut.setNewDelta(1000, 2.f, 10);
    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LE(p - previous, 1);
        previous = p;
        EXPECT_TRUE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::RampIn) << "failed at " << i;
    }
    for (size_t i = 0; i < 495; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LE(p - previous, 1);
        previous = p;
        EXPECT_TRUE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::Constant) << "failed at " << i;
    }
    for (size_t i = 0; i < 10; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LE(p - previous, 1);
        previous = p;
        EXPECT_TRUE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::RampOut) << "failed at " << i;
    }
    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % BufferSize);
        EXPECT_GT(p, previous);
        EXPECT_LE(p - previous, 1.5);
        previous = p;
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), FracReadHead<BufferSize>::TransitionPhase::Idle) << "failed at " << i;
    }
}
}