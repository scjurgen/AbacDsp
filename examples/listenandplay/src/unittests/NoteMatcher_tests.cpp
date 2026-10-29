#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "TestTakes.h"
#include "impl/NoteMatcher.h"

using namespace NoteMatching;
using namespace TestTakes;

TEST(NoteMatcherTest, PerfectTakeMatchesEveryNoteWithoutError)
{
    const auto setup = makeSetup(makePattern());
    const auto played = perfectTake(setup);
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    EXPECT_TRUE(result.missed.empty());
    EXPECT_TRUE(result.extras.empty());
    for (const auto& note : result.matched)
    {
        EXPECT_NEAR(note.timingMs, 0.f, 0.01f);
        EXPECT_TRUE(note.pitchRight);
        EXPECT_NEAR(note.lengthRatio, 1.f, 1e-4f);
    }
}

TEST(NoteMatcherTest, WrongPitchInTimeIsAMatchNotAMissAndAnExtra)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[3].midiNote += 2.f;
    const auto result = matchNotes(setup, played);
    EXPECT_EQ(result.matched.size(), 8u);
    EXPECT_FALSE(result.matched[3].pitchRight);
    EXPECT_NEAR(result.matched[3].pitchError, 2.f, 1e-4f);
    EXPECT_TRUE(result.missed.empty());
    EXPECT_TRUE(result.extras.empty());
}

TEST(NoteMatcherTest, MissingAndExtraNotesAreFound)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played.erase(played.begin() + 2);
    AbacDsp::PlayedNote extra = played[0];
    extra.onsetSeconds += 0.3f * kSecondsPerBeat + 5.f * kSecondsPerBeat;
    extra.offsetSeconds = extra.onsetSeconds + 0.1f;
    played.push_back(extra);
    std::ranges::sort(played, {}, &AbacDsp::PlayedNote::onsetSeconds);
    const auto result = matchNotes(setup, played);
    EXPECT_EQ(result.matched.size(), 7u);
    ASSERT_EQ(result.missed.size(), 1u);
    EXPECT_EQ(result.missed[0], 2u);
    EXPECT_EQ(result.extras.size(), 1u);
}

TEST(NoteMatcherTest, ConstantDelayShowsAsTimingWhenTheLatencyIsNotSetAndSuggestsIt)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.onsetSeconds += 0.08f;
        note.offsetSeconds += 0.08f;
    }
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    for (const auto& note : result.matched)
    {
        EXPECT_NEAR(note.timingMs, 80.f, 0.1f);
    }
    EXPECT_TRUE(result.latencyKnown);
    EXPECT_NEAR(result.suggestedLatencyMs, 80.f, 0.1f);
}

TEST(NoteMatcherTest, LatencySettingRemovesTheConstantDelay)
{
    auto setup = makeSetup(makePattern());
    setup.latencyMs = 80.f;
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.onsetSeconds += 0.08f;
        note.offsetSeconds += 0.08f;
    }
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    for (const auto& note : result.matched)
    {
        EXPECT_NEAR(note.timingMs, 0.f, 0.1f);
        EXPECT_NEAR(note.gridMs, 80.f, 0.1f);
    }
}

TEST(NoteMatcherTest, ClickOffTakesALateStartAsNoError)
{
    const auto setup = makeSetup(makePattern(), false);
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.onsetSeconds += 0.9f;
        note.offsetSeconds += 0.9f;
    }
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    EXPECT_NEAR(result.model.offsetSeconds, 0.9, 0.01);
    for (const auto& note : result.matched)
    {
        EXPECT_NEAR(note.timingMs, 0.f, 5.f);
    }
    EXPECT_FALSE(result.latencyKnown);
}

TEST(NoteMatcherTest, ClickOffTakesAnotherTempoAsNoError)
{
    const auto setup = makeSetup(makePattern(), false);
    auto played = perfectTake(setup);
    constexpr float kSlower{1.15f};
    for (auto& note : played)
    {
        note.onsetSeconds = setup.downbeatSeconds + (note.onsetSeconds - setup.downbeatSeconds) * kSlower;
        note.offsetSeconds = setup.downbeatSeconds + (note.offsetSeconds - setup.downbeatSeconds) * kSlower;
    }
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    EXPECT_NEAR(result.model.scale, 1.15, 0.01);
    for (const auto& note : result.matched)
    {
        EXPECT_NEAR(note.timingMs, 0.f, 10.f);
    }
}

