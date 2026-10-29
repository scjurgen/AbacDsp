#include <cmath>

#include "gtest/gtest.h"

#include "Analysis/PitchReading.h"

namespace AbacDsp::Test
{

TEST(PitchReadingTest, ConcertAIsInTune)
{
    const auto reading = pitchReadingOf(440.f);
    EXPECT_EQ(reading.midiNote, 69);
    EXPECT_EQ(reading.name(), "A");
    EXPECT_EQ(reading.octave(), 4);
    EXPECT_NEAR(reading.cents, 0.f, 1e-3f);
}

TEST(PitchReadingTest, SlightlySharpReadsPositiveCents)
{
    EXPECT_NEAR(pitchReadingOf(441.f).cents, 3.9302f, 1e-3f);
    EXPECT_NEAR(pitchReadingOf(439.f).cents, -3.9390f, 1e-3f);
}

TEST(PitchReadingTest, NamesSharpsAndOctaves)
{
    const auto aSharp = pitchReadingOf(466.1638f);
    EXPECT_EQ(aSharp.name(), "A#");
    EXPECT_NEAR(aSharp.cents, 0.f, 1e-2f);

    const auto middleC = pitchReadingOf(261.6256f);
    EXPECT_EQ(middleC.midiNote, 60);
    EXPECT_EQ(middleC.octave(), 4);

    const auto lowE = pitchReadingOf(41.2034f);
    EXPECT_EQ(lowE.name(), "E");
    EXPECT_EQ(lowE.octave(), 1);
}

TEST(PitchReadingTest, ReferenceShiftsReadings)
{
    const auto reading = pitchReadingOf(440.f, 443.f);
    EXPECT_EQ(reading.midiNote, 69);
    EXPECT_NEAR(reading.cents, -11.7647f, 1e-3f);

    EXPECT_NEAR(pitchReadingOf(443.f, 443.f).cents, 0.f, 1e-3f);
}

TEST(PitchReadingTest, HalfSemitoneEdgeRoundsUp)
{
    const float quarterToneAboveA = 440.f * std::exp2(0.5f / 12.f);
    const auto below = pitchReadingOf(quarterToneAboveA * 0.9999f);
    EXPECT_EQ(below.midiNote, 69);
    EXPECT_GT(below.cents, 49.f);

    const auto above = pitchReadingOf(quarterToneAboveA * 1.0001f);
    EXPECT_EQ(above.midiNote, 70);
    EXPECT_LT(above.cents, -49.f);
}

}
