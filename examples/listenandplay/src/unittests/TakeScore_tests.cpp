#include <gtest/gtest.h>
#include <vector>

#include "TestTakes.h"
#include "impl/TakeScore.h"

using namespace NoteScoring;
using namespace NoteMatching;
using namespace TestTakes;

namespace
{
TakeScore score(const TakeSetup& setup, const std::vector<AbacDsp::PlayedNote>& played)
{
    return scoreTake(setup, matchNotes(setup, played));
}

void shift(std::vector<AbacDsp::PlayedNote>& played, const size_t index, const float seconds)
{
    played[index].onsetSeconds += seconds;
    played[index].offsetSeconds += seconds;
}
}

TEST(TakeScoreTest, BandCreditIsFullInsideGoodZeroOutsideZeroAndLinearBetween)
{
    EXPECT_FLOAT_EQ(bandCredit(0.f, 60.f, 200.f), 1.f);
    EXPECT_FLOAT_EQ(bandCredit(-60.f, 60.f, 200.f), 1.f);
    EXPECT_FLOAT_EQ(bandCredit(130.f, 60.f, 200.f), 0.5f);
    EXPECT_FLOAT_EQ(bandCredit(-130.f, 60.f, 200.f), 0.5f);
    EXPECT_FLOAT_EQ(bandCredit(200.f, 60.f, 200.f), 0.f);
    EXPECT_FLOAT_EQ(bandCredit(500.f, 60.f, 200.f), 0.f);
}

TEST(TakeScoreTest, EveryFocusWeightsSumToOne)
{
    for (const auto& weights : kWeights)
    {
        EXPECT_NEAR(weights.notes + weights.rhythm + weights.dynamics, 1.f, 1e-6f);
    }
}

TEST(TakeScoreTest, PerfectTakeScoresFullMarksAndPasses)
{
    const auto setup = makeSetup(makePattern());
    const auto result = score(setup, perfectTake(setup));
    EXPECT_FLOAT_EQ(result.notes, 100.f);
    EXPECT_NEAR(result.rhythm, 100.f, 1e-3f);
    EXPECT_NEAR(result.dynamics, 100.f, 1e-3f);
    EXPECT_NEAR(result.overall, 100.f, 1e-3f);
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.expectedCount, 8u);
    EXPECT_EQ(result.matchedCount, 8u);
}

TEST(TakeScoreTest, RhythmFallsAsTheTimingGetsWorseAndTheOtherScoresStay)
{
    const auto setup = makeSetup(makePattern());
    float previous = 101.f;
    for (const float jitter : {0.f, 0.02f, 0.05f, 0.08f, 0.1f, 0.12f})
    {
        auto played = perfectTake(setup);
        for (size_t i = 0; i < played.size(); ++i)
        {
            shift(played, i, i % 2 == 0 ? jitter : -jitter);
        }
        const auto result = score(setup, played);
        EXPECT_LE(result.rhythm, previous) << "jitter " << jitter;
        EXPECT_FLOAT_EQ(result.notes, 100.f);
        EXPECT_NEAR(result.dynamics, 100.f, 1e-3f);
        previous = result.rhythm;
    }
    EXPECT_NEAR(previous, 100.f * (200.f - 120.f) / 140.f, 1.f);
}

TEST(TakeScoreTest, TimingBandsGiveTheDocumentedCredit)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (size_t i = 0; i < played.size(); ++i)
    {
        shift(played, i, i % 2 == 0 ? 0.1f : -0.1f);
    }
    EXPECT_NEAR(score(setup, played).rhythm, 100.f * (200.f - 100.f) / 140.f, 0.5f);
}

TEST(TakeScoreTest, WrongPitchesLowerOnlyTheNotesScore)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[1].midiNote += 3.f;
    played[5].midiNote -= 1.f;
    const auto result = score(setup, played);
    EXPECT_NEAR(result.notes, 75.f, 1e-3f);
    EXPECT_NEAR(result.rhythm, 100.f, 1e-3f);
    EXPECT_NEAR(result.dynamics, 100.f, 1e-3f);
}

TEST(TakeScoreTest, MissedNotesLowerEveryScore)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played.erase(played.begin() + 2);
    played.erase(played.begin() + 4);
    const auto result = score(setup, played);
    EXPECT_EQ(result.missedCount, 2u);
    EXPECT_NEAR(result.notes, 75.f, 1e-3f);
    EXPECT_NEAR(result.rhythm, 75.f, 1e-3f);
    EXPECT_NEAR(result.dynamics, 75.f, 1e-3f);
}

TEST(TakeScoreTest, ExtraNotesLowerNotesAndRhythmButNotDynamics)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    AbacDsp::PlayedNote extra = played[0];
    extra.onsetSeconds = setup.downbeatSeconds + 11.f * kSecondsPerBeat;
    extra.offsetSeconds = extra.onsetSeconds + 0.2f;
    played.push_back(extra);
    const auto result = score(setup, played);
    EXPECT_EQ(result.extraCount, 1u);
    EXPECT_NEAR(result.notes, 100.f * 8.f / 9.f, 1e-3f);
    EXPECT_NEAR(result.rhythm, 100.f * 8.f / 9.f, 1e-3f);
    EXPECT_NEAR(result.dynamics, 100.f, 1e-3f);
}

