#include <algorithm>

#include "gtest/gtest.h"

#include "Harmony/ChordName.h"
#include "Harmony/HarmonicPalette.h"

namespace AbacDsp::Test
{

namespace
{
[[nodiscard]] const HarmonicState& findState(const std::array<HarmonicState, kDefaultPaletteSize>& palette,
                                             const std::string_view name)
{
    const auto it =
        std::find_if(palette.begin(), palette.end(), [name](const auto& state) { return state.name == name; });
    return *it;
}
}

TEST(NoteNameTest, mapsPitchClassesToSharpsOnlyNames)
{
    EXPECT_EQ(noteName(0), "C");
    EXPECT_EQ(noteName(4), "E");
    EXPECT_EQ(noteName(11), "B");
}

TEST(NoteNameTest, foldsOutOfRangePitchClasses)
{
    EXPECT_EQ(noteName(-1), "B");
    EXPECT_EQ(noteName(12), "C");
    EXPECT_EQ(noteName(13), "C#");
}

TEST(NameChordTest, emptyVoicingReturnsEmptyLabel)
{
    EXPECT_TRUE(nameChord(Voicing{}).empty());
}

TEST(NameChordTest, plainMaj7RootInTheBassNeedsNoSlash)
{
    const auto palette = transposedPalette(0);
    EXPECT_EQ(nameChord(findState(palette, "1maj7").voicing), "Cmaj7");
}

TEST(NameChordTest, sameEntryTransposedToADifferentHome)
{
    const auto palette = transposedPalette(4);
    EXPECT_EQ(nameChord(findState(palette, "1maj7").voicing), "Emaj7");
}

TEST(NameChordTest, lowestNoteBelowTheRootProducesASlashChord)
{
    const auto palette = transposedPalette(0);
    EXPECT_EQ(nameChord(findState(palette, "1m9").voicing), "Cm9/A#");
}

TEST(NameChordTest, twoOctaveDownRootIsNotMistakenForAnInversion)
{
    const auto palette = transposedPalette(0);
    EXPECT_EQ(nameChord(findState(palette, "1m9/lo").voicing), "Cm9");
}

}
