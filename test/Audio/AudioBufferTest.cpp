#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Audio/AudioBuffer.h"

#include <array>

class AudioBufferTest : public ::testing::Test
{
  protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AudioBufferTest, ConstructorAndSize)
{
    constexpr AudioBuffer<2, 100> sut;
    EXPECT_EQ(sut.numFrames(), 100);
    EXPECT_EQ(sut.numChannels(), 2);
}

// Remove Resize test as it's no longer applicable

TEST_F(AudioBufferTest, DataAccess)
{
    AudioBuffer<2, 100> sut;
    sut(50, 0) = 0.5f;
    sut(50, 1) = -0.5f;
    EXPECT_FLOAT_EQ(sut(50, 0), 0.5f);
    EXPECT_FLOAT_EQ(sut(50, 1), -0.5f);
}

TEST_F(AudioBufferTest, FrameIterator)
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

TEST_F(AudioBufferTest, Mux)
{
    AudioBuffer<2, 3> sut;
    std::array left = {1.0f, 2.0f, 3.0f};
    std::array right = {-1.0f, -2.0f, -3.0f};

    sut.mux({left, right});

    EXPECT_FLOAT_EQ(sut(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(sut(0, 1), -1.0f);
    EXPECT_FLOAT_EQ(sut(1, 0), 2.0f);
    EXPECT_FLOAT_EQ(sut(1, 1), -2.0f);
    EXPECT_FLOAT_EQ(sut(2, 0), 3.0f);
    EXPECT_FLOAT_EQ(sut(2, 1), -3.0f);
}

TEST_F(AudioBufferTest, Demux)
{
    AudioBuffer<2, 3> sut;
    sut(0, 0) = 1.0f;
    sut(0, 1) = -1.0f;
    sut(1, 0) = 2.0f;
    sut(1, 1) = -2.0f;
    sut(2, 0) = 3.0f;
    sut(2, 1) = -3.0f;

    std::array<float, 3> left{}, right{};
    sut.demux({left, right});

    EXPECT_FLOAT_EQ(left[0], 1.0f);
    EXPECT_FLOAT_EQ(left[1], 2.0f);
    EXPECT_FLOAT_EQ(left[2], 3.0f);
    EXPECT_FLOAT_EQ(right[0], -1.0f);
    EXPECT_FLOAT_EQ(right[1], -2.0f);
    EXPECT_FLOAT_EQ(right[2], -3.0f);
}

TEST_F(AudioBufferTest, MuxDemuxConsistency)
{
    AudioBuffer<2, 100> sut;
    std::array<float, 100> originalLeft{}, originalRight{};
    for (size_t i = 0; i < 100; ++i)
    {
        originalLeft[i] = static_cast<float>(i);
        originalRight[i] = -static_cast<float>(i);
    }

    sut.mux({originalLeft, originalRight});

    std::array<float, 100> resultLeft{}, resultRight{};
    sut.demux({resultLeft, resultRight});

    EXPECT_EQ(originalLeft, resultLeft);
    EXPECT_EQ(originalRight, resultRight);
}

TEST_F(AudioBufferTest, ConstIterator)
{
    constexpr AudioBuffer<2, 100> sut;
    auto it = sut.cbegin();
    auto end = sut.cend();
    EXPECT_NE(it, end);
    EXPECT_EQ(std::distance(it, end), 100);
}

TEST_F(AudioBufferTest, DifferentChannelCounts)
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

TEST_F(AudioBufferTest, MuxThrowsOnSizeMismatch)
{
    AudioBuffer<2, 100> sut{};
    std::array<float, 99> left{};
    std::array<float, 100> right{};
    EXPECT_THROW(sut.mux({left, right}), std::invalid_argument);
}

TEST_F(AudioBufferTest, DemuxThrowsOnSizeMismatch)
{
    constexpr AudioBuffer<2, 100> sut{};
    std::array<float, 99> left{};
    std::array<float, 100> right{};
    EXPECT_THROW(sut.demux({left, right}), std::invalid_argument);
}
