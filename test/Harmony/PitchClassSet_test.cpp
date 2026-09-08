#include <array>

#include "gtest/gtest.h"

#include "Harmony/PitchClassSet.h"

namespace AbacDsp::Test
{

namespace
{
// Two of the spec's E-centred regions, authored as semitone offsets from home (E = 0):
// Em(add9)  = E G B F#  -> root, m3, 5th, 9th
constexpr auto kEmAdd9 = std::to_array<int>({0, 3, 7, 2});
// Emaj7     = E G# B D# -> root, M3, 5th, maj7
constexpr auto kEmaj7 = std::to_array<int>({0, 4, 7, 11});
}

TEST(PitchClassSetTest, addAndContainsFoldOctaves)
{
    PitchClassSet set{};
    set.add(0);
    set.add(4);
    set.add(7);
    set.add(-5); // folds to pitch class 7 (already present)
    set.add(16); // folds to pitch class 4 (already present)
    EXPECT_TRUE(set.contains(0));
    EXPECT_TRUE(set.contains(4));
    EXPECT_TRUE(set.contains(7));
    EXPECT_TRUE(set.contains(12)); // pitch class 0, an octave up
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(set.size(), 3u);
}

TEST(PitchClassSetTest, fromSemitonesMatchesRepeatedAdd)
{
    const auto viaFactory = PitchClassSet::fromSemitones(kEmAdd9);
    PitchClassSet viaAdd{};
    for (const auto semitone : kEmAdd9)
    {
        viaAdd.add(semitone);
    }
    EXPECT_EQ(viaFactory.size(), viaAdd.size());
    for (int pc = 0; pc < 12; ++pc)
    {
        EXPECT_EQ(viaFactory.contains(pc), viaAdd.contains(pc));
    }
}

TEST(PitchClassSetTest, unionWithCombinesDistinctSets)
{
    const auto a = PitchClassSet::fromSemitones(std::to_array<int>({0, 3}));
    const auto b = PitchClassSet::fromSemitones(std::to_array<int>({7, 10}));
    const auto combined = a.unionWith(b);
    EXPECT_EQ(combined.size(), 4u);
    EXPECT_TRUE(combined.contains(0));
    EXPECT_TRUE(combined.contains(3));
    EXPECT_TRUE(combined.contains(7));
    EXPECT_TRUE(combined.contains(10));
}

TEST(PitchClassSetTest, commonToneCountCountsSharedPitchClasses)
{
    const auto emAdd9 = PitchClassSet::fromSemitones(kEmAdd9);
    const auto emaj7 = PitchClassSet::fromSemitones(kEmaj7);
    // Shared pitch classes: root (0) and 5th (7).
    EXPECT_EQ(emAdd9.commonToneCount(emaj7), 2u);
}

TEST(VoicingTest, notesAndSizeReflectAddedSemitones)
{
    const auto voicing = Voicing::fromSemitones(kEmAdd9);
    ASSERT_EQ(voicing.size(), kEmAdd9.size());
    for (size_t i = 0; i < kEmAdd9.size(); ++i)
    {
        EXPECT_EQ(voicing.notes()[i], kEmAdd9[i]);
    }
}

TEST(VoicingTest, addStopsAtCapacity)
{
    Voicing voicing{};
    for (int i = 0; i < static_cast<int>(Voicing::kMaxNotes) + 3; ++i)
    {
        voicing.add(i);
    }
    EXPECT_EQ(voicing.size(), Voicing::kMaxNotes);
}

TEST(VoicingTest, pitchClassesDerivesSetFromNotes)
{
    const auto voicing = Voicing::fromSemitones(std::to_array<int>({0, 12, 7})); // 0 doubled an octave up
    const auto pitchClasses = voicing.pitchClasses();
    EXPECT_EQ(pitchClasses.size(), 2u); // {0, 7}, the octave duplicate collapses
}

TEST(CommonTonesTest, countsExactSemitoneMatchesOnly)
{
    const auto emAdd9 = Voicing::fromSemitones(kEmAdd9);
    const auto emaj7 = Voicing::fromSemitones(kEmaj7);
    // Root (0) and 5th (7) appear at the same absolute semitone in both; the 9th (2) and the
    // major 7th (11) don't match anything in the other voicing.
    EXPECT_EQ(commonTones(emAdd9, emaj7), 2u);
}

TEST(CommonTonesTest, octaveDuplicateDoesNotCountAsAMatch)
{
    const auto a = Voicing::fromSemitones(std::to_array<int>({0, 12}));
    const auto b = Voicing::fromSemitones(std::to_array<int>({0}));
    EXPECT_EQ(commonTones(a, b), 1u); // only the literal 0 matches; 12 has no partner in b
}

TEST(VoiceLeadingMotionTest, identicalVoicingsHaveZeroMotion)
{
    const auto voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}));
    const auto motion = voiceLeadingMotion(voicing, voicing);
    EXPECT_EQ(motion.matchedCount, 3u);
    EXPECT_EQ(motion.maxMotionSemitones, 0);
    EXPECT_EQ(motion.totalMotionSemitones, 0);
}

TEST(VoiceLeadingMotionTest, singleVoiceMovesBySemitone)
{
    const auto from = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}));
    const auto to = Voicing::fromSemitones(std::to_array<int>({0, 4, 8})); // raise the 5th
    const auto motion = voiceLeadingMotion(from, to);
    EXPECT_EQ(motion.matchedCount, 3u);
    EXPECT_EQ(motion.maxMotionSemitones, 1);
    EXPECT_EQ(motion.totalMotionSemitones, 1);
}

TEST(VoiceLeadingMotionTest, unequalSizeLeavesTheExtraNoteUnmatched)
{
    const auto from = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}));
    const auto to = Voicing::fromSemitones(kEmaj7); // same triad plus an added major 7th
    const auto motion = voiceLeadingMotion(from, to);
    EXPECT_EQ(motion.matchedCount, 3u);
    EXPECT_EQ(motion.maxMotionSemitones, 0);
    EXPECT_EQ(motion.totalMotionSemitones, 0);
}

TEST(VoiceLeadingMotionTest, handWorkedEmAdd9ToEmaj7)
{
    // Worked by hand: root->root and 5th->5th pair first at distance 0, then 9th->maj3rd at
    // distance 1, leaving maj7 unmatched by anything but the remaining note (distance 9).
    const auto from = Voicing::fromSemitones(kEmAdd9);
    const auto to = Voicing::fromSemitones(kEmaj7);
    const auto motion = voiceLeadingMotion(from, to);
    EXPECT_EQ(motion.matchedCount, 4u);
    EXPECT_EQ(motion.maxMotionSemitones, 9);
    EXPECT_EQ(motion.totalMotionSemitones, 10);
}

}
