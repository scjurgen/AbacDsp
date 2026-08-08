#include <cassert>
#include <gtest/gtest.h>
#include <memory>
#include <string>
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

namespace
{
// Every MIDI notify*() test loads a script recording the last-seen event as globals,
// then smuggles them back out through the already-proven NextNotes()/DroneNote channel
// rather than adding a test-only "read a Lua global" API to production code.
// Returned via unique_ptr: DroneScriptEngine is deliberately non-movable (its pool's
// free-list pointers are relative to its own arena's address), so it can't come back
// from a factory function by value.
std::unique_ptr<DroneScriptEngine> makeEngineRecordingLastEvent(const char* globalsAssignment)
{
    auto engine = std::make_unique<DroneScriptEngine>();
    std::string script = globalsAssignment;
    script += R"(
        function NextNotes()
            return { { note = A or -1, velocity = B or -1, channel = C or -1, length = D or -1, delay = 0 } }
        end
    )";
    [[maybe_unused]] const bool loaded = engine->loadScript(script);
    assert(loaded);
    return engine;
}
}

TEST(DroneScriptEngine, NotifyNoteOnDispatchesToOnNoteOn)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnNoteOn(channel, note, velocity) A = note; B = velocity; C = channel end
    )");
    engine->notifyNoteOn(2, 60, 100);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 60.f);
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 100.f);
    EXPECT_EQ(result.notes[0].channel, 2u);
}

TEST(DroneScriptEngine, NotifyNoteOffDispatchesToOnNoteOff)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnNoteOff(channel, note, velocity) A = note; B = velocity; C = channel end
    )");
    engine->notifyNoteOff(1, 61, 5);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 61.f);
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 5.f);
    EXPECT_EQ(result.notes[0].channel, 1u);
}

TEST(DroneScriptEngine, NotifyCcDispatchesToOnCC)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnCC(channel, ccNumber, value) A = ccNumber; B = value; C = channel end
    )");
    engine->notifyCC(3, 74, 127);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 74.f);
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 127.f);
    EXPECT_EQ(result.notes[0].channel, 3u);
}

TEST(DroneScriptEngine, NotifyProgramChangeDispatchesToOnProgramChange)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnProgramChange(channel, program) A = program; C = channel end
    )");
    engine->notifyProgramChange(4, 12);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 12.f);
    EXPECT_EQ(result.notes[0].channel, 4u);
}

TEST(DroneScriptEngine, NotifyAftertouchDispatchesToOnAftertouch)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnAftertouch(channel, value) A = value; C = channel end
    )");
    engine->notifyAftertouch(5, 99);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 99.f);
    EXPECT_EQ(result.notes[0].channel, 5u);
}

TEST(DroneScriptEngine, NotifyPolyPressureDispatchesToOnPolyPressure)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnPolyPressure(channel, note, value) A = note; B = value; C = channel end
    )");
    engine->notifyPolyPressure(6, 72, 80);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 72.f);
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 80.f);
    EXPECT_EQ(result.notes[0].channel, 6u);
}

TEST(DroneScriptEngine, NotifyPitchBendDispatchesToOnPitchBend)
{
    auto engine = makeEngineRecordingLastEvent(R"(
        function OnPitchBend(channel, bendValue) A = bendValue; C = channel end
    )");
    engine->notifyPitchBend(7, 8192);
    const auto result = engine->nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 8192.f);
    EXPECT_EQ(result.notes[0].channel, 7u);
}

TEST(DroneScriptEngine, MissingMidiHandlersAreSilentlyIgnored)
{
    // No OnNoteOn/etc defined at all - should behave like OnTiming's absence: no-op,
    // no error, previous NextNotes() behavior unaffected.
    DroneScriptEngine engine;
    engine.notifyNoteOn(0, 60, 100);
    engine.notifyNoteOff(0, 60, 0);
    engine.notifyCC(0, 1, 1);
    engine.notifyProgramChange(0, 1);
    engine.notifyAftertouch(0, 1);
    engine.notifyPolyPressure(0, 60, 1);
    engine.notifyPitchBend(0, 8192);
    EXPECT_FALSE(engine.hasError());
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u) << "stub script's NextNotes should be unaffected";
}

TEST(DroneScriptEngine, NotifyStartAndStopDispatchToOnStartAndOnStop)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        Started = 0
        Stopped = 0
        function OnStart() Started = Started + 1 end
        function OnStop() Stopped = Stopped + 1 end
        function NextNotes()
            return { { note = Started, velocity = Stopped, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    engine.notifyStart();
    engine.notifyStart();
    engine.notifyStop();
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 2.f) << "OnStart should have fired twice";
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 1.f) << "OnStop should have fired once";
}

TEST(DroneScriptEngine, MissingStartStopHandlersAreSilentlyIgnored)
{
    DroneScriptEngine engine;
    engine.notifyStart();
    engine.notifyStop();
    EXPECT_FALSE(engine.hasError());
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u) << "stub script's NextNotes should be unaffected";
}

TEST(DroneScriptEngine, FullSkeletonScriptLoadsCleanlyAndDefinesEveryHandler)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(DroneScriptEngine::kFullSkeletonScript));
    EXPECT_FALSE(engine.hasError());
    const auto result = engine.nextNotes();
    EXPECT_EQ(result.count, 0u) << "skeleton's NextNotes deliberately returns an empty table";

    // Exercise every handler the skeleton claims to define - none should error, since an
    // empty function body is trivially valid regardless of the arguments passed to it.
    engine.notifyTiming(120.f, 4);
    engine.notifyStart();
    engine.notifyStop();
    engine.notifyNoteOn(0, 60, 100);
    engine.notifyNoteOff(0, 60, 0);
    engine.notifyCC(0, 1, 1);
    engine.notifyProgramChange(0, 1);
    engine.notifyAftertouch(0, 1);
    engine.notifyPolyPressure(0, 60, 1);
    engine.notifyPitchBend(0, 0);
    EXPECT_FALSE(engine.hasError());
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
