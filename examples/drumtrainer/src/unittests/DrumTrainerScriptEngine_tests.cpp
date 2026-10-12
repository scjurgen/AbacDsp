#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "impl/DrumTrainerScriptEngine.h"

using namespace DrumPattern;

namespace
{
constexpr size_t kBarBeats{4};

float dbToLinear(const float db)
{
    return std::pow(10.f, db / 20.f);
}
}

TEST(DrumTrainerScriptEngineTest, ScriptWithoutPatternCallsLeavesThePatternInactive)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("x = 1\n", kBarBeats));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(DrumTrainerScriptEngineTest, ClearPatternAloneYieldsActiveSilentPattern)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("ClearPattern()\n", kBarBeats));
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(DrumTrainerScriptEngineTest, AddInstrumentRecordsPositionInstrumentAndLinearGain)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddInstrument(1.5, Snare, -6)\n", kBarBeats));
    const auto& pattern = engine.result().pattern;
    ASSERT_TRUE(pattern.active);
    ASSERT_EQ(pattern.hitCount, 1u);
    EXPECT_FLOAT_EQ(pattern.hits[0].positionBeats, 1.5f);
    EXPECT_EQ(pattern.hits[0].instrument, Instrument::Snare);
    EXPECT_NEAR(pattern.hits[0].gain, dbToLinear(-6.f), 1e-6f);
}

TEST(DrumTrainerScriptEngineTest, HitsAreKeptSortedAndStackedHitsKeepCallOrder)
{
    DrumTrainerScriptEngine engine;
    const std::string script = "AddInstrument(2, Snare, 0)\n"
                               "AddInstrument(0, Kick, 0)\n"
                               "AddInstrument(2, Hihat, 0)\n"
                               "AddInstrument(1, Wood, 0)\n";
    ASSERT_TRUE(engine.loadPattern(script, kBarBeats));
    const auto& pattern = engine.result().pattern;
    ASSERT_EQ(pattern.hitCount, 4u);
    EXPECT_EQ(pattern.hits[0].instrument, Instrument::Kick);
    EXPECT_EQ(pattern.hits[1].instrument, Instrument::Wood);
    EXPECT_EQ(pattern.hits[2].instrument, Instrument::Snare);
    EXPECT_EQ(pattern.hits[3].instrument, Instrument::Hihat);
}

TEST(DrumTrainerScriptEngineTest, LevelIsClampedToTheSupportedRange)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddInstrument(0, Kick, -500)\nAddInstrument(1, Kick, 40)\n", kBarBeats));
    const auto& pattern = engine.result().pattern;
    EXPECT_NEAR(pattern.hits[0].gain, dbToLinear(kMinLevelDb), 1e-7f);
    EXPECT_NEAR(pattern.hits[1].gain, dbToLinear(kMaxLevelDb), 1e-4f);
}

TEST(DrumTrainerScriptEngineTest, ClearPatternDiscardsEarlierHits)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(
        engine.loadPattern("AddInstrument(0, Kick, 0)\nClearPattern()\nAddInstrument(1, Snare, 0)\n", kBarBeats));
    ASSERT_EQ(engine.result().pattern.hitCount, 1u);
    EXPECT_EQ(engine.result().pattern.hits[0].instrument, Instrument::Snare);
}

TEST(DrumTrainerScriptEngineTest, BarBeatsGlobalReflectsTheRequestedBarLength)
{
    DrumTrainerScriptEngine engine;
    const std::string script = "for beat = 0, BarBeats - 1 do AddInstrument(beat, Kick, 0) end\n";
    ASSERT_TRUE(engine.loadPattern(script, 7));
    EXPECT_EQ(engine.result().pattern.hitCount, 7u);
    ASSERT_TRUE(engine.loadPattern(script, 3));
    EXPECT_EQ(engine.result().pattern.hitCount, 3u);
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarRecordsTheLengthAndActivatesASilentPattern)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("SetBeatsPerBar(7)\n", kBarBeats));
    EXPECT_EQ(engine.result().pattern.beatsPerBar, 7u);
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarUpdatesTheBarBeatsGlobal)
{
    DrumTrainerScriptEngine engine;
    const std::string script = "SetBeatsPerBar(5)\nfor beat = 0, BarBeats - 1 do AddInstrument(beat, Kick, 0) end\n";
    ASSERT_TRUE(engine.loadPattern(script, kBarBeats));
    EXPECT_EQ(engine.result().pattern.hitCount, 5u);
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarWidensTheAllowedPositions)
{
    DrumTrainerScriptEngine engine;
    EXPECT_TRUE(engine.loadPattern("SetBeatsPerBar(7)\nAddInstrument(6.5, Kick, 0)\n", kBarBeats));
    EXPECT_FALSE(engine.loadPattern("SetBeatsPerBar(2)\nAddInstrument(2, Kick, 0)\n", kBarBeats));
}

TEST(DrumTrainerScriptEngineTest, ClearPatternKeepsTheBarLengthSetBefore)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("SetBeatsPerBar(6)\nClearPattern()\n", kBarBeats));
    EXPECT_EQ(engine.result().pattern.beatsPerBar, 6u);
}

