#include <cmath>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "impl/MetronomeScriptEngine.h"

using namespace MetronomePattern;

namespace
{
constexpr size_t kBarBeats{4};

float dbToLinear(const float db)
{
    return std::pow(10.f, db / 20.f);
}
}

TEST(MetronomeScriptEngineTest, ScriptWithoutPatternCallsLeavesBothInactive)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("x = 1\n", kBarBeats));
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_FALSE(engine.result().analysis.active);
}

TEST(MetronomeScriptEngineTest, ClearPatternAloneYieldsActiveSilentPattern)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("ClearPattern()\n", kBarBeats));
    EXPECT_TRUE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(MetronomeScriptEngineTest, AddInstrumentRecordsPositionInstrumentAndLinearGain)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddInstrument(1.5, Snare, -6)\n", kBarBeats));
    const auto& pattern = engine.result().pattern;
    ASSERT_TRUE(pattern.active);
    ASSERT_EQ(pattern.hitCount, 1u);
    EXPECT_FLOAT_EQ(pattern.hits[0].positionBeats, 1.5f);
    EXPECT_EQ(pattern.hits[0].instrument, Instrument::Snare);
    EXPECT_NEAR(pattern.hits[0].gain, dbToLinear(-6.f), 1e-6f);
}

TEST(MetronomeScriptEngineTest, HitsAreKeptSortedAndStackedHitsKeepCallOrder)
{
    MetronomeScriptEngine engine;
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

TEST(MetronomeScriptEngineTest, LevelIsClampedToTheSupportedRange)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddInstrument(0, Kick, -500)\nAddInstrument(1, Kick, 40)\n", kBarBeats));
    const auto& pattern = engine.result().pattern;
    EXPECT_NEAR(pattern.hits[0].gain, dbToLinear(kMinLevelDb), 1e-7f);
    EXPECT_NEAR(pattern.hits[1].gain, dbToLinear(kMaxLevelDb), 1e-4f);
}

TEST(MetronomeScriptEngineTest, ClearPatternDiscardsEarlierHits)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(
        engine.loadPattern("AddInstrument(0, Kick, 0)\nClearPattern()\nAddInstrument(1, Snare, 0)\n", kBarBeats));
    ASSERT_EQ(engine.result().pattern.hitCount, 1u);
    EXPECT_EQ(engine.result().pattern.hits[0].instrument, Instrument::Snare);
}

TEST(MetronomeScriptEngineTest, BarBeatsGlobalReflectsTheRequestedBarLength)
{
    MetronomeScriptEngine engine;
    const std::string script = "for beat = 0, BarBeats - 1 do AddInstrument(beat, Kick, 0) end\n";
    ASSERT_TRUE(engine.loadPattern(script, 7));
    EXPECT_EQ(engine.result().pattern.hitCount, 7u);
    ASSERT_TRUE(engine.loadPattern(script, 3));
    EXPECT_EQ(engine.result().pattern.hitCount, 3u);
}

TEST(MetronomeScriptEngineTest, AnalysisPositionsAreSortedAndDeduplicated)
{
    MetronomeScriptEngine engine;
    const std::string script = "AddAnalysisPosition(2)\n"
                               "AddAnalysisPosition(0.5)\n"
                               "AddAnalysisPosition(2)\n"
                               "AddAnalysisPosition(3.75)\n";
    ASSERT_TRUE(engine.loadPattern(script, kBarBeats));
    const auto& analysis = engine.result().analysis;
    ASSERT_TRUE(analysis.active);
    ASSERT_EQ(analysis.count, 3u);
    EXPECT_FLOAT_EQ(analysis.positions[0], 0.5f);
    EXPECT_FLOAT_EQ(analysis.positions[1], 2.f);
    EXPECT_FLOAT_EQ(analysis.positions[2], 3.75f);
}

TEST(MetronomeScriptEngineTest, ClearAnalysisAloneYieldsActiveEmptyGrid)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddAnalysisPosition(1)\nClearAnalysis()\n", kBarBeats));
    EXPECT_TRUE(engine.result().analysis.active);
    EXPECT_EQ(engine.result().analysis.count, 0u);
}

TEST(MetronomeScriptEngineTest, PlayedPatternAndAnalysisAreIndependent)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("AddInstrument(0, Kick, 0)\nAddAnalysisPosition(0.333)\n", kBarBeats));
    EXPECT_EQ(engine.result().pattern.hitCount, 1u);
    EXPECT_EQ(engine.result().analysis.count, 1u);
}

