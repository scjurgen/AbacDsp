#include <gtest/gtest.h>

#include "Sampler/SequencePattern.h"

namespace AbacDsp::test
{

namespace
{
[[nodiscard]] SequenceEvent makeEvent(const size_t stepPosition, const size_t track = 0, const size_t sliceIndex = 0)
{
    SequenceEvent event{};
    event.stepPosition = stepPosition;
    event.track = track;
    event.sliceIndex = sliceIndex;
    return event;
}
}

TEST(SequencePatternTest, TotalStepsIsProductOfDimensions)
{
    const SequencePattern pattern(2, 4, 4); // 2 bars, 4 beats/bar, 4 steps/beat
    EXPECT_EQ(pattern.lengthBars(), 2u);
    EXPECT_EQ(pattern.beatsPerBar(), 4u);
    EXPECT_EQ(pattern.stepsPerBeat(), 4u);
    EXPECT_EQ(pattern.totalSteps(), 32u);
}

TEST(SequencePatternTest, StartsEmpty)
{
    const SequencePattern pattern(1, 4, 4);
    EXPECT_EQ(pattern.eventCount(), 0u);
    EXPECT_TRUE(pattern.events().empty());
}

TEST(SequencePatternTest, AddEventWithinRangeSucceeds)
{
    SequencePattern pattern(1, 4, 4); // totalSteps = 16
    EXPECT_TRUE(pattern.addEvent(makeEvent(0)));
    EXPECT_TRUE(pattern.addEvent(makeEvent(15)));
    EXPECT_EQ(pattern.eventCount(), 2u);
}

TEST(SequencePatternTest, AddEventAtOrBeyondTotalStepsIsRejected)
{
    SequencePattern pattern(1, 4, 4); // totalSteps = 16
    EXPECT_FALSE(pattern.addEvent(makeEvent(16)));
    EXPECT_FALSE(pattern.addEvent(makeEvent(100)));
    EXPECT_EQ(pattern.eventCount(), 0u);
}

TEST(SequencePatternTest, AddEventPreservesFields)
{
    SequencePattern pattern(1, 4, 4);
    SequenceEvent event{};
    event.stepPosition = 3;
    event.track = 2;
    event.sliceIndex = 5;
    event.gain = 0.5f;
    event.pitchRatio = 1.5f;
    event.reverse = true;
    event.randomizeSlice = true;
    event.timingOffsetFrames = -7;
    event.humanizeAmountFrames = 12.f;

    ASSERT_TRUE(pattern.addEvent(event));
    const SequenceEvent& stored = pattern.event(0);
    EXPECT_EQ(stored.stepPosition, 3u);
    EXPECT_EQ(stored.track, 2u);
    EXPECT_EQ(stored.sliceIndex, 5u);
    EXPECT_FLOAT_EQ(stored.gain, 0.5f);
    EXPECT_FLOAT_EQ(stored.pitchRatio, 1.5f);
    EXPECT_TRUE(stored.reverse);
    EXPECT_TRUE(stored.randomizeSlice);
    EXPECT_EQ(stored.timingOffsetFrames, -7);
    EXPECT_FLOAT_EQ(stored.humanizeAmountFrames, 12.f);
}

TEST(SequencePatternTest, RemoveEventErasesByIndex)
{
    SequencePattern pattern(1, 4, 4);
    pattern.addEvent(makeEvent(0, 0, 0));
    pattern.addEvent(makeEvent(4, 0, 1));
    pattern.addEvent(makeEvent(8, 0, 2));

    pattern.removeEvent(1);
    ASSERT_EQ(pattern.eventCount(), 2u);
    EXPECT_EQ(pattern.event(0).sliceIndex, 0u);
    EXPECT_EQ(pattern.event(1).sliceIndex, 2u);
}

TEST(SequencePatternTest, RemoveEventOutOfRangeIsNoOp)
{
    SequencePattern pattern(1, 4, 4);
    pattern.addEvent(makeEvent(0));
    pattern.removeEvent(5);
    EXPECT_EQ(pattern.eventCount(), 1u);
}

TEST(SequencePatternTest, ClearRemovesAllEvents)
{
    SequencePattern pattern(1, 4, 4);
    pattern.addEvent(makeEvent(0));
    pattern.addEvent(makeEvent(1));
    pattern.clear();
    EXPECT_EQ(pattern.eventCount(), 0u);
}

TEST(SequencePatternTest, MutableEventAccessAllowsInPlaceEdits)
{
    SequencePattern pattern(1, 4, 4);
    pattern.addEvent(makeEvent(0));
    pattern.event(0).gain = 0.25f;
    EXPECT_FLOAT_EQ(pattern.event(0).gain, 0.25f);
}

TEST(SequencePatternTest, EventIndicesAtStepFindsOnlyMatchingStepsInOrder)
{
    SequencePattern pattern(1, 4, 4);
    pattern.addEvent(makeEvent(4, 0, 0));
    pattern.addEvent(makeEvent(8, 1, 0));
    pattern.addEvent(makeEvent(4, 2, 0)); // layered with the first event

    const std::vector<size_t> atStep4 = pattern.eventIndicesAtStep(4);
    ASSERT_EQ(atStep4.size(), 2u);
    EXPECT_EQ(atStep4[0], 0u);
    EXPECT_EQ(atStep4[1], 2u);

    const std::vector<size_t> atStep8 = pattern.eventIndicesAtStep(8);
    ASSERT_EQ(atStep8.size(), 1u);
    EXPECT_EQ(atStep8[0], 1u);

    EXPECT_TRUE(pattern.eventIndicesAtStep(1).empty());
}

}