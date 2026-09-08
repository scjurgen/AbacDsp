#include <array>

#include "gtest/gtest.h"

#include "Harmony/HarmonicPalette.h"

namespace AbacDsp::Test
{

namespace
{
constexpr int kRegisterBoundSemitones{24}; // 2 octaves either side of home is a generous bound
}

TEST(StateTagTest, returnsTheMatchingIntrinsicTag)
{
    const HarmonicState state{
        .luminosity = 0.1f, .minorColor = 0.2f, .density = 0.3f, .ambiguity = 0.4f, .tension = 0.5f};
    EXPECT_FLOAT_EQ(*stateTag(state, WishKind::Luminosity), 0.1f);
    EXPECT_FLOAT_EQ(*stateTag(state, WishKind::MinorColor), 0.2f);
    EXPECT_FLOAT_EQ(*stateTag(state, WishKind::Density), 0.3f);
    EXPECT_FLOAT_EQ(*stateTag(state, WishKind::Ambiguity), 0.4f);
    EXPECT_FLOAT_EQ(*stateTag(state, WishKind::Tension), 0.5f);
}

TEST(StateTagTest, returnsNulloptForTheTwoRelationalAxes)
{
    const HarmonicState state{};
    EXPECT_FALSE(stateTag(state, WishKind::Closeness).has_value());
    EXPECT_FALSE(stateTag(state, WishKind::Mobility).has_value());
}

TEST(TransposeStateTest, shiftsEveryNoteByTheOffset)
{
    const auto original = defaultPalette()[3]; // 1maj7: {-1, 0, 4, 7}
    ASSERT_EQ(original.name, "1maj7");
    const auto transposed = transposeState(original, 5);
    ASSERT_EQ(transposed.voicing.size(), original.voicing.size());
    for (size_t i = 0; i < original.voicing.size(); ++i)
    {
        EXPECT_EQ(transposed.voicing.notes()[i], original.voicing.notes()[i] + 5);
    }
}

TEST(TransposeStateTest, preservesNameRegionAndTags)
{
    const auto original = defaultPalette()[3];
    const auto transposed = transposeState(original, -7);
    EXPECT_EQ(transposed.name, original.name);
    EXPECT_EQ(transposed.region, original.region);
    EXPECT_FLOAT_EQ(transposed.luminosity, original.luminosity);
    EXPECT_FLOAT_EQ(transposed.minorColor, original.minorColor);
    EXPECT_FLOAT_EQ(transposed.density, original.density);
    EXPECT_FLOAT_EQ(transposed.ambiguity, original.ambiguity);
    EXPECT_FLOAT_EQ(transposed.tension, original.tension);
}

class HarmonicPaletteTest : public ::testing::TestWithParam<size_t>
{
};

TEST_P(HarmonicPaletteTest, hasABetween2And9NoteVoicing)
{
    const auto state = defaultPalette()[GetParam()];
    EXPECT_GE(state.voicing.size(), 2u);
    EXPECT_LE(state.voicing.size(), Voicing::kMaxNotes);
}

TEST_P(HarmonicPaletteTest, hasNoDuplicateExactSemitones)
{
    const auto state = defaultPalette()[GetParam()];
    const auto notes = state.voicing.notes();
    for (size_t i = 0; i < notes.size(); ++i)
    {
        for (size_t j = i + 1; j < notes.size(); ++j)
        {
            EXPECT_NE(notes[i], notes[j]) << "duplicate note in " << state.name;
        }
    }
}

TEST_P(HarmonicPaletteTest, staysWithinAReasonableRegister)
{
    const auto state = defaultPalette()[GetParam()];
    for (const auto note : state.voicing.notes())
    {
        EXPECT_GE(note, -kRegisterBoundSemitones) << state.name;
        EXPECT_LE(note, kRegisterBoundSemitones) << state.name;
    }
}

TEST_P(HarmonicPaletteTest, hasEveryTagInUnitRange)
{
    const auto state = defaultPalette()[GetParam()];
    for (const auto tag : {state.luminosity, state.minorColor, state.density, state.ambiguity, state.tension})
    {
        EXPECT_GE(tag, 0.f) << state.name;
        EXPECT_LE(tag, 1.f) << state.name;
    }
}

INSTANTIATE_TEST_SUITE_P(EveryEntry, HarmonicPaletteTest, ::testing::Range(size_t{0}, kDefaultPaletteSize));

TEST(HarmonicPaletteTest, coversEveryRegionAtLeastOnce)
{
    std::array<bool, 5> seen{};
    for (const auto& state : defaultPalette())
    {
        seen[static_cast<size_t>(state.region)] = true;
    }
    for (const auto region : seen)
    {
        EXPECT_TRUE(region);
    }
}

TEST(TransposedPaletteTest, everyEntryMatchesTheDefaultShiftedByHome)
{
    const auto plain = defaultPalette();
    const auto shifted = transposedPalette(3);
    for (size_t i = 0; i < kDefaultPaletteSize; ++i)
    {
        ASSERT_EQ(shifted[i].voicing.size(), plain[i].voicing.size());
        for (size_t n = 0; n < plain[i].voicing.size(); ++n)
        {
            EXPECT_EQ(shifted[i].voicing.notes()[n], plain[i].voicing.notes()[n] + 3);
        }
    }
}

}
