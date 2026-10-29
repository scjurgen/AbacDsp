#include <gtest/gtest.h>

#include "TestTakes.h"
#include "impl/SnapshotBuilders.h"

using namespace TestTakes;
using namespace ListenPlayDisplay;

namespace
{
TakeAnalysis::TakeResult resultOf(const NoteMatching::TakeSetup& setup, std::vector<AbacDsp::PlayedNote> played)
{
    return TakeAnalysis::scoreTake(setup, std::move(played), "Lesson", 1, "a01-x");
}
}

TEST(SnapshotBuildersTest, EveryTakeStateHasItsOwnPhase)
{
    using State = TakeSession::State;
    EXPECT_EQ(phaseOf(State::Idle), Phase::Idle);
    EXPECT_EQ(phaseOf(State::Listening), Phase::Listening);
    EXPECT_EQ(phaseOf(State::CountIn), Phase::CountIn);
    EXPECT_EQ(phaseOf(State::Capturing), Phase::Capturing);
    EXPECT_EQ(phaseOf(State::Analysing), Phase::Analysing);
    EXPECT_EQ(phaseOf(State::Review), Phase::Review);
    EXPECT_EQ(phaseOf(State::Replaying), Phase::Replaying);
}

TEST(SnapshotBuildersTest, PhrasesAreTheSameWhenEveryNoteIs)
{
    const auto pattern = makePattern();
    EXPECT_TRUE(samePhrase(pattern, pattern));
    auto other = pattern;
    other.notes[3].pitch += 1;
    EXPECT_FALSE(samePhrase(pattern, other));
    other = pattern;
    other.notes[5].positionBeats += 0.25f;
    EXPECT_FALSE(samePhrase(pattern, other));
    other = pattern;
    other.pickupBeats = 1.f;
    EXPECT_FALSE(samePhrase(pattern, other));
    other = pattern;
    other.noteCount = 7;
    EXPECT_FALSE(samePhrase(pattern, other));
}

TEST(SnapshotBuildersTest, TheRollOfAPhraseHoldsItsNotesAndGrid)
{
    auto pattern = makePattern();
    pattern.pickupBeats = 1.f;
    const auto roll = rollOfPhrase(pattern);
    EXPECT_EQ(roll.targetCount, 8u);
    EXPECT_FLOAT_EQ(roll.pickupBeats, 1.f);
    EXPECT_FLOAT_EQ(roll.patternBeats, 8.f);
    EXPECT_EQ(roll.beatsPerBar, 4u);
    EXPECT_EQ(roll.target[6].pitch, 64);
    EXPECT_FLOAT_EQ(roll.target[6].startBeats, 6.5f);
    EXPECT_FLOAT_EQ(roll.target[6].lengthBeats, 0.5f);
    EXPECT_EQ(roll.playedCount, 0u);
    EXPECT_FALSE(roll.revealed);
}

TEST(SnapshotBuildersTest, APerfectTakeLiesOnTheTargetNotes)
{
    const auto setup = makeSetup(makePattern());
    const auto result = resultOf(setup, perfectTake(setup));
    auto roll = rollOfPhrase(setup.pattern);
    addPlayedNotes(roll, result);
    ASSERT_EQ(roll.playedCount, 8u);
    EXPECT_TRUE(roll.revealed);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_NEAR(roll.played[i].startBeats, roll.target[i].startBeats, 1e-3f) << i;
        EXPECT_NEAR(roll.played[i].lengthBeats, roll.target[i].lengthBeats, 1e-3f) << i;
        EXPECT_NEAR(roll.played[i].midi, static_cast<float>(roll.target[i].pitch), 1e-3f) << i;
        EXPECT_GT(roll.played[i].credit, 0.99f);
        EXPECT_TRUE(roll.played[i].pitchRight);
        EXPECT_FALSE(roll.played[i].extra);
        EXPECT_FALSE(roll.target[i].missed);
    }
}

TEST(SnapshotBuildersTest, MissedNotesAreMarkedAndExtraNotesAreAdded)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played.erase(played.begin() + 2);
    AbacDsp::PlayedNote extra = played[0];
    extra.onsetSeconds = setup.downbeatSeconds + 11.f * kSecondsPerBeat;
    extra.offsetSeconds = extra.onsetSeconds + 0.3f;
    played.push_back(extra);
    const auto result = resultOf(setup, played);
    auto roll = rollOfPhrase(setup.pattern);
    addPlayedNotes(roll, result);
    EXPECT_TRUE(roll.target[2].missed);
    EXPECT_FALSE(roll.target[3].missed);
    ASSERT_EQ(roll.playedCount, 8u);
    EXPECT_TRUE(roll.played[7].extra);
    EXPECT_NEAR(roll.played[7].startBeats, 11.f, 1e-3f);
    EXPECT_FLOAT_EQ(roll.played[7].credit, 0.f);
}

TEST(SnapshotBuildersTest, WrongAndLateNotesGetLessCredit)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[1].midiNote += 2.f;
    played[4].onsetSeconds += 0.13f;
    played[4].offsetSeconds += 0.13f;
    const auto result = resultOf(setup, played);
    auto roll = rollOfPhrase(setup.pattern);
    addPlayedNotes(roll, result);
    EXPECT_FALSE(roll.played[1].pitchRight);
    EXPECT_FLOAT_EQ(roll.played[1].credit, 0.f);
    EXPECT_NEAR(roll.played[1].midi, 64.f, 1e-3f);
    EXPECT_TRUE(roll.played[4].pitchRight);
    EXPECT_LT(roll.played[4].credit, 0.6f);
    EXPECT_GT(roll.played[4].credit, 0.4f);
}

TEST(SnapshotBuildersTest, ALateStartAndAnOctaveShiftAreTakenOutWithoutTheClick)
{
    const auto setup = makeSetup(makePattern(), false);
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.onsetSeconds += 0.9f;
        note.offsetSeconds += 0.9f;
        note.midiNote += 12.f;
    }
    const auto result = resultOf(setup, played);
    auto roll = rollOfPhrase(setup.pattern);
    addPlayedNotes(roll, result);
    ASSERT_EQ(roll.playedCount, 8u);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_NEAR(roll.played[i].startBeats, roll.target[i].startBeats, 0.05f) << i;
        EXPECT_NEAR(roll.played[i].midi, static_cast<float>(roll.target[i].pitch), 1e-3f) << i;
    }
}

TEST(SnapshotBuildersTest, TheScoreSnapshotCarriesTheScoresAndThePassMark)
{
    auto setup = makeSetup(makePattern());
    setup.pattern.scoring.passScore = 88.f;
    auto played = perfectTake(setup);
    played[0].midiNote += 3.f;
    const auto result = resultOf(setup, played);
    const auto score = scoreOfResult(result);
    EXPECT_TRUE(score.hasResult);
    EXPECT_FLOAT_EQ(score.notes, 87.5f);
    EXPECT_FLOAT_EQ(score.passScore, 88.f);
    EXPECT_EQ(score.expected, 8u);
    EXPECT_EQ(score.matched, 8u);
    EXPECT_EQ(score.missed, 0u);
    EXPECT_TRUE(score.clickOn);
    EXPECT_EQ(score.passed, result.score.passed);
}