TEST(MetronomeScriptEngineTest, PositionOutsideTheBarRaisesAScriptError)
{
    MetronomeScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(4, Kick, 0)\n", kBarBeats));
    EXPECT_TRUE(engine.hasError());
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the bar"));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(MetronomeScriptEngineTest, NegativeAnalysisPositionRaisesAScriptError)
{
    MetronomeScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddAnalysisPosition(-0.5)\n", kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("outside the bar"));
}

TEST(MetronomeScriptEngineTest, UnknownInstrumentRaisesAScriptError)
{
    MetronomeScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(0, 999, 0)\n", kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("unknown instrument"));
}

TEST(MetronomeScriptEngineTest, UndefinedInstrumentNameRaisesAScriptError)
{
    MetronomeScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(0, Cowbell, 0)\n", kBarBeats));
    EXPECT_TRUE(engine.hasError());
}

TEST(MetronomeScriptEngineTest, PatternCapacityIsEnforced)
{
    MetronomeScriptEngine engine;
    const std::string script = "for i = 1, " + std::to_string(kMaxHits + 1) + " do AddInstrument(0, Kick, 0) end\n";
    EXPECT_FALSE(engine.loadPattern(script, kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("at most"));
}

TEST(MetronomeScriptEngineTest, AnalysisCapacityIsEnforced)
{
    MetronomeScriptEngine engine;
    const std::string count = std::to_string(kMaxAnalysisPositions);
    const std::string script = "for i = 0, " + count + " do AddAnalysisPosition(i * 4 / (" + count + " + 1)) end\n";
    EXPECT_FALSE(engine.loadPattern(script, kBarBeats));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("at most"));
}

TEST(MetronomeScriptEngineTest, SyntaxErrorFailsTheLoadAndEmptiesTheResult)
{
    MetronomeScriptEngine engine;
    EXPECT_FALSE(engine.loadPattern("AddInstrument(0, Kick, 0)\nthis is not lua\n", kBarBeats));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(MetronomeScriptEngineTest, ASuccessfulLoadClearsThePreviousError)
{
    MetronomeScriptEngine engine;
    ASSERT_FALSE(engine.loadPattern("AddInstrument(9, Kick, 0)\n", kBarBeats));
    ASSERT_TRUE(engine.loadPattern("AddInstrument(1, Kick, 0)\n", kBarBeats));
    EXPECT_FALSE(engine.hasError());
}

TEST(MetronomeScriptEngineTest, PatternFunctionsCalledFromATimerAreRejected)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern("Timer.After(1, function() AddInstrument(0, Kick, 0) end)\n", kBarBeats));
    engine.tickBlock(100000);
    EXPECT_FALSE(engine.result().pattern.active);
    EXPECT_EQ(engine.result().pattern.hitCount, 0u);
}

TEST(MetronomeScriptEngineTest, StubAndSkeletonScriptsLoadWithoutDefiningAPattern)
{
    MetronomeScriptEngine engine;
    ASSERT_TRUE(engine.loadPattern(std::string{MetronomeScriptEngine::kStubScript}, kBarBeats));
    EXPECT_FALSE(engine.result().pattern.active);
    ASSERT_TRUE(engine.loadPattern(std::string{MetronomeScriptEngine::kSkeletonScript}, kBarBeats));
    EXPECT_FALSE(engine.result().pattern.active);
}

TEST(MetronomeBaseScriptsTest, EveryShippedScriptLoadsForCommonBarLengths)
{
    size_t scriptCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator{METRONOME_BASE_SCRIPTS_DIR})
    {
        if (entry.path().extension() != ".lua")
        {
            continue;
        }
        std::ifstream file{entry.path()};
        std::stringstream source;
        source << file.rdbuf();
        for (const size_t barBeats : {2u, 3u, 4u, 5u, 7u, 13u})
        {
            MetronomeScriptEngine engine;
            EXPECT_TRUE(engine.loadPattern(source.str(), barBeats))
                << entry.path().filename() << " with " << barBeats << " beats: " << engine.lastError();
            EXPECT_TRUE(engine.result().pattern.active) << entry.path().filename();
            EXPECT_GT(engine.result().pattern.hitCount, 0u) << entry.path().filename();
            EXPECT_GT(engine.result().analysis.count, 0u) << entry.path().filename();
        }
        ++scriptCount;
    }
    EXPECT_GE(scriptCount, 4u);
}
