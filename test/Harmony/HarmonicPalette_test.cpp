#include <array>

#include "gtest/gtest.h"

#include "Harmony/HarmonicPalette.h"

namespace AbacDsp::Test
{

namespace
{
constexpr int kRegisterBoundSemitones{30}; // 2.5 octaves either side - the bass-forward "/lo"
                                           // entries sit right at 2 octaves down; leave headroom
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

TEST(TransposeStateTest, preservesEachNotesCentsOffset)
{
    const auto original = makeCustomHarmonicState(PaletteRegion::Home, std::to_array<float>({0.f, 3.5f, 7.f}));
    const auto transposed = transposeState(original, 2);
    ASSERT_EQ(transposed.voicing.size(), original.voicing.size());
    for (size_t i = 0; i < original.voicing.size(); ++i)
    {
        EXPECT_FLOAT_EQ(transposed.voicing.centsValues()[i], original.voicing.centsValues()[i]);
    }
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

TEST(MakeCustomHarmonicStateTest, voicingMatchesTheGivenSemitonesExactly)
{
    const auto semitones = std::to_array<float>({-24.f, 0.f, 4.f, 7.f, 10.f});
    const auto state = makeCustomHarmonicState(PaletteRegion::OpenSuspended, semitones);
    ASSERT_EQ(state.voicing.size(), semitones.size());
    for (size_t i = 0; i < semitones.size(); ++i)
    {
        EXPECT_EQ(state.voicing.notes()[i], static_cast<int>(semitones[i]));
        EXPECT_FLOAT_EQ(state.voicing.centsValues()[i], 0.f);
    }
}

TEST(MakeCustomHarmonicStateTest, setsTheGivenRegion)
{
    const auto state = makeCustomHarmonicState(PaletteRegion::ChromaticWeather, std::to_array<float>({0.f, 3.f, 7.f}));
    EXPECT_EQ(state.region, PaletteRegion::ChromaticWeather);
}

TEST(MakeCustomHarmonicStateTest, everyTagDefaultsToNeutral)
{
    const auto state = makeCustomHarmonicState(PaletteRegion::Home, std::to_array<float>({0.f, 4.f, 7.f}));
    EXPECT_FLOAT_EQ(state.luminosity, 0.5f);
    EXPECT_FLOAT_EQ(state.minorColor, 0.5f);
    EXPECT_FLOAT_EQ(state.density, 0.5f);
    EXPECT_FLOAT_EQ(state.ambiguity, 0.5f);
    EXPECT_FLOAT_EQ(state.tension, 0.5f);
}

TEST(MakeCustomHarmonicStateTest, explicitTagsOverrideTheNeutralDefault)
{
    const auto state = makeCustomHarmonicState(PaletteRegion::Home, std::to_array<float>({0.f, 0.01f, 7.1f}), 0.55f,
                                               0.5f, 0.05f, 0.85f, 0.05f);
    EXPECT_FLOAT_EQ(state.luminosity, 0.55f);
    EXPECT_FLOAT_EQ(state.minorColor, 0.5f);
    EXPECT_FLOAT_EQ(state.density, 0.05f);
    EXPECT_FLOAT_EQ(state.ambiguity, 0.85f);
    EXPECT_FLOAT_EQ(state.tension, 0.05f);
}

TEST(MakeCustomHarmonicStateTest, fractionalSemitonesKeepTheirRoundedIdentityAndCents)
{
    const auto state = makeCustomHarmonicState(PaletteRegion::ChromaticWeather, std::to_array<float>({3.3f, 7.8f}));
    ASSERT_EQ(state.voicing.size(), 2u);
    EXPECT_EQ(state.voicing.notes()[0], 3);
    EXPECT_NEAR(state.voicing.centsValues()[0], 30.f, 1e-3f);
    EXPECT_EQ(state.voicing.notes()[1], 8);
    EXPECT_NEAR(state.voicing.centsValues()[1], -20.f, 1e-3f);
}

TEST(VoicingFromFractionalSemitonesTest, roundsHalfAwayFromZero)
{
    const auto voicing = Voicing::fromFractionalSemitones(std::to_array<float>({0.5f, -0.5f, 1.5f, -1.5f}));
    ASSERT_EQ(voicing.size(), 4u);
    EXPECT_EQ(voicing.notes()[0], 1);
    EXPECT_EQ(voicing.notes()[1], -1);
    EXPECT_EQ(voicing.notes()[2], 2);
    EXPECT_EQ(voicing.notes()[3], -2);
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
