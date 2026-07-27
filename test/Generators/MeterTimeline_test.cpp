#include <gtest/gtest.h>

#include "Generators/MeterTimeline.h"

namespace AbacDsp::test
{

TEST(MeterTimelineTest, EmptyTimelineReadsAsConstantDefault)
{
    const MeterTimeline timeline;
    EXPECT_TRUE(timeline.empty());
    const MeterSegment& seg = timeline.segmentForBar(7);
    EXPECT_EQ(seg.beatsPerBar, 4u);
    EXPECT_FALSE(seg.eighthUnit);
}

TEST(MeterTimelineTest, SingleSegmentCoversEveryBar)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 7, true);
    EXPECT_EQ(timeline.segmentCount(), 1u);
    for (const size_t bar : {0u, 1u, 100u})
    {
        const MeterSegment& seg = timeline.segmentForBar(bar);
        EXPECT_EQ(seg.beatsPerBar, 7u);
        EXPECT_TRUE(seg.eighthUnit);
    }
}

TEST(MeterTimelineTest, LaterSegmentAppliesFromItsStartBarOnward)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false);
    timeline.addSegment(2, 7, true);
    timeline.addSegment(5, 3, false);

    EXPECT_EQ(timeline.segmentForBar(0).beatsPerBar, 4u);
    EXPECT_EQ(timeline.segmentForBar(1).beatsPerBar, 4u);
    EXPECT_EQ(timeline.segmentForBar(2).beatsPerBar, 7u);
    EXPECT_TRUE(timeline.segmentForBar(2).eighthUnit);
    EXPECT_EQ(timeline.segmentForBar(4).beatsPerBar, 7u);
    EXPECT_EQ(timeline.segmentForBar(5).beatsPerBar, 3u);
    EXPECT_FALSE(timeline.segmentForBar(5).eighthUnit);
    EXPECT_EQ(timeline.segmentForBar(999).beatsPerBar, 3u);
}

TEST(MeterTimelineTest, DuplicateConsecutiveSegmentIsDropped)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false);
    timeline.addSegment(3, 4, false); // same meter as the previous segment
    EXPECT_EQ(timeline.segmentCount(), 1u);

    timeline.addSegment(6, 6, true);
    timeline.addSegment(8, 6, true); // same meter as the previous segment again
    EXPECT_EQ(timeline.segmentCount(), 2u);
}

TEST(MeterTimelineTest, ClearResetsSegmentsAndFrameMap)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false);
    timeline.buildFrameMap(4, 1000.f);
    ASSERT_GT(timeline.totalFrames(), 0u);

    timeline.clear();
    EXPECT_TRUE(timeline.empty());
    EXPECT_EQ(timeline.totalFrames(), 0u);
}

TEST(MeterTimelineTest, FrameMapAccumulatesConstantMeterBarLengths)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false);
    constexpr float kSamplesPerQuarterBeat = 1000.f;
    timeline.buildFrameMap(3, kSamplesPerQuarterBeat);

    constexpr size_t kSamplesPerBar = 4 * 1000;
    EXPECT_EQ(timeline.frameOffsetForBar(0), 0u);
    EXPECT_EQ(timeline.frameOffsetForBar(1), kSamplesPerBar);
    EXPECT_EQ(timeline.frameOffsetForBar(2), 2 * kSamplesPerBar);
    EXPECT_EQ(timeline.totalFrames(), 3 * kSamplesPerBar);
}

TEST(MeterTimelineTest, FrameMapHonorsMeterChangeMidLoop)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false); // 2 bars of 4/4
    timeline.addSegment(2, 7, false); // then 1 bar of 7/4
    constexpr float kSamplesPerQuarterBeat = 1000.f;
    timeline.buildFrameMap(3, kSamplesPerQuarterBeat);

    constexpr size_t kBar4_4 = 4 * 1000;
    constexpr size_t kBar7_4 = 7 * 1000;
    EXPECT_EQ(timeline.frameOffsetForBar(0), 0u);
    EXPECT_EQ(timeline.frameOffsetForBar(1), kBar4_4);
    EXPECT_EQ(timeline.frameOffsetForBar(2), 2 * kBar4_4);
    EXPECT_EQ(timeline.frameOffsetForBar(3), 2 * kBar4_4 + kBar7_4);
    EXPECT_EQ(timeline.totalFrames(), 2 * kBar4_4 + kBar7_4);
}

TEST(MeterTimelineTest, EighthNoteUnitHalvesBeatDuration)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 6, true); // 6/8: beat = eighth note, half a quarter-note beat
    constexpr float kSamplesPerQuarterBeat = 1000.f;
    timeline.buildFrameMap(1, kSamplesPerQuarterBeat);

    constexpr size_t kExpectedBarLength = 6 * 500; // 6 beats * (1000/2) samples per beat
    EXPECT_EQ(timeline.totalFrames(), kExpectedBarLength);
}

TEST(MeterTimelineTest, FrameOffsetClampsPastLastBuiltBar)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false);
    timeline.buildFrameMap(2, 1000.f);

    EXPECT_EQ(timeline.frameOffsetForBar(2), timeline.totalFrames());
    EXPECT_EQ(timeline.frameOffsetForBar(50), timeline.totalFrames());
}

TEST(MeterTimelineTest, BarCountForFramesMatchesConstantMeter)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false);
    constexpr float kSamplesPerQuarterBeat = 1000.f;
    constexpr size_t kSamplesPerBar = 4 * 1000;

    EXPECT_EQ(timeline.barCountForFrames(3 * kSamplesPerBar, kSamplesPerQuarterBeat), 3u);
}

TEST(MeterTimelineTest, BarCountForFramesHonorsMeterChangeMidTake)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 4, false); // 2 bars of 4/4
    timeline.addSegment(2, 7, false); // then 2 bars of 7/4
    constexpr float kSamplesPerQuarterBeat = 1000.f;
    constexpr size_t kBar4_4 = 4 * 1000;
    constexpr size_t kBar7_4 = 7 * 1000;

    const size_t totalFrames = 2 * kBar4_4 + 2 * kBar7_4;
    EXPECT_EQ(timeline.barCountForFrames(totalFrames, kSamplesPerQuarterBeat), 4u);
}

TEST(MeterTimelineTest, BarCountForFramesRoundTripsThroughBuildFrameMap)
{
    MeterTimeline timeline;
    timeline.addSegment(0, 6, true);
    timeline.addSegment(3, 4, false);
    constexpr float kSamplesPerQuarterBeat = 833.f;
    constexpr size_t kExpectedBars = 5;

    timeline.buildFrameMap(kExpectedBars, kSamplesPerQuarterBeat);
    const size_t totalFrames = timeline.totalFrames();

    EXPECT_EQ(timeline.barCountForFrames(totalFrames, kSamplesPerQuarterBeat), kExpectedBars);
}

}
