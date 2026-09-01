#include "gtest/gtest.h"

#include "Synthesizer/PitchQuantize.h"

namespace AbacDsp::Test
{

TEST(PitchQuantize, zeroLevelLeavesInputUnchanged)
{
    EXPECT_FLOAT_EQ(pitchQuantize(0.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(0.45f, 0.0f), 0.45f);
    EXPECT_FLOAT_EQ(pitchQuantize(0.5f, 0.0f), 0.5f);
    EXPECT_FLOAT_EQ(pitchQuantize(-0.5f, 0.0f), -0.5f);
}

TEST(PitchQuantize, fullLevelSnapsToNearestInteger)
{
    EXPECT_FLOAT_EQ(pitchQuantize(0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(0.45f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(0.5f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(0.75f, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(-0.75f, 1.0f), -1.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(8.75f, 1.0f), 9.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(-11.75f, 1.0f), -12.0f);
}

TEST(PitchQuantize, partialLevelBlendsTowardNearestInteger)
{
    EXPECT_FLOAT_EQ(pitchQuantize(0.0f, 0.5f), 0.0f);
    EXPECT_FLOAT_EQ(pitchQuantize(0.45f, 0.5f), 0.225f);
    EXPECT_FLOAT_EQ(pitchQuantize(8.75f, 0.5f), 8.875f);
}

TEST(PitchQuantize, worksForDoubleAsWellAsFloat)
{
    EXPECT_DOUBLE_EQ(pitchQuantize(8.75, 1.0), 9.0);
    EXPECT_DOUBLE_EQ(pitchQuantize(8.75, 0.0), 8.75);
}

}
