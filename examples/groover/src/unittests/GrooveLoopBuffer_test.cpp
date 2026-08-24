#include <gtest/gtest.h>

#include "impl/GrooveLoopBuffer.h"

TEST(GrooveLoopBufferTest, WriteThenReadRoundTrips)
{
    GrooveLoopBuffer buffer(10.f, 1.f); // 10 frames capacity
    buffer.writeFrame(1.f, 2.f);
    buffer.writeFrame(3.f, 4.f);

    const auto first = buffer.readFrame();
    EXPECT_FLOAT_EQ(first[0], 1.f);
    EXPECT_FLOAT_EQ(first[1], 2.f);

    const auto second = buffer.readFrame();
    EXPECT_FLOAT_EQ(second[0], 3.f);
    EXPECT_FLOAT_EQ(second[1], 4.f);
}

TEST(GrooveLoopBufferTest, FramesAheadTracksTheUnreadGap)
{
    GrooveLoopBuffer buffer(10.f, 1.f);
    EXPECT_EQ(buffer.framesAhead(), 0u);

    buffer.writeFrame(0.f, 0.f);
    buffer.writeFrame(0.f, 0.f);
    buffer.writeFrame(0.f, 0.f);
    EXPECT_EQ(buffer.framesAhead(), 3u);

    static_cast<void>(buffer.readFrame());
    EXPECT_EQ(buffer.framesAhead(), 2u);
}

TEST(GrooveLoopBufferTest, WrapsCleanlyAtCapacityBoundary)
{
    GrooveLoopBuffer buffer(4.f, 1.f); // 4 frames capacity

    for (int i = 0; i < 4; ++i)
    {
        buffer.writeFrame(static_cast<float>(i), static_cast<float>(i) * 10.f);
    }
    EXPECT_EQ(buffer.framesAhead(), 4u);

    for (int i = 0; i < 4; ++i)
    {
        const auto frame = buffer.readFrame();
        EXPECT_FLOAT_EQ(frame[0], static_cast<float>(i));
        EXPECT_FLOAT_EQ(frame[1], static_cast<float>(i) * 10.f);
    }
    EXPECT_EQ(buffer.framesAhead(), 0u);

    // Write cursor has wrapped past capacity once already; confirm it keeps
    // writing/reading correctly on the second lap.
    buffer.writeFrame(9.f, 90.f);
    const auto wrapped = buffer.readFrame();
    EXPECT_FLOAT_EQ(wrapped[0], 9.f);
    EXPECT_FLOAT_EQ(wrapped[1], 90.f);
}

TEST(GrooveLoopBufferTest, ResetZeroesBothCursors)
{
    GrooveLoopBuffer buffer(10.f, 1.f);
    buffer.writeFrame(1.f, 1.f);
    buffer.writeFrame(1.f, 1.f);
    static_cast<void>(buffer.readFrame());
    ASSERT_EQ(buffer.framesAhead(), 1u);

    buffer.reset();
    EXPECT_EQ(buffer.framesAhead(), 0u);

    buffer.writeFrame(5.f, 6.f);
    const auto frame = buffer.readFrame();
    EXPECT_FLOAT_EQ(frame[0], 5.f);
    EXPECT_FLOAT_EQ(frame[1], 6.f);
}