TEST(TakeScoreTest, DynamicsAreRelativeSoPlayingLouderOrSofterOverallCostsNothing)
{
    const auto setup = makeSetup(makePattern());
    for (const float offset : {-25.f, 10.f})
    {
        auto played = perfectTake(setup);
        for (auto& note : played)
        {
            note.attackRmsDb += offset;
        }
        EXPECT_NEAR(score(setup, played).dynamics, 100.f, 1e-3f) << "offset " << offset;
    }
}

TEST(TakeScoreTest, AFlippedLevelContourScoresLow)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    std::vector<float> levels;
    for (const auto& note : played)
    {
        levels.push_back(note.attackRmsDb);
    }
    for (size_t i = 0; i < played.size(); ++i)
    {
        played[i].attackRmsDb = -levels[i] - 50.f;
    }
    EXPECT_LT(score(setup, played).dynamics, 50.f);
}

TEST(TakeScoreTest, OneNoteSixDecibelsOffGetsHalfCredit)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[3].attackRmsDb += 6.f;
    const auto result = score(setup, played);
    EXPECT_NEAR(result.dynamics, 100.f * (7.f + 0.5f) / 8.f, 0.1f);
    EXPECT_NEAR(result.levelErrorDb[3], 6.f, 1e-3f);
}

TEST(TakeScoreTest, ClickOffForgivesAnotherTempoWhereClickOnDoesNot)
{
    auto stretched = [](const TakeSetup& setup)
    {
        auto played = perfectTake(setup);
        for (auto& note : played)
        {
            note.onsetSeconds = setup.downbeatSeconds + (note.onsetSeconds - setup.downbeatSeconds) * 1.15f;
            note.offsetSeconds = setup.downbeatSeconds + (note.offsetSeconds - setup.downbeatSeconds) * 1.15f;
        }
        return played;
    };
    const auto free = makeSetup(makePattern(), false);
    const auto clicked = makeSetup(makePattern(), true);
    const auto freeScore = score(free, stretched(free));
    EXPECT_GT(freeScore.rhythm, 95.f);
    EXPECT_NEAR(freeScore.tempoRatio, 1.f / 1.15f, 0.02f);
    EXPECT_LT(score(clicked, stretched(clicked)).rhythm, 60.f);
}

TEST(TakeScoreTest, LatencySettingTurnsAConstantDelayIntoFullMarks)
{
    auto delayed = [](const TakeSetup& setup)
    {
        auto played = perfectTake(setup);
        for (size_t i = 0; i < played.size(); ++i)
        {
            shift(played, i, 0.08f);
        }
        return played;
    };
    auto setup = makeSetup(makePattern());
    EXPECT_NEAR(score(setup, delayed(setup)).rhythm, 100.f * (200.f - 80.f) / 140.f, 0.5f);
    setup.latencyMs = 80.f;
    EXPECT_NEAR(score(setup, delayed(setup)).rhythm, 100.f, 1e-3f);
}

TEST(TakeScoreTest, FocusDecidesWhichScoresCountAndPassingFollowsTheOverallScore)
{
    auto pitchSetup = makeSetup(makePattern(NotePattern::Focus::Pitch));
    auto dynamicsSetup = makeSetup(makePattern(NotePattern::Focus::Dynamics));
    pitchSetup.pattern.scoring.passScore = 90.f;
    dynamicsSetup.pattern.scoring.passScore = 90.f;
    auto played = perfectTake(pitchSetup);
    played[0].midiNote += 4.f;
    played[4].midiNote += 4.f;
    const auto pitchScore = score(pitchSetup, played);
    const auto dynamicsScore = score(dynamicsSetup, played);
    EXPECT_NEAR(pitchScore.overall, 0.7f * 75.f + 0.3f * 100.f, 1e-2f);
    EXPECT_NEAR(dynamicsScore.overall, 0.2f * 75.f + 0.2f * 100.f + 0.6f * 100.f, 1e-2f);
    EXPECT_FALSE(pitchScore.passed);
    EXPECT_TRUE(dynamicsScore.passed);
}

TEST(TakeScoreTest, IntonationIsReportedInCentsAndNotScored)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.midiNote += 0.2f;
    }
    const auto result = score(setup, played);
    EXPECT_NEAR(result.meanIntonationCents, 20.f, 0.1f);
    EXPECT_NEAR(result.intonationSpreadCents, 0.f, 0.1f);
    EXPECT_FLOAT_EQ(result.notes, 100.f);
}

TEST(TakeScoreTest, NoteLengthCountsOnlyWhenAsked)
{
    auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.offsetSeconds = note.onsetSeconds + 0.3f * (note.offsetSeconds - note.onsetSeconds);
    }
    EXPECT_NEAR(score(setup, played).rhythm, 100.f, 1e-3f);
    setup.scoreLength = true;
    EXPECT_NEAR(score(setup, played).rhythm, 60.f, 1e-2f);
}

