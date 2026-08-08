#include <gtest/gtest.h>
#include <tuple>

#include "impl/DroneScriptEngine.h"

TEST(DroneScriptEngine, StubScriptReturnsOneNote)
{
    DroneScriptEngine engine;
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 60.f);
    EXPECT_FALSE(engine.hasError());
}

TEST(DroneScriptEngine, MultiNoteScriptIsCappedAtMaxNotesPerRequest)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            local notes = {}
            for i = 1, 20 do
                notes[i] = { note = i, velocity = 0.5, channel = 0, length = 0, delay = 0 }
            end
            return notes
        end
    )"));
    const auto result = engine.nextNotes();
    EXPECT_EQ(result.count, DroneScriptEngine::kMaxNotesPerRequest);
}

TEST(DroneScriptEngine, EmptyTableMeansPlayNothing)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function NextNotes() return {} end"));
    const auto result = engine.nextNotes();
    EXPECT_EQ(result.count, 0u);
}

TEST(DroneScriptEngine, MissingNextNotesFunctionYieldsEmptyResult)
{
    DroneScriptEngine engine;
    // The constructor's stub script already defines NextNotes; explicitly clear it to
    // exercise the "function truly absent" path rather than the "keeps prior script
    // running" path (that one's covered by MalformedScriptIsRejectedAndPreviousScriptKeepsRunning).
    ASSERT_TRUE(engine.loadScript("NextNotes = nil"));
    const auto result = engine.nextNotes();
    EXPECT_EQ(result.count, 0u);
}

TEST(DroneScriptEngine, MalformedScriptIsRejectedAndPreviousScriptKeepsRunning)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function NextNotes() return { { note = 61 } } end"));

    EXPECT_FALSE(engine.loadScript("function NextNotes( this is not lua"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.lastError().empty());

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 61.f);
}

TEST(DroneScriptEngine, TopLevelRuntimeErrorIsRejectedAtLoadTime)
{
    // Not a syntax error - "rand" is a real Lua expression, just an undefined global -
    // so this only fails when the top-level chunk actually runs (calls it), not when
    // it's parsed. Distinct from MalformedScriptIsRejectedAndPreviousScriptKeepsRunning,
    // which covers a genuine parse/syntax failure.
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function NextNotes() return { { note = 61 } } end"));

    EXPECT_FALSE(engine.loadScript("x = rand()\nfunction NextNotes() return {} end"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.lastError().empty());

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u) << "previous script's NextNotes should still be running";
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 61.f);
}

TEST(DroneScriptEngine, RuntimeErrorInsideNextNotesIsCaughtPerCallAndClearsOnRecovery)
{
    // A script that compiles fine (loadScript succeeds) can still error every time it's
    // actually called, e.g. a typo'd function name only reached inside the function body.
    // This must be caught per-call, not just at load time.
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        should_fail = true
        function NextNotes()
            if should_fail then
                local a = 60 + rand()
                return { { note = a } }
            end
            return { { note = 61 } }
        end
    )"));
    EXPECT_FALSE(engine.hasError()) << "loading must not eagerly call NextNotes()";

    const auto failing = engine.nextNotes();
    EXPECT_EQ(failing.count, 0u);
    EXPECT_TRUE(engine.hasError());
    EXPECT_FALSE(engine.lastError().empty());

    ASSERT_TRUE(engine.loadScript("should_fail = false"));
    const auto recovered = engine.nextNotes();
    ASSERT_EQ(recovered.count, 1u);
    EXPECT_FLOAT_EQ(recovered.notes[0].noteHeight, 61.f);
    EXPECT_FALSE(engine.hasError()) << "a later successful call must clear the stale error";
}

TEST(DroneScriptEngine, NotifyTimingFeedsGlobalsIntoNextNotes)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function OnTiming(bpm, division)
            BPM = bpm
            DIVISION = division
        end
        function NextNotes()
            return { { note = BPM or -1, velocity = 0, channel = 0, length = 0, delay = DIVISION or -1 } }
        end
    )"));
    engine.notifyTiming(133.f, 7);
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 133.f);
    EXPECT_FLOAT_EQ(result.notes[0].delayMs, 7.f);
}

TEST(DroneScriptEngine, NoteFieldsRoundTripThroughLua)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            return { { note = 67, velocity = 0.42, channel = 3, length = 250, delay = -15 } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    const auto& note = result.notes[0];
    EXPECT_FLOAT_EQ(note.noteHeight, 67.f);
    EXPECT_FLOAT_EQ(note.velocity, 0.42f);
    EXPECT_EQ(note.channel, 3u);
    EXPECT_FLOAT_EQ(note.lengthMs, 250.f);
    EXPECT_FLOAT_EQ(note.delayMs, -15.f);
}

TEST(DroneScriptEngine, RepeatedCallsDoNotGrowPoolUsageUnbounded)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            local notes = {}
            for i = 1, 8 do
                notes[i] = { note = i, velocity = 0.1, channel = i - 1, length = 10, delay = 0 }
            end
            return notes
        end
    )"));
    for (int i = 0; i < 500; ++i)
    {
        std::ignore = engine.nextNotes();
    }
    engine.collectGarbage();
    const size_t warmedUp = engine.poolBytesInUse();
    for (int i = 0; i < 5000; ++i)
    {
        std::ignore = engine.nextNotes();
    }
    engine.collectGarbage();
    EXPECT_EQ(engine.poolBytesInUse(), warmedUp)
        << "after a full GC cycle, live pool usage should be identical regardless of "
           "how many NextNotes() calls happened in between - anything else is a leak";
}
