#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "impl/DrumTrainerScriptEngine.h"

using namespace DrumPattern;

namespace
{
constexpr size_t kBarBeats{4};

bool loads(DrumTrainerScriptEngine& engine, const std::string& script)
{
    return engine.loadPattern(script, kBarBeats);
}
}

TEST(DrumTrainerScriptEngineTest, ScriptWithoutPatternCallsLeavesThePatternInactive)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "x = 1\n"));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(DrumTrainerScriptEngineTest, ClearPatternAloneYieldsAnActiveEmptyPattern)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "ClearPattern()\n"));
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
    EXPECT_EQ(engine.result().pattern.patternBars, 1u);
}

TEST(DrumTrainerScriptEngineTest, AddHitRecordsPositionInstrumentAndVelocity)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddHit(1.5, Snare)\nAddHit(2, TomFloor, 42)\n"));
    const auto& pattern = engine.result().pattern;
    ASSERT_TRUE(pattern.active);
    ASSERT_EQ(pattern.hitCount, 2u);
    EXPECT_FLOAT_EQ(pattern.hits[0].positionBeats, 1.5f);
    EXPECT_EQ(pattern.hits[0].instrument, Instrument::Snare);
    EXPECT_EQ(pattern.hits[0].velocity, kDefaultVelocity);
    EXPECT_EQ(pattern.hits[1].instrument, Instrument::TomFloor);
    EXPECT_EQ(pattern.hits[1].velocity, 42);
}

TEST(DrumTrainerScriptEngineTest, EveryInstrumentConstantIsDefined)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddHit(0, Kick)\nAddHit(0, Snare)\nAddHit(0, HihatClosed)\nAddHit(1, HihatOpen)\n"
                              "AddHit(0, Crash)\nAddHit(0, TomHigh)\nAddHit(0, TomLow)\nAddHit(0, TomFloor)\n"));
    EXPECT_EQ(engine.result().pattern.hitCount, 8u);
}

TEST(DrumTrainerScriptEngineTest, HitsAreKeptSortedAndSamePositionHitsKeepCallOrder)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddHit(3, Snare)\nAddHit(0, Kick)\nAddHit(0, HihatClosed)\nAddHit(1, Snare)\n"));
    const auto& pattern = engine.result().pattern;
    ASSERT_EQ(pattern.hitCount, 4u);
    EXPECT_EQ(pattern.hits[0].instrument, Instrument::Kick);
    EXPECT_EQ(pattern.hits[1].instrument, Instrument::HihatClosed);
    EXPECT_FLOAT_EQ(pattern.hits[2].positionBeats, 1.f);
    EXPECT_FLOAT_EQ(pattern.hits[3].positionBeats, 3.f);
}

TEST(DrumTrainerScriptEngineTest, VelocityMustBeAWholeNumberFromOneTo127)
{
    for (const auto* velocity : {"0", "128", "64.5", "0/0"})
    {
        DrumTrainerScriptEngine engine;
        EXPECT_FALSE(loads(engine, std::string{"AddHit(0, Kick, "} + velocity + ")\n")) << velocity;
        EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("velocity")) << velocity;
    }
    DrumTrainerScriptEngine engine;
    EXPECT_TRUE(loads(engine, "AddHit(0, Kick, 1)\nAddHit(1, Kick, 127)\n"));
}

TEST(DrumTrainerScriptEngineTest, AnInstrumentMayAppearOnlyOncePerPosition)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddHit(1, Snare)\nAddHit(1, Snare, 50)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("clashes with Snare"));
}

TEST(DrumTrainerScriptEngineTest, ClosedAndOpenHihatCannotShareAPosition)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddHit(1.5, HihatClosed)\nAddHit(1.5, HihatOpen)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("HihatOpen at position 1.5 clashes with HihatClosed"));
}

TEST(DrumTrainerScriptEngineTest, DifferentLimbsMayShareAPosition)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "AddHit(0, Kick)\nAddHit(0, Crash)\nAddHit(0, HihatOpen)\n"));
    EXPECT_EQ(engine.result().pattern.hitCount, 3u);
}

TEST(DrumTrainerScriptEngineTest, PositionOutsideThePatternRaisesAScriptError)
{
    for (const auto* position : {"4", "-0.5", "1/0"})
    {
        DrumTrainerScriptEngine engine;
        EXPECT_FALSE(loads(engine, std::string{"AddHit("} + position + ", Kick)\n")) << position;
        EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the pattern")) << position;
    }
}

TEST(DrumTrainerScriptEngineTest, UnknownInstrumentRaisesAScriptError)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddHit(0, 8)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("unknown instrument"));
}

TEST(DrumTrainerScriptEngineTest, UndefinedInstrumentNameRaisesAScriptError)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddHit(0, Hihat)\n"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DrumTrainerScriptEngineTest, LengthGlobalsStartAtThePresetBarAndOneBar)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("if BarBeats ~= 7 or PatternBars ~= 1 then error('wrong length') end\n", 7));
}

TEST(DrumTrainerScriptEngineTest, SetPatternBarsWidensThePatternAndUpdatesTheGlobal)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetPatternBars(2)\nif PatternBars ~= 2 then error('no') end\nAddHit(7.5, Crash)\n"));
    const auto& pattern = engine.result().pattern;
    EXPECT_TRUE(pattern.active);
    EXPECT_EQ(pattern.patternBars, 2u);
    EXPECT_EQ(pattern.beatsPerBar, 0u);
    EXPECT_EQ(pattern.hitCount, 1u);
}

