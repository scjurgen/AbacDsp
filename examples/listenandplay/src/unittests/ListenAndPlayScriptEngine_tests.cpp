#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "impl/ListenAndPlayScriptEngine.h"

using namespace NotePattern;

namespace
{
bool loads(ListenAndPlayScriptEngine& engine, const std::string& script)
{
    return engine.loadPattern(script);
}
}

TEST(ListenAndPlayScriptEngineTest, ScriptWithoutPatternCallsLeavesThePatternInactive)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "x = 1\n"));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(ListenAndPlayScriptEngineTest, ClearPatternAloneYieldsAnActiveEmptyPattern)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "ClearPattern()\n"));
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.noteCount, 0u);
    EXPECT_EQ(engine.result().pattern.patternBars, 1u);
}

TEST(ListenAndPlayScriptEngineTest, AddNoteRecordsPositionPitchDurationAndVelocity)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddNote(1.5, 62, 0.5)\nAddNote(2, 64, 1, 42)\n"));
    const auto& pattern = engine.result().pattern;
    ASSERT_EQ(pattern.noteCount, 2u);
    EXPECT_FLOAT_EQ(pattern.notes[0].positionBeats, 1.5f);
    EXPECT_FLOAT_EQ(pattern.notes[0].durationBeats, 0.5f);
    EXPECT_EQ(pattern.notes[0].pitch, 62);
    EXPECT_EQ(pattern.notes[0].velocity, kDefaultVelocity);
    EXPECT_EQ(pattern.notes[1].pitch, 64);
    EXPECT_EQ(pattern.notes[1].velocity, 42);
}

TEST(ListenAndPlayScriptEngineTest, NotesAreKeptSortedByPosition)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddNote(2, 64, 1)\nAddNote(0, 60, 1)\nAddNote(1, 62, 1)\n"));
    const auto& pattern = engine.result().pattern;
    ASSERT_EQ(pattern.noteCount, 3u);
    EXPECT_EQ(pattern.notes[0].pitch, 60);
    EXPECT_EQ(pattern.notes[1].pitch, 62);
    EXPECT_EQ(pattern.notes[2].pitch, 64);
}

TEST(ListenAndPlayScriptEngineTest, PickupAllowsNotesBeforeTheDownbeat)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetPickup(1)\nAddNote(-1, 55, 1)\nAddNote(0, 60, 2)\n"));
    const auto& pattern = engine.result().pattern;
    EXPECT_FLOAT_EQ(pattern.pickupBeats, 1.f);
    ASSERT_EQ(pattern.noteCount, 2u);
    EXPECT_FLOAT_EQ(pattern.notes[0].positionBeats, -1.f);
}

TEST(ListenAndPlayScriptEngineTest, NegativePositionWithoutPickupFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddNote(-0.5, 60, 0.5)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the phrase"));
}

TEST(ListenAndPlayScriptEngineTest, PositionAtOrBeyondThePhraseEndFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddNote(4, 60, 1)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the phrase"));
}

TEST(ListenAndPlayScriptEngineTest, NoteRunningPastThePhraseEndFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddNote(3, 60, 2)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("end inside the phrase"));
}

TEST(ListenAndPlayScriptEngineTest, NonPositiveDurationFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddNote(0, 60, 0)\n"));
    EXPECT_FALSE(loads(engine, "AddNote(0, 60, -1)\n"));
}

TEST(ListenAndPlayScriptEngineTest, OverlappingNotesFailBothWays)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddNote(0, 60, 2)\nAddNote(1, 62, 1)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("overlaps"));
    EXPECT_FALSE(loads(engine, "AddNote(1, 62, 1)\nAddNote(0, 60, 2)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("overlaps"));
}

TEST(ListenAndPlayScriptEngineTest, NotesMayTouchEndToEnd)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddNote(0, 60, 1)\nAddNote(1, 62, 1)\nAddNote(2, 64, 2)\n"));
    EXPECT_EQ(engine.result().pattern.noteCount, 3u);
}

TEST(ListenAndPlayScriptEngineTest, InvalidPitchOrVelocityFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddNote(0, 128, 1)\n"));
    EXPECT_FALSE(loads(engine, "AddNote(0, -1, 1)\n"));
    EXPECT_FALSE(loads(engine, "AddNote(0, 60.5, 1)\n"));
    EXPECT_FALSE(loads(engine, "AddNote(0, 60, 1, 0)\n"));
    EXPECT_FALSE(loads(engine, "AddNote(0, 60, 1, 128)\n"));
}