TEST(DrumTrainerScriptEngineTest, ScriptWithoutSetBeatsPerBarLeavesTheBarLengthToThePreset)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddInstrument(0, Kick, 0)\n", kBarBeats));
    EXPECT_EQ(engine.result().pattern.beatsPerBar, 0u);
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarRejectsLengthsOutsideOneToSixteen)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("SetBeatsPerBar(0)\n", kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("whole number from 1 to 16"));
    EXPECT_FALSE(engine.loadPattern("SetBeatsPerBar(17)\n", kBarBeats));
    EXPECT_TRUE(engine.loadPattern("SetBeatsPerBar(16)\n", kBarBeats));
    EXPECT_TRUE(engine.loadPattern("SetBeatsPerBar(1)\n", kBarBeats));
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarRejectsFractionalLengths)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("SetBeatsPerBar(3.5)\n", kBarBeats));
    EXPECT_FALSE(engine.loadPattern("SetBeatsPerBar(0/0)\n", kBarBeats));
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarRejectsAShorterBarThanThePositionsAlreadyAdded)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("SetBeatsPerBar(8)\nAddInstrument(6, Kick, 0)\nSetBeatsPerBar(4)\n", kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("already added"));
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarCalledFromATimerIsRejected)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("Timer.After(1, function() SetBeatsPerBar(5) end)\n", kBarBeats));
    engine.tickBlock(100000);
    EXPECT_EQ(engine.result().pattern.beatsPerBar, 0u);
}

TEST(DrumTrainerScriptEngineTest, PositionOutsideTheBarRaisesAScriptError)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(4, Kick, 0)\n", kBarBeats));
    EXPECT_TRUE(engine.hasError());
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the bar"));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(DrumTrainerScriptEngineTest, UnknownInstrumentRaisesAScriptError)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(0, 999, 0)\n", kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("unknown instrument"));
}

TEST(DrumTrainerScriptEngineTest, UndefinedInstrumentNameRaisesAScriptError)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(0, Cowbell, 0)\n", kBarBeats));
    EXPECT_TRUE(engine.hasError());
}

TEST(DrumTrainerScriptEngineTest, PatternCapacityIsEnforced)
{
    DrumTrainerScriptEngine engine;
    const std::string script = "for i = 1, " + std::to_string(kMaxHits + 1) + " do AddInstrument(0, Kick, 0) end\n";
    EXPECT_FALSE(engine.loadPattern(script, kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("at most"));
}

TEST(DrumTrainerScriptEngineTest, SyntaxErrorFailsTheLoadAndEmptiesTheResult)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(0, Kick, 0)\nthis is not lua\n", kBarBeats));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(DrumTrainerScriptEngineTest, ASuccessfulLoadClearsThePreviousError)
{
    DrumTrainerScriptEngine engine;
    ASSERT_FALSE(engine.loadPattern("AddInstrument(9, Kick, 0)\n", kBarBeats));
    ASSERT_TRUE(engine.loadPattern("AddInstrument(1, Kick, 0)\n", kBarBeats));
    EXPECT_FALSE(engine.hasError());
}

TEST(DrumTrainerScriptEngineTest, PatternFunctionsCalledFromATimerAreRejected)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("Timer.After(1, function() AddInstrument(0, Kick, 0) end)\n", kBarBeats));
    engine.tickBlock(100000);
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(DrumTrainerScriptEngineTest, StubAndSkeletonScriptsLoadWithoutDefiningAPattern)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern(std::string{DrumTrainerScriptEngine::kStubScript}, kBarBeats));
    EXPECT_FALSE(engine.result().pattern.active);
    ASSERT_TRUE(engine.loadPattern(std::string{DrumTrainerScriptEngine::kSkeletonScript}, kBarBeats));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(DrumTrainerScriptEngineTest, SetNameStoresTheNameAndLeavesThePatternInactive)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("SetName('Bossa Nova')\n", kBarBeats));
    EXPECT_EQ(engine.result().name, "Bossa Nova");
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(DrumTrainerScriptEngineTest, EachRunStartsWithAnEmptyName)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("SetName('Rock')\n", kBarBeats));
    ASSERT_TRUE(engine.loadPattern("x = 1\n", kBarBeats));
    EXPECT_TRUE(engine.result().name.empty());
}

TEST(DrumTrainerScriptEngineTest, LastSetNameCallWins)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("SetName('A')\nSetName('B')\n", kBarBeats));
    EXPECT_EQ(engine.result().name, "B");
}

TEST(DrumTrainerScriptEngineTest, SetNameRejectsEmptyOverlongAndControlCharacterNames)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("SetName('')\n", kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("1 to 64 bytes"));
    EXPECT_FALSE(engine.loadPattern("SetName(string.rep('x', 65))\n", kBarBeats));
    EXPECT_FALSE(engine.loadPattern("SetName('a\\nb')\n", kBarBeats));
    EXPECT_TRUE(engine.loadPattern("SetName(string.rep('x', 64))\n", kBarBeats));
}

TEST(DrumTrainerScriptEngineTest, FailedLoadLeavesNoName)
{
    DrumTrainerScriptEngine engine;
    ASSERT_FALSE(engine.loadPattern("SetName('Rock')\nthis is not lua\n", kBarBeats));
    EXPECT_TRUE(engine.result().name.empty());
}
