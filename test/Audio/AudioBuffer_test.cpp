#include <gtest/gtest.h>
#include <array>
#include "Audio/AudioBuffer.h"
#include <span>

namespace AbacDsp::Test
{

TEST(AudioBufferTest, ConstructorAndSize)
{
    constexpr AudioBuffer<2, 100> sut;
    EXPECT_EQ(sut.numFrames(), 100);
    EXPECT_EQ(sut.numChannels(), 2);
}

TEST(AudioBufferTest, DataAccess)
{
    AudioBuffer<2, 100> sut;
    sut(50, 0) = 0.5f;
    sut(50, 1) = -0.5f;
    EXPECT_FLOAT_EQ(sut(50, 0), 0.5f);
    EXPECT_FLOAT_EQ(sut(50, 1), -0.5f);
}

TEST(AudioBufferTest, SubscriptOperatorRead)
{
    AudioBuffer<2, 100> sut;
    sut(50, 0) = 0.5f;
    sut(50, 1) = -0.5f;
    EXPECT_FLOAT_EQ(sut[50][0], 0.5f);
    EXPECT_FLOAT_EQ(sut[50][1], -0.5f);
}

TEST(AudioBufferTest, SubscriptOperatorWrite)
{
    AudioBuffer<2, 100> sut;
    sut[50][0] = 0.75f;
    sut[50][1] = -0.25f;
    EXPECT_FLOAT_EQ(sut(50, 0), 0.75f);
    EXPECT_FLOAT_EQ(sut(50, 1), -0.25f);
}

TEST(AudioBufferTest, SubscriptOperatorFrameAssignment)
{
    AudioBuffer<2, 100> sut;
    AudioBuffer<2, 100>::Frame frame = {1.0f, -1.0f};
    sut[42] = frame;
    EXPECT_FLOAT_EQ(sut[42][0], 1.0f);
    EXPECT_FLOAT_EQ(sut[42][1], -1.0f);
}

TEST(AudioBufferTest, SubscriptOperatorConversion)
{
    AudioBuffer<2, 100> sut;
    sut(25, 0) = 0.3f;
    sut(25, 1) = 0.7f;
    AudioBuffer<2, 100>::Frame frame = sut[25];
    EXPECT_FLOAT_EQ(frame[0], 0.3f);
    EXPECT_FLOAT_EQ(frame[1], 0.7f);
}

TEST(AudioBufferTest, SubscriptOperatorConst)
{
    AudioBuffer<2, 100> buffer;
    buffer(10, 0) = 0.2f;
    buffer(10, 1) = 0.8f;
    const auto& constBuffer = buffer;
    EXPECT_FLOAT_EQ(constBuffer[10][0], 0.2f);
    EXPECT_FLOAT_EQ(constBuffer[10][1], 0.8f);
}

TEST(AudioBufferTest, FrameIterator)
{
    AudioBuffer<2, 100> sut;
    sut(0, 0) = 1.0f;
    sut(0, 1) = -1.0f;
    sut(1, 0) = 0.5f;
    sut(1, 1) = -0.5f;
    auto it = sut.begin();
    EXPECT_FLOAT_EQ((*it)[0], 1.0f);
    EXPECT_FLOAT_EQ((*it)[1], -1.0f);
    ++it;
    EXPECT_FLOAT_EQ((*it)[0], 0.5f);
    EXPECT_FLOAT_EQ((*it)[1], -0.5f);
}

TEST(AudioBufferTest, Mux)
{
    AudioBuffer<2, 3> sut;
    std::array<float, 3> left = {1.0f, 2.0f, 3.0f};
    std::array<float, 3> right = {-1.0f, -2.0f, -3.0f};
    sut.mux({left, right});
    EXPECT_FLOAT_EQ(sut(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(sut(0, 1), -1.0f);
    EXPECT_FLOAT_EQ(sut(1, 0), 2.0f);
    EXPECT_FLOAT_EQ(sut(1, 1), -2.0f);
    EXPECT_FLOAT_EQ(sut(2, 0), 3.0f);
    EXPECT_FLOAT_EQ(sut(2, 1), -3.0f);
}

TEST(AudioBufferTest, Demux)
{
    AudioBuffer<2, 3> sut;
    sut(0, 0) = 1.0f;
    sut(0, 1) = -1.0f;
    sut(1, 0) = 2.0f;
    sut(1, 1) = -2.0f;
    sut(2, 0) = 3.0f;
    sut(2, 1) = -3.0f;
    std::array<float, 3> left{};
    std::array<float, 3> right{};
    sut.demux({left, right});
    EXPECT_FLOAT_EQ(left[0], 1.0f);
    EXPECT_FLOAT_EQ(left[1], 2.0f);
    EXPECT_FLOAT_EQ(left[2], 3.0f);
    EXPECT_FLOAT_EQ(right[0], -1.0f);
    EXPECT_FLOAT_EQ(right[1], -2.0f);
    EXPECT_FLOAT_EQ(right[2], -3.0f);
}

TEST(AudioBufferTest, MuxDemuxConsistency)
{
    AudioBuffer<2, 100> sut;
    std::array<float, 100> originalLeft{};
    std::array<float, 100> originalRight{};
    for (size_t i = 0; i < 100; ++i)
    {
        originalLeft[i] = static_cast<float>(i);
        originalRight[i] = -static_cast<float>(i);
    }
    sut.mux({originalLeft, originalRight});
    std::array<float, 100> resultLeft{};
    std::array<float, 100> resultRight{};
    sut.demux({resultLeft, resultRight});
    EXPECT_EQ(originalLeft, resultLeft);
    EXPECT_EQ(originalRight, resultRight);
}

TEST(AudioBufferTest, ConstIterator)
{
    constexpr AudioBuffer<2, 100> sut;
    auto it = sut.cbegin();
    auto end = sut.cend();
    EXPECT_NE(it, end);
    EXPECT_EQ(std::distance(it, end), 100);
}

TEST(AudioBufferTest, DifferentChannelCounts)
{
    constexpr AudioBuffer<1, 50> mono;
    constexpr AudioBuffer<4, 25> quad;
    constexpr AudioBuffer<6, 10> surround;
    EXPECT_EQ(mono.numChannels(), 1);
    EXPECT_EQ(quad.numChannels(), 4);
    EXPECT_EQ(surround.numChannels(), 6);
    EXPECT_EQ(mono.numFrames(), 50);
    EXPECT_EQ(quad.numFrames(), 25);
    EXPECT_EQ(surround.numFrames(), 10);
}

TEST(AudioBufferTest, MuxThrowsOnSizeMismatch)
{
    AudioBuffer<2, 100> sut{};
    std::array<float, 50> left{};
    std::array<float, 50> right{};
    EXPECT_THROW(sut.mux({left, right}), std::invalid_argument);
}

TEST(AudioBufferTest, DemuxThrowsOnSizeMismatch)
{
    constexpr AudioBuffer<2, 100> sut{};
    std::array<float, 50> left{};
    std::array<float, 50> right{};
    EXPECT_THROW(sut.demux({left, right}), std::invalid_argument);
}

}