TEST(ListenAndPlayScriptEngineTest, PhraseHoldsAtMostMaxNotes)
{
    ListenAndPlayScriptEngine engine;
    const std::string fill = "SetBeatsPerBar(16)\nSetPatternBars(8)\nfor i = 0, " + std::to_string(kMaxNotes) +
                             " do AddNote(i, 60, 1) end\n";
    EXPECT_FALSE(loads(engine, fill));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("at most"));
}

TEST(ListenAndPlayScriptEngineTest, BarAndPhraseLengthSettersValidateTheirRange)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetBeatsPerBar(0)\n"));
    EXPECT_FALSE(loads(engine, "SetBeatsPerBar(17)\n"));
    EXPECT_FALSE(loads(engine, "SetBeatsPerBar(3.5)\n"));
    EXPECT_FALSE(loads(engine, "SetPatternBars(9)\n"));
    ASSERT_TRUE(loads(engine, "SetBeatsPerBar(3)\nSetPatternBars(2)\n"));
    EXPECT_EQ(engine.result().pattern.beatsPerBar, 3u);
    EXPECT_EQ(engine.result().pattern.patternBars, 2u);
}

TEST(ListenAndPlayScriptEngineTest, ShorteningThePhraseBelowExistingNotesFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetPatternBars(2)\nAddNote(5, 60, 1)\nSetPatternBars(1)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the new phrase length"));
}

TEST(ListenAndPlayScriptEngineTest, ShrinkingThePickupBelowExistingNotesFails)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetPickup(2)\nAddNote(-2, 60, 1)\nSetPickup(1)\n"));
}

TEST(ListenAndPlayScriptEngineTest, PickupMustBeInRange)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetPickup(-1)\n"));
    EXPECT_FALSE(loads(engine, "SetPickup(9)\n"));
    EXPECT_TRUE(loads(engine, "SetPickup(0.5)\n"));
}

TEST(ListenAndPlayScriptEngineTest, TempoFocusAndClickDefaultAreStored)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetTempo(72)\nSetFocus(FocusRhythm)\nSetClickDefault(false)\n"));
    const auto& pattern = engine.result().pattern;
    EXPECT_FLOAT_EQ(pattern.bpm, 72.f);
    EXPECT_EQ(pattern.focus, Focus::Rhythm);
    EXPECT_FALSE(pattern.clickDefault);
    EXPECT_TRUE(pattern.active);
}

TEST(ListenAndPlayScriptEngineTest, TempoAndFocusRejectOutOfRangeValues)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetTempo(10)\n"));
    EXPECT_FALSE(loads(engine, "SetTempo(500)\n"));
    EXPECT_FALSE(loads(engine, "SetFocus(9)\n"));
}

TEST(ListenAndPlayScriptEngineTest, EveryFocusConstantIsDefined)
{
    ListenAndPlayScriptEngine engine;
    for (size_t i = 0; i < kFocusNames.size(); ++i)
    {
        ASSERT_TRUE(loads(engine, "SetFocus(" + std::string{kFocusNames[i]} + ")\n"));
        EXPECT_EQ(static_cast<size_t>(engine.result().pattern.focus), i);
    }
}

TEST(ListenAndPlayScriptEngineTest, SetNameStoresAndValidatesTheName)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetName('Up the triad')\n"));
    EXPECT_EQ(engine.result().name, "Up the triad");
    EXPECT_FALSE(loads(engine, "SetName('')\n"));
    EXPECT_FALSE(loads(engine, "SetName('a\\nb')\n"));
    EXPECT_FALSE(loads(engine, "SetName(string.rep('x', 65))\n"));
}

TEST(ListenAndPlayScriptEngineTest, RequestGlobalsDescribeTheRequest)
{
    ListenAndPlayScriptEngine engine;
    engine.setRange(40, 70);
    ASSERT_TRUE(engine.loadPattern("AddNote(0, RangeLow, 0.5)\nAddNote(0.5, RangeHigh, 0.5)\n"
                                   "AddNote(1, BarBeats + PatternBars, 0.5)\nAddNote(1.5, Seed, 0.5)\n",
                                   3, 7));
    const auto& pattern = engine.result().pattern;
    EXPECT_EQ(pattern.notes[0].pitch, 40);
    EXPECT_EQ(pattern.notes[1].pitch, 70);
    EXPECT_EQ(pattern.notes[2].pitch, 3 + 1);
    EXPECT_EQ(pattern.notes[3].pitch, 7);
}

TEST(ListenAndPlayScriptEngineTest, ShortBarsLimitTheNotePositions)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddNote(3, 60, 0.5)\n", 3, 1));
    EXPECT_TRUE(engine.loadPattern("AddNote(2, 60, 1)\n", 3, 1));
}