TEST(NoteMatcherTest, ClickOffStillShowsUnevenPlaying)
{
    const auto setup = makeSetup(makePattern(), false);
    auto played = perfectTake(setup);
    played[4].onsetSeconds += 0.06f;
    played[4].offsetSeconds += 0.06f;
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    EXPECT_GT(std::abs(result.matched[4].timingMs), 40.f);
}

TEST(NoteMatcherTest, ClickOnDoesNotForgiveALateStart)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.onsetSeconds += 0.9f;
        note.offsetSeconds += 0.9f;
    }
    const auto result = matchNotes(setup, played);
    EXPECT_LT(result.matched.size(), 8u);
}

TEST(NoteMatcherTest, OctaveShiftIsRemovedWhenAllowed)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.midiNote += 12.f;
    }
    const auto result = matchNotes(setup, played);
    EXPECT_EQ(result.octaveShift, 12);
    EXPECT_TRUE(std::ranges::all_of(result.matched, [](const MatchedNote& n) { return n.pitchRight; }));
}

TEST(NoteMatcherTest, OctaveShiftCountsAsWrongWhenNotAllowed)
{
    auto setup = makeSetup(makePattern());
    setup.pattern.scoring.octaveFree = false;
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.midiNote -= 24.f;
    }
    const auto result = matchNotes(setup, played);
    EXPECT_EQ(result.octaveShift, 0);
    EXPECT_TRUE(std::ranges::none_of(result.matched, [](const MatchedNote& n) { return n.pitchRight; }));
}

TEST(NoteMatcherTest, ATransposedFifthIsNotSnappedToAnOctave)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.midiNote += 7.f;
    }
    const auto result = matchNotes(setup, played);
    EXPECT_EQ(result.octaveShift, 0);
    EXPECT_TRUE(std::ranges::none_of(result.matched, [](const MatchedNote& n) { return n.pitchRight; }));
}

TEST(NoteMatcherTest, UnpitchedNoteIsMatchedButNotRight)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[1].midiNote = 0.f;
    played[1].pitchConfidence = 0.f;
    const auto result = matchNotes(setup, played);
    ASSERT_EQ(result.matched.size(), 8u);
    EXPECT_FALSE(result.matched[1].pitched);
    EXPECT_FALSE(result.matched[1].pitchRight);
}

TEST(NoteMatcherTest, PickupNotesAreMatchedLikeAnyOther)
{
    NotePattern::Pattern pattern{};
    pattern.pickupBeats = 1.f;
    pattern.bpm = kBpm;
    addNote(pattern, -1.f, 0.5f, 55);
    addNote(pattern, -0.5f, 0.5f, 57);
    addNote(pattern, 0.f, 1.f, 60);
    addNote(pattern, 1.f, 1.f, 64);
    const auto setup = makeSetup(pattern);
    const auto result = matchNotes(setup, perfectTake(setup));
    EXPECT_EQ(result.matched.size(), 4u);
    EXPECT_TRUE(result.missed.empty());
}

TEST(NoteMatcherTest, NothingPlayedMissesEverything)
{
    const auto setup = makeSetup(makePattern());
    const auto result = matchNotes(setup, {});
    EXPECT_TRUE(result.matched.empty());
    EXPECT_EQ(result.missed.size(), 8u);
    EXPECT_TRUE(matchNotes(makeSetup(makePattern(), false), {}).matched.empty());
}

TEST(NoteMatcherTest, WithoutAPatternEveryPlayedNoteIsExtra)
{
    NoteMatching::TakeSetup setup;
    const auto played = perfectTake(makeSetup(makePattern()));
    const auto result = matchNotes(setup, played);
    EXPECT_TRUE(result.matched.empty());
    EXPECT_EQ(result.extras.size(), played.size());
}

TEST(NoteMatcherTest, LengthRatioFollowsHowLongTheNoteWasHeld)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[2].offsetSeconds = played[2].onsetSeconds + 0.5f * kSecondsPerBeat;
    const auto result = matchNotes(setup, played);
    EXPECT_NEAR(result.matched[2].lengthRatio, 0.5f, 1e-3f);
}