TEST(TakeScoreTest, TimingStatisticsDescribeTheDeviations)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    for (size_t i = 0; i < played.size(); ++i)
    {
        shift(played, i, i % 2 == 0 ? 0.03f : -0.01f);
    }
    const auto result = score(setup, played);
    EXPECT_NEAR(result.meanTimingMs, 10.f, 0.1f);
    EXPECT_NEAR(result.timingSpreadMs, 20.f, 0.1f);
}

TEST(TakeScoreTest, EmptyPatternAndEmptyTakeScoreZeroAndFail)
{
    NoteMatching::TakeSetup empty;
    const auto none = scoreTake(empty, matchNotes(empty, {}));
    EXPECT_FLOAT_EQ(none.overall, 0.f);
    EXPECT_FALSE(none.passed);
    const auto setup = makeSetup(makePattern());
    const auto silent = score(setup, {});
    EXPECT_FLOAT_EQ(silent.overall, 0.f);
    EXPECT_FALSE(silent.passed);
    EXPECT_EQ(silent.missedCount, 8u);
}

TEST(TakeScoreTest, NotesScoreFallsWithEveryWrongNote)
{
    const auto setup = makeSetup(makePattern());
    float previous = 101.f;
    for (size_t wrong = 0; wrong <= 8; ++wrong)
    {
        auto played = perfectTake(setup);
        for (size_t i = 0; i < wrong; ++i)
        {
            played[i].midiNote += 2.f;
        }
        const float notes = score(setup, played).notes;
        EXPECT_LT(notes, previous) << wrong << " wrong";
        previous = notes;
    }
    EXPECT_FLOAT_EQ(previous, 0.f);
}

TEST(TakeScoreTest, EveryScoreFallsWithEveryMissedNote)
{
    const auto setup = makeSetup(makePattern());
    TakeScore previous;
    previous.notes = previous.rhythm = previous.dynamics = previous.overall = 101.f;
    for (size_t missed = 0; missed <= 7; ++missed)
    {
        auto played = perfectTake(setup);
        played.erase(played.begin(), played.begin() + static_cast<std::ptrdiff_t>(missed));
        const auto result = score(setup, played);
        EXPECT_LT(result.notes, previous.notes) << missed << " missed";
        EXPECT_LT(result.rhythm, previous.rhythm) << missed << " missed";
        EXPECT_LT(result.dynamics, previous.dynamics) << missed << " missed";
        EXPECT_LT(result.overall, previous.overall) << missed << " missed";
        previous = result;
    }
}

TEST(TakeScoreTest, DynamicsFallAsTheLevelsDriftApart)
{
    const auto setup = makeSetup(makePattern());
    float previous = 101.f;
    for (const float deviation : {0.f, 2.f, 4.f, 6.f, 8.f, 10.f, 12.f})
    {
        auto played = perfectTake(setup);
        for (const size_t i : {0u, 2u, 4u})
        {
            played[i].attackRmsDb += deviation;
        }
        const float dynamics = score(setup, played).dynamics;
        EXPECT_LE(dynamics, previous) << "deviation " << deviation;
        previous = dynamics;
    }
    EXPECT_NEAR(previous, 100.f * 5.f / 8.f, 1e-3f);
}

TEST(TakeScoreTest, WithTheClickRhythmFallsAsTheTempoDriftsAndWithoutItNever)
{
    float previous = 101.f;
    for (const float factor : {1.f, 1.02f, 1.05f, 1.1f, 1.15f, 1.2f})
    {
        auto stretched = [factor](const TakeSetup& setup)
        {
            auto played = perfectTake(setup);
            for (auto& note : played)
            {
                note.onsetSeconds = setup.downbeatSeconds + (note.onsetSeconds - setup.downbeatSeconds) * factor;
                note.offsetSeconds = setup.downbeatSeconds + (note.offsetSeconds - setup.downbeatSeconds) * factor;
            }
            return played;
        };
        const auto clicked = makeSetup(makePattern(), true);
        const float rhythm = score(clicked, stretched(clicked)).rhythm;
        if (factor <= 1.1f)
        {
            EXPECT_LE(rhythm, previous) << "factor " << factor;
        }
        previous = rhythm;
        const auto free = makeSetup(makePattern(), false);
        EXPECT_GT(score(free, stretched(free)).rhythm, 95.f) << "factor " << factor;
    }
}

TEST(TakeScoreTest, SuggestedLatencyFollowsTheDelayOfTheTake)
{
    const auto setup = makeSetup(makePattern());
    for (const float delayMs : {0.f, 20.f, 60.f, 100.f, 150.f})
    {
        auto played = perfectTake(setup);
        for (size_t i = 0; i < played.size(); ++i)
        {
            shift(played, i, delayMs * 0.001f);
        }
        const auto result = score(setup, played);
        EXPECT_TRUE(result.latencyKnown);
        EXPECT_NEAR(result.suggestedLatencyMs, delayMs, 0.5f) << "delay " << delayMs;
    }
}