TEST(ListenAndPlayScriptEngineTest, SameSeedGivesTheSamePhraseAndSeedsDiffer)
{
    const std::string script = "for i = 0, 3 do AddNote(i, math.random(40, 90), 1) end\n";
    const auto pitchesFor = [&](const long long seed)
    {
        ListenAndPlayScriptEngine engine;
        EXPECT_TRUE(engine.loadPattern(script, 4, seed));
        std::vector<int> pitches;
        for (size_t i = 0; i < engine.result().pattern.noteCount; ++i)
        {
            pitches.push_back(engine.result().pattern.notes[i].pitch);
        }
        return pitches;
    };
    EXPECT_EQ(pitchesFor(5), pitchesFor(5));
    EXPECT_NE(pitchesFor(5), pitchesFor(6));
}

TEST(ListenAndPlayScriptEngineTest, FailedLoadLeavesAnEmptyResultAndAnError)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetName('ok')\nAddNote(0, 60, 1)\n"));
    EXPECT_FALSE(loads(engine, "SetName('bad')\nAddNote(99, 60, 1)\n"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_TRUE(engine.result().name.empty());
}

TEST(ListenAndPlayScriptEngineTest, ImportedLibraryIsSplicedIn)
{
    ListenAndPlayScriptEngine engine;
    engine.setImportResolver(
        [](const std::string_view name) -> ImportLookup
        {
            if (name == "tiny")
            {
                return {std::string{"function Hello() AddNote(0, 61, 1) end"}, {}};
            }
            return {};
        });
    ASSERT_TRUE(loads(engine, "import \"tiny\"\nHello()\n"));
    EXPECT_EQ(engine.result().pattern.notes[0].pitch, 61);
    EXPECT_FALSE(loads(engine, "import \"missing\"\n"));
}

TEST(ListenAndPlayScriptEngineTest, ScoringDefaultsApplyWhenTheScriptSetsNothing)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "ClearPattern()\n"));
    const auto& scoring = engine.result().pattern.scoring;
    EXPECT_FLOAT_EQ(scoring.timingGoodMs, 60.f);
    EXPECT_FLOAT_EQ(scoring.timingZeroMs, 200.f);
    EXPECT_FLOAT_EQ(scoring.dynamicsGoodDb, 3.f);
    EXPECT_FLOAT_EQ(scoring.dynamicsZeroDb, 9.f);
    EXPECT_FLOAT_EQ(scoring.passScore, 70.f);
    EXPECT_TRUE(scoring.octaveFree);
}

TEST(ListenAndPlayScriptEngineTest, ScoringSettersStoreTheirValues)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetTimingTolerance(45, 150)\nSetDynamicsTolerance(2.5, 7)\nSetPassScore(85)\n"
                              "SetOctaveFree(false)\n"));
    const auto& scoring = engine.result().pattern.scoring;
    EXPECT_FLOAT_EQ(scoring.timingGoodMs, 45.f);
    EXPECT_FLOAT_EQ(scoring.timingZeroMs, 150.f);
    EXPECT_FLOAT_EQ(scoring.dynamicsGoodDb, 2.5f);
    EXPECT_FLOAT_EQ(scoring.dynamicsZeroDb, 7.f);
    EXPECT_FLOAT_EQ(scoring.passScore, 85.f);
    EXPECT_FALSE(scoring.octaveFree);
    EXPECT_TRUE(engine.result().pattern.active);
}

TEST(ListenAndPlayScriptEngineTest, ScoringSettersRejectInconsistentValues)
{
    ListenAndPlayScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetTimingTolerance(100, 100)\n"));
    EXPECT_FALSE(loads(engine, "SetTimingTolerance(-1, 100)\n"));
    EXPECT_FALSE(loads(engine, "SetTimingTolerance(50, 5000)\n"));
    EXPECT_FALSE(loads(engine, "SetDynamicsTolerance(9, 3)\n"));
    EXPECT_FALSE(loads(engine, "SetDynamicsTolerance(3, 99)\n"));
    EXPECT_FALSE(loads(engine, "SetPassScore(0)\n"));
    EXPECT_FALSE(loads(engine, "SetPassScore(101)\n"));
}

TEST(ListenAndPlayScriptEngineTest, TheKeyNoteIsStoredAndValidated)
{
    ListenAndPlayScriptEngine engine;
    ASSERT_TRUE(loads(engine, "ClearPattern()\n"));
    EXPECT_EQ(engine.result().pattern.tonic, 0);
    ASSERT_TRUE(loads(engine, "SetTonic(62)\n"));
    EXPECT_EQ(engine.result().pattern.tonic, 62);
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_FALSE(loads(engine, "SetTonic(128)\n"));
    EXPECT_FALSE(loads(engine, "SetTonic(-1)\n"));
    EXPECT_FALSE(loads(engine, "SetTonic(60.5)\n"));
}
