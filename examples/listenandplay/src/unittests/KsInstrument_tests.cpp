#include <gtest/gtest.h>

#include "impl/KsInstrument.h"

using namespace KsInstrument;

TEST(KsInstrumentTest, EveryIdHasANamedPreset)
{
    for (size_t i = 0; i < kPresets.size(); ++i)
    {
        const auto id = idFromIndex(static_cast<long long>(i));
        ASSERT_TRUE(id.has_value());
        EXPECT_FALSE(presetOf(*id).name.empty());
    }
    EXPECT_FALSE(idFromIndex(-1).has_value());
    EXPECT_FALSE(idFromIndex(static_cast<long long>(kPresets.size())).has_value());
}

TEST(KsInstrumentTest, EveryRangeHoldsTheTwoOctaveWindowOfTheWidestLesson)
{
    for (const auto& preset : kPresets)
    {
        EXPECT_GE(preset.highestNote() - preset.lowestNote, 24) << preset.name;
    }
}

TEST(KsInstrumentTest, TrimInterpolatesLinearlyBetweenMeasuredPoints)
{
    for (const auto& preset : kPresets)
    {
        const float low = static_cast<float>(preset.lowestNote);
        const float step = static_cast<float>(kTrimStepSemitones);
        EXPECT_FLOAT_EQ(trimDbAt(preset, low), preset.trimDb[0]) << preset.name;
        EXPECT_FLOAT_EQ(trimDbAt(preset, low + step), preset.trimDb[1]) << preset.name;
        EXPECT_FLOAT_EQ(trimDbAt(preset, low + 0.5f * step), 0.5f * (preset.trimDb[0] + preset.trimDb[1]))
            << preset.name;
    }
}

TEST(KsInstrumentTest, TrimIsHeldFlatOutsideTheMeasuredRange)
{
    for (const auto& preset : kPresets)
    {
        EXPECT_FLOAT_EQ(trimDbAt(preset, 0.f), preset.trimDb.front()) << preset.name;
        EXPECT_FLOAT_EQ(trimDbAt(preset, 127.f), preset.trimDb.back()) << preset.name;
    }
}

TEST(KsInstrumentTest, VelocityStepsAreEvenInDecibels)
{
    EXPECT_FLOAT_EQ(velocityDb(127), 0.f);
    EXPECT_NEAR(velocityDb(103), -6.f, 1e-5f);
    EXPECT_NEAR(velocityDb(80) - velocityDb(55), velocityDb(105) - velocityDb(80), 1e-5f);
}

TEST(KsInstrumentTest, NoteGainRisesWithVelocity)
{
    const auto& preset = presetOf(Id::Guitar);
    float previous = 0.f;
    for (int velocity = 1; velocity <= 127; ++velocity)
    {
        const float gain = noteGain(preset, 60, static_cast<uint8_t>(velocity));
        EXPECT_GT(gain, previous);
        previous = gain;
    }
}

TEST(KsInstrumentTest, EveryPresetHasADistinctNameAndAnAscendingRange)
{
    for (size_t i = 0; i < kPresets.size(); ++i)
    {
        EXPECT_GT(kPresets[i].highestNote(), kPresets[i].lowestNote);
        EXPECT_LE(kPresets[i].highestNote(), 108);
        for (size_t j = i + 1; j < kPresets.size(); ++j)
        {
            EXPECT_NE(kPresets[i].name, kPresets[j].name);
        }
    }
    EXPECT_EQ(presetOf(Id::MusicBox).name, "Music box");
}