TEST(DrumTrainerScriptEngineTest, SetPatternBarsRejectsCountsOutsideOneToEight)
{
    for (const auto* bars : {"0", "9", "1.5"})
    {
        DrumTrainerScriptEngine engine;
        EXPECT_FALSE(loads(engine, std::string{"SetPatternBars("} + bars + ")\n")) << bars;
        EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("1 to 8")) << bars;
    }
}

TEST(DrumTrainerScriptEngineTest, ShorteningThePatternBelowAddedHitsIsAnError)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(loads(engine, "SetPatternBars(2)\nAddHit(5, Kick)\nSetPatternBars(1)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the new pattern length"));
    EXPECT_FALSE(loads(engine, "SetPatternBars(2)\nAddHit(7, Kick)\nSetBeatsPerBar(3)\n"));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the new pattern length"));
    EXPECT_TRUE(loads(engine, "SetPatternBars(2)\nAddHit(5, Kick)\nSetBeatsPerBar(3)\n"));
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarRecordsTheLengthAndUpdatesTheGlobal)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetBeatsPerBar(7)\nif BarBeats ~= 7 then error('no') end\nAddHit(6.5, Snare)\n"));
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.beatsPerBar, 7u);
}

TEST(DrumTrainerScriptEngineTest, SetBeatsPerBarRejectsLengthsOutsideOneToSixteen)
{
    for (const auto* beats : {"0", "17", "3.5", "0/0"})
    {
        DrumTrainerScriptEngine engine;
        EXPECT_FALSE(loads(engine, std::string{"SetBeatsPerBar("} + beats + ")\n")) << beats;
    }
}

TEST(DrumTrainerScriptEngineTest, ClearPatternKeepsTheLengthsSetBefore)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "SetBeatsPerBar(5)\nSetPatternBars(2)\nAddHit(0, Kick)\nClearPattern()\n"));
    const auto& pattern = engine.result().pattern;
    EXPECT_EQ(pattern.hitCount, 0u);
    EXPECT_EQ(pattern.beatsPerBar, 5u);
    EXPECT_EQ(pattern.patternBars, 2u);
}

TEST(DrumTrainerScriptEngineTest, PatternCapacityIsEnforced)
{
    DrumTrainerScriptEngine engine;
    const std::string count = std::to_string(kMaxHits);
    const std::string script = "for i = 0, " + count + " do AddHit(i * 4 / (" + count + " + 1), Kick) end\n";
    EXPECT_FALSE(loads(engine, script));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("at most"));
}

TEST(DrumTrainerScriptEngineTest, SyntaxErrorFailsTheLoadAndEmptiesTheResult)
{
    DrumTrainerScriptEngine engine;
    EXPECT_FALSE(loads(engine, "AddHit(0, Kick)\nthis is not lua\n"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(DrumTrainerScriptEngineTest, ASuccessfulLoadClearsThePreviousError)
{
    DrumTrainerScriptEngine engine;
    ASSERT_FALSE(loads(engine, "AddHit(9, Kick)\n"));
    ASSERT_TRUE(loads(engine, "AddHit(1, Kick)\n"));
    EXPECT_FALSE(engine.hasError());
}

TEST(DrumTrainerScriptEngineTest, PatternFunctionsCalledFromATimerAreRejected)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, "Timer.After(1, function() AddHit(0, Kick) SetPatternBars(2) end)\n"));
    engine.tickBlock(100000);
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(DrumTrainerScriptEngineTest, StubAndSkeletonScriptsLoadWithoutDefiningAPattern)
{
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, std::string{DrumTrainerScriptEngine::kStubScript}));
    EXPECT_FALSE(engine.result().pattern.active);
    ASSERT_TRUE(loads(engine, std::string{DrumTrainerScriptEngine::kSkeletonScript}));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(DrumTrainerScriptEngineTest, SkeletonExampleRunsWhenUncommented)
{
    std::string example;
    std::istringstream lines{std::string{DrumTrainerScriptEngine::kSkeletonScript}};
    bool inExample = false;
    for (std::string line; std::getline(lines, line);)
    {
        inExample = inExample || line.starts_with("-- SetName('");
        if (inExample && line.size() > 3)
        {
            example += line.substr(3) + "\n";
        }
    }
    DrumTrainerScriptEngine engine;
    ASSERT_TRUE(loads(engine, example)) << engine.lastError() << "\n" << example;
    EXPECT_EQ(engine.result().name, "Rock beat");
    EXPECT_EQ(engine.result().pattern.hitCount, 12u);
}

TEST(DrumTrainerBaseScriptsTest, EveryShippedScriptLoadsForCommonBarLengths)
{
    size_t scriptCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator{DRUMTRAINER_BASE_SCRIPTS_DIR})
    {
        if (entry.path().extension() != ".lua")
        {
            continue;
        }
        std::ifstream file{entry.path()};
        std::stringstream source;
        source << file.rdbuf();
        for (const size_t barBeats : {3u, 4u, 7u})
        {
            DrumTrainerScriptEngine engine;
            EXPECT_TRUE(engine.loadPattern(source.str(), barBeats))
                << entry.path().filename() << " with " << barBeats << " beats: " << engine.lastError();
            EXPECT_TRUE(engine.result().pattern.active) << entry.path().filename();
            EXPECT_GT(engine.result().pattern.hitCount, 0u) << entry.path().filename();
            EXPECT_FALSE(engine.result().name.empty()) << entry.path().filename();
        }
        ++scriptCount;
    }
    EXPECT_GE(scriptCount, 5u);
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
