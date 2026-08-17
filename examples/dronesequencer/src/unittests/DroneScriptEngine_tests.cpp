#include <array>
#include <cassert>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "impl/DroneScriptEngine.h"

namespace
{
std::vector<float> makeSineBlock(const float sampleRate, const float frequencyHz, const size_t numSamples)
{
    std::vector<float> block(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        block[i] = std::sin(2.f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / sampleRate);
    }
    return block;
}
}

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
    engine.notifyTransportSnapshot(140.0, 1.0, 1.0, 3, 8, true, true, true);
    engine.tickBlock(64);
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

TEST(DroneScriptEngine, UiParametersStartUnclaimed)
{
    DroneScriptEngine engine;
    for (const auto& slot : engine.uiParamSlots())
    {
        EXPECT_FALSE(slot.claimed);
    }
}

TEST(DroneScriptEngine, RegisteringOneKnobParameterClaimsSlotZero)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "cutoff", name = "Cutoff", type = "knob",
              range = { min = 200, max = 8000, step = 1, skew = 0.3 },
              default = 1000, unit = "Hz", description = "Filter cutoff" },
        })
        function NextNotes() return {} end
    )"));
    const auto& slot = engine.uiParamSlots()[0];
    EXPECT_TRUE(slot.claimed);
    EXPECT_EQ(slot.id, "cutoff");
    EXPECT_EQ(slot.name, "Cutoff");
    EXPECT_EQ(slot.type, LuaUiParamType::Knob);
    EXPECT_FLOAT_EQ(slot.rangeMin, 200.f);
    EXPECT_FLOAT_EQ(slot.rangeMax, 8000.f);
    EXPECT_FLOAT_EQ(slot.rangeSkew, 0.3f);
    EXPECT_FLOAT_EQ(slot.defaultValue, 1000.f);
    EXPECT_EQ(slot.unit, "Hz");
    EXPECT_EQ(slot.description, "Filter cutoff");
    for (size_t i = 1; i < DroneScriptEngine::kMaxLuaParams; ++i)
    {
        EXPECT_FALSE(engine.uiParamSlots()[i].claimed);
    }
}

TEST(DroneScriptEngine, RegisteringSwitchAndDropParameters)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "sync", name = "Sync", type = "switch", default = 1 },
            { id = "wave", name = "Waveform", type = "drop",
              items = { "Sine", "Saw", "Square" }, default = 2 },
        })
        function NextNotes() return {} end
    )"));
    const auto& sync = engine.uiParamSlots()[0];
    EXPECT_EQ(sync.type, LuaUiParamType::Switch);
    EXPECT_FLOAT_EQ(sync.rangeMin, 0.f);
    EXPECT_FLOAT_EQ(sync.rangeMax, 1.f);
    EXPECT_FLOAT_EQ(sync.defaultValue, 1.f);

    const auto& wave = engine.uiParamSlots()[1];
    EXPECT_EQ(wave.type, LuaUiParamType::Drop);
    ASSERT_EQ(wave.items.size(), 3u);
    EXPECT_EQ(wave.items[0], "Sine");
    EXPECT_EQ(wave.items[2], "Square");
    EXPECT_FLOAT_EQ(wave.rangeMin, 0.f);
    EXPECT_FLOAT_EQ(wave.rangeMax, 2.f);
    EXPECT_FLOAT_EQ(wave.defaultValue, 2.f);
}

TEST(DroneScriptEngine, OverflowingParameterPoolFailsScriptLoad)
{
    DroneScriptEngine engine;
    std::string script = "UICreateParameterSet({\n";
    for (size_t i = 0; i < DroneScriptEngine::kMaxLuaParams + 1; ++i)
    {
        script += "{ id = \"p" + std::to_string(i) + "\", type = \"switch\" },\n";
    }
    script += "})\nfunction NextNotes() return {} end\n";

    EXPECT_FALSE(engine.loadScript(script));
    EXPECT_TRUE(engine.hasError());
    for (const auto& slot : engine.uiParamSlots())
    {
        EXPECT_FALSE(slot.claimed) << "stub script's empty parameter set should still be live";
    }
}

TEST(DroneScriptEngine, DuplicateParameterIdsFailScriptLoad)
{
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "gain", type = "switch" },
            { id = "gain", type = "switch" },
        })
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DroneScriptEngine, ReloadingWithoutUICreateParameterSetClearsPreviousParameters)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({ { id = "gain", type = "switch" } })
        function NextNotes() return {} end
    )"));
    ASSERT_TRUE(engine.uiParamSlots()[0].claimed);

    ASSERT_TRUE(engine.loadScript("function NextNotes() return {} end"));
    for (const auto& slot : engine.uiParamSlots())
    {
        EXPECT_FALSE(slot.claimed);
    }
}

TEST(DroneScriptEngine, FailedReloadKeepsPreviousParameterSet)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({ { id = "gain", type = "switch" } })
        function NextNotes() return {} end
    )"));
    ASSERT_TRUE(engine.uiParamSlots()[0].claimed);

    EXPECT_FALSE(engine.loadScript("function NextNotes( this is not lua"));
    EXPECT_TRUE(engine.uiParamSlots()[0].claimed) << "previous parameter set should still be live";
    EXPECT_EQ(engine.uiParamSlots()[0].id, "gain");
}

TEST(DroneScriptEngine, NotifyUiParameterChangedDispatchesToNamedCallback)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "cutoff", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0 },
        })
        LastCutoff = -1
        function OnCutoffChanged(value) LastCutoff = value end
        function NextNotes() return { { note = LastCutoff, velocity = 0, channel = 0, length = 0, delay = 0 } } end
    )"));
    engine.notifyUiParameterChanged(0, 0.75f);
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 0.75f);
}

// Regression: the notify value must be mapped through the slot's declared display
// range, not handed through raw - invisible for a [0,1] range (test above), but breaks
// anything else, e.g. a "drop" slot's discrete index comparisons.
TEST(DroneScriptEngine, NotifyUiParameterChangedMapsThroughDeclaredRange)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "pitch", type = "knob", range = { min = -12, max = 12, step = 1, skew = 1 }, default = 0 },
            { id = "pattern", type = "drop", items = { "Up", "Down", "Random" }, default = 0 },
        })
        LastPitch = -1000
        LastPattern = -1
        function OnPitchChanged(value) LastPitch = value end
        function OnPatternChanged(value) LastPattern = value end
        function NextNotes()
            return { { note = LastPitch, velocity = LastPattern, channel = 0, length = 0, delay = 0 } }
        end
    )"));

    engine.notifyUiParameterChanged(0, 0.75f); // pitch: -12..12, raw 0.75 -> display 6
    engine.notifyUiParameterChanged(1, 0.5f);  // pattern: 0..2 (3 items), raw 0.5 -> display 1 ("Down")
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 6.f);
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 1.f);
}

TEST(DroneScriptEngine, NotifyUiParameterChangedMapsThroughSkewedRange)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "depth", type = "knob", range = { min = 0, max = 100, step = 0, skew = 2 }, default = 0 },
        })
        LastDepth = -1000
        function OnDepthChanged(value) LastDepth = value end
        function NextNotes()
            return { { note = LastDepth, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));

    // skew = 2: display = span * normalized^(1/skew), matching juce::NormalisableRange's
    // convertFrom0to1 - not normalized^skew, which was the bug this test guards against.
    engine.notifyUiParameterChanged(0, 0.25f); // -> 100 * sqrt(0.25) = 50
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 50.f);
}

TEST(DroneScriptEngine, NotifyUiParameterChangedOnUnclaimedOrOutOfRangeSlotIsIgnored)
{
    DroneScriptEngine engine;
    engine.notifyUiParameterChanged(0, 1.f);
    engine.notifyUiParameterChanged(DroneScriptEngine::kMaxLuaParams, 1.f);
    engine.notifyUiParameterChanged(DroneScriptEngine::kMaxLuaParams + 100, 1.f);
    EXPECT_FALSE(engine.hasError());
}

TEST(DroneScriptEngine, InvalidParameterIdIsRejected)
{
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        UICreateParameterSet({ { id = "1bad", type = "switch" } })
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DroneScriptEngine, MissingRequiredFieldIsRejected)
{
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        UICreateParameterSet({ { id = "noType" } })
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DroneScriptEngine, KnobRangeMustBeNonInverted)
{
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "bad", type = "knob", range = { min = 5, max = 5, step = 1, skew = 1 } },
        })
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DroneScriptEngine, DefaultValueOutsideRangeIsRejected)
{
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        UICreateParameterSet({
            { id = "bad", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 5 },
        })
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
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

TEST(DroneScriptEngine, TimerAfterFiresOnceWhenDue)
{
    DroneScriptEngine engine;
    engine.setSampleRate(1000.f);
    ASSERT_TRUE(engine.loadScript(R"(
        Fired = 0
        Timer.After(10, function() Fired = Fired + 1 end)
        function NextNotes()
            return { { note = Fired, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    engine.tickBlock(5);
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 0.f) << "not due yet";
    engine.tickBlock(10);
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 1.f) << "fired once when due";
    engine.tickBlock(100);
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 1.f) << "Timer.After is one-shot";
}

TEST(DroneScriptEngine, TimerEveryFiresRepeatedly)
{
    DroneScriptEngine engine;
    engine.setSampleRate(1000.f);
    ASSERT_TRUE(engine.loadScript(R"(
        Fired = 0
        Timer.Every(10, function() Fired = Fired + 1 end)
        function NextNotes()
            return { { note = Fired, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    engine.tickBlock(10);
    engine.tickBlock(10);
    engine.tickBlock(10);
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 3.f);
}

TEST(DroneScriptEngine, TimerCancelStopsFutureFiring)
{
    DroneScriptEngine engine;
    engine.setSampleRate(1000.f);
    ASSERT_TRUE(engine.loadScript(R"(
        Fired = 0
        local id = Timer.Every(10, function() Fired = Fired + 1 end)
        Timer.Cancel(id)
        function NextNotes()
            return { { note = Fired, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    engine.tickBlock(100);
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 0.f);
}

TEST(DroneScriptEngine, TimerPoolExhaustionReturnsZero)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        Ids = {}
        for i = 1, 9 do
            Ids[i] = Timer.After(10000, function() end)
        end
        function NextNotes()
            return { { note = Ids[9], velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 0.f) << "only 8 timer slots are available";
}

TEST(DroneScriptEngine, TimerSlotsAreClearedOnScriptReload)
{
    DroneScriptEngine engine;
    engine.setSampleRate(1000.f);
    ASSERT_TRUE(engine.loadScript(R"(
        Fired = false
        Timer.After(10, function() Fired = true end)
        function NextNotes()
            return { { note = Fired and 1 or 0, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    ASSERT_TRUE(engine.loadScript(R"(
        Fired = false
        function NextNotes()
            return { { note = Fired and 1 or 0, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    engine.tickBlock(100);
    EXPECT_FLOAT_EQ(engine.nextNotes().notes[0].noteHeight, 0.f)
        << "the first script's timer callback must not survive a reload";
}

TEST(DroneScriptEngine, TransportDefaultsMatchDocumentedDefaults)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            local n, d = Transport.TimeSignature()
            return { { note = Transport.Tempo(), velocity = n / 100, channel = d,
                       length = Transport.PlayingState() and 1 or 0, delay = Transport.TimeInSeconds() } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 120.f) << "default tempo is 120 bpm";
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 0.04f) << "default time signature is 4/4";
    EXPECT_EQ(result.notes[0].channel, 4u) << "default time signature is 4/4";
    EXPECT_FLOAT_EQ(result.notes[0].lengthMs, 0.f) << "default playing state is false";
    EXPECT_FLOAT_EQ(result.notes[0].delayMs, 0.f) << "default time in seconds is 0";
}

TEST(DroneScriptEngine, TransportQueryFunctionsReflectLastSnapshot)
{
    DroneScriptEngine engine;
    engine.notifyTransportSnapshot(133.0, 12.5, 6.25, 7, 8, true, true, true);
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            local n, d = Transport.TimeSignature()
            return { { note = Transport.Tempo(), velocity = Transport.TimeInQuarterNotes() / 100,
                       channel = n, length = d, delay = Transport.TimeInSeconds() } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 133.f);
    EXPECT_NEAR(result.notes[0].velocity, 0.125f, 1e-5f);
    EXPECT_EQ(result.notes[0].channel, 7u);
    EXPECT_FLOAT_EQ(result.notes[0].lengthMs, 8.f);
    EXPECT_FLOAT_EQ(result.notes[0].delayMs, 6.25f);
}

TEST(DroneScriptEngine, TransportLoopingAndRecordingStateReflectSnapshot)
{
    DroneScriptEngine engine;
    engine.notifyTransportSnapshot(120.0, 0.0, 0.0, 4, 4, false, true, true);
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            return { { note = Transport.LoopingState() and 1 or 0, velocity = Transport.RecordingState() and 1 or 0,
                       channel = 0, length = 0, delay = 0 } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 1.f);
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 1.f);
}

TEST(DroneScriptEngine, NotifyTransportSnapshotFiresChangeHandlersOnlyOnActualChange)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        TempoChanges = 0
        TimeSigChanges = 0
        Starts = 0
        Stops = 0
        function OnTempoChanged(bpm) TempoChanges = TempoChanges + 1 end
        function OnTimeSignatureChanged(n, d) TimeSigChanges = TimeSigChanges + 1 end
        function OnPlayingStart() Starts = Starts + 1 end
        function OnPlayingStop() Stops = Stops + 1 end
        function NextNotes()
            return { { note = TempoChanges, velocity = TimeSigChanges, channel = Starts, length = Stops, delay = 0 } }
        end
    )"));

    // Defaults are 120 bpm / 4:4 / not playing; a first snapshot matching those should
    // not fire anything.
    engine.notifyTransportSnapshot(120.0, 0.0, 0.0, 4, 4, false, false, false);
    auto result = engine.nextNotes();
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 0.f) << "tempo unchanged from default";
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 0.f) << "time signature unchanged from default";
    EXPECT_EQ(result.notes[0].channel, 0u) << "still not playing";

    engine.notifyTransportSnapshot(140.0, 1.0, 1.0, 3, 4, true, false, false);
    result = engine.nextNotes();
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 1.f) << "tempo changed";
    EXPECT_FLOAT_EQ(result.notes[0].velocity, 1.f) << "time signature changed";
    EXPECT_EQ(result.notes[0].channel, 1u) << "playing started";
    EXPECT_FLOAT_EQ(result.notes[0].lengthMs, 0.f) << "not stopped yet";

    engine.notifyTransportSnapshot(140.0, 2.0, 2.0, 3, 4, false, false, false);
    result = engine.nextNotes();
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 1.f) << "tempo unchanged since last snapshot";
    EXPECT_EQ(result.notes[0].channel, 1u) << "OnPlayingStart should not fire again";
    EXPECT_FLOAT_EQ(result.notes[0].lengthMs, 1.f) << "playing stopped";
}

TEST(DroneScriptEngine, MissingTransportHandlersAreSilentlyIgnored)
{
    DroneScriptEngine engine;
    engine.notifyTransportSnapshot(140.0, 1.0, 1.0, 3, 8, true, true, true);
    EXPECT_FALSE(engine.hasError());
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u) << "stub script's NextNotes should be unaffected";
}

TEST(DroneScriptEngine, ImportSplicesLibrarySourceBeforeScript)
{
    DroneScriptEngine engine;
    engine.setImportResolver(
        [](const std::string_view name) -> ImportLookup
        {
            if (name == "helpers")
            {
                return {std::string("function Helper() return 42 end"), {}};
            }
            return {std::nullopt, "not found"};
        });
    ASSERT_TRUE(engine.loadScript(R"(
        import "helpers"
        function NextNotes()
            return { { note = Helper(), velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 42.f);
}

TEST(DroneScriptEngine, ImportAcceptsExplicitLuaSuffix)
{
    // "import \"helpers.lua\"" must resolve the same library as "import \"helpers\"" -
    // this is exactly the natural spelling a user reaches for first.
    DroneScriptEngine engine;
    engine.setImportResolver(
        [](const std::string_view name) -> ImportLookup
        {
            if (name == "helpers")
            {
                return {std::string("function Helper() return 42 end"), {}};
            }
            return {std::nullopt, "not found"};
        });
    ASSERT_TRUE(engine.loadScript(R"(
        import "helpers.lua"
        function NextNotes()
            return { { note = Helper(), velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 42.f);
}

TEST(DroneScriptEngine, ImportWithInvalidNameFailsWithClearMessageRatherThanFallingThrough)
{
    // Before this fix, a name character parseImportLine didn't accept made the whole line
    // fall through as literal Lua, producing Lua's own confusing "nil value" error instead.
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        import "bad name!"
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("invalid library name"), std::string::npos) << engine.lastError();
}

TEST(DroneScriptEngine, MultipleImportsConcatenateInWrittenOrder)
{
    DroneScriptEngine engine;
    engine.setImportResolver(
        [](const std::string_view name) -> ImportLookup
        {
            if (name == "a")
            {
                return {std::string("Value = 1"), {}};
            }
            if (name == "b")
            {
                return {std::string("Value = Value + 1"), {}};
            }
            return {std::nullopt, "not found"};
        });
    ASSERT_TRUE(engine.loadScript(R"(
        import "a"
        import "b"
        function NextNotes()
            return { { note = Value, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 2.f) << "\"a\" then \"b\" must run in that order";
}

TEST(DroneScriptEngine, UnresolvedImportFailsLoadAndKeepsPreviousScriptRunning)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function NextNotes() return { { note = 61 } } end"));

    engine.setImportResolver([](std::string_view) -> ImportLookup
                             { return {std::nullopt, "looked in /fake/User/missing.lua; /fake/Base/missing.lua"}; });
    EXPECT_FALSE(engine.loadScript(R"(
        import "missing"
        function NextNotes() return { { note = 1 } } end
    )"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("missing"), std::string::npos);
    EXPECT_NE(engine.lastError().find("/fake/User/missing.lua"), std::string::npos)
        << "resolver's notFoundDetail should be folded into the error: " << engine.lastError();

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 61.f);
}

TEST(DroneScriptEngine, ImportWithNoResolverConfiguredFails)
{
    DroneScriptEngine engine;
    EXPECT_FALSE(engine.loadScript(R"(
        import "anything"
        function NextNotes() return {} end
    )"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DroneScriptEngine, ImportLineIsOnlyRecognizedAtTheTopOfTheScript)
{
    DroneScriptEngine engine;
    engine.setImportResolver(
        [](const std::string_view name) -> ImportLookup
        {
            if (name == "helpers")
            {
                return {std::string("function Helper() return 42 end"), {}};
            }
            return {std::nullopt, "not found"};
        });
    // A later import line is left as literal, non-Lua text and fails to parse - it must
    // not be treated as a directive once the header block has already ended.
    EXPECT_FALSE(engine.loadScript(R"(
        function NextNotes() return {} end
        import "helpers"
    )"));
    EXPECT_TRUE(engine.hasError());
}

TEST(DroneScriptEngine, ExciteFieldsRoundTripThroughLua)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            Excite(2, { type = "bow", start = 5, ["end"] = 100, strength = 0.75, harmonic = 3 })
            return {}
        end
    )"));
    std::ignore = engine.nextNotes();

    const auto pending = engine.drainExcitations();
    ASSERT_EQ(pending.count, 1u);
    const auto& excitation = pending.excitations[0];
    EXPECT_EQ(excitation.channel, 2u);
    EXPECT_EQ(excitation.event.type, AbacDsp::ExcitationType::Bow);
    EXPECT_FLOAT_EQ(excitation.event.startMs, 5.f);
    EXPECT_FLOAT_EQ(excitation.event.endMs, 100.f);
    EXPECT_FLOAT_EQ(excitation.event.strength, 0.75f);
    ASSERT_TRUE(excitation.event.harmonic.has_value());
    EXPECT_FLOAT_EQ(*excitation.event.harmonic, 3.f);
}

TEST(DroneScriptEngine, ExciteWithoutHarmonicLeavesItUnset)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            Excite(0, { type = "pluck", strength = 0.5 })
            return {}
        end
    )"));
    std::ignore = engine.nextNotes();

    const auto pending = engine.drainExcitations();
    ASSERT_EQ(pending.count, 1u);
    EXPECT_FALSE(pending.excitations[0].event.harmonic.has_value());
}

TEST(DroneScriptEngine, ExciteParsesAllEightTypeStrings)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            Excite(0, { type = "pluck" })
            Excite(0, { type = "strike" })
            Excite(0, { type = "mute" })
            Excite(0, { type = "palmmute" })
            Excite(0, { type = "bow" })
            Excite(0, { type = "sympathetic" })
            Excite(0, { type = "wind" })
            Excite(0, { type = "rub" })
            return {}
        end
    )"));
    std::ignore = engine.nextNotes();

    const auto pending = engine.drainExcitations();
    ASSERT_EQ(pending.count, 8u);
    const std::array<AbacDsp::ExcitationType, 8> expected{
        AbacDsp::ExcitationType::Pluck,    AbacDsp::ExcitationType::Strike, AbacDsp::ExcitationType::Mute,
        AbacDsp::ExcitationType::PalmMute, AbacDsp::ExcitationType::Bow,    AbacDsp::ExcitationType::Sympathetic,
        AbacDsp::ExcitationType::Wind,     AbacDsp::ExcitationType::Rub,
    };
    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(pending.excitations[i].event.type, expected[i]) << "index " << i;
    }
}

TEST(DroneScriptEngine, ExciteWithUnknownTypeIsIgnored)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            Excite(0, { type = "banana" })
            return {}
        end
    )"));
    std::ignore = engine.nextNotes();
    EXPECT_EQ(engine.drainExcitations().count, 0u);
}

TEST(DroneScriptEngine, DrainExcitationsClearsThePendingQueue)
{
    DroneScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            Excite(0, { type = "pluck" })
            return {}
        end
    )"));
    std::ignore = engine.nextNotes();
    ASSERT_EQ(engine.drainExcitations().count, 1u);
    EXPECT_EQ(engine.drainExcitations().count, 0u) << "a second drain without a new Excite() call must be empty";
}

TEST(DroneScriptEngine, ExciteOverflowingPendingBufferDropsExtras)
{
    DroneScriptEngine engine;
    std::string script = "function NextNotes()\n";
    for (size_t i = 0; i < DroneScriptEngine::kMaxExcitationsPerRequest + 5; ++i)
    {
        script += "    Excite(0, { type = \"pluck\" })\n";
    }
    script += "    return {}\nend\n";
    ASSERT_TRUE(engine.loadScript(script));
    std::ignore = engine.nextNotes();
    EXPECT_EQ(engine.drainExcitations().count, DroneScriptEngine::kMaxExcitationsPerRequest);
}

TEST(DroneScriptEngine, ImportHeaderSkipsBlankLinesAndComments)
{
    DroneScriptEngine engine;
    engine.setImportResolver(
        [](const std::string_view name) -> ImportLookup
        {
            if (name == "helpers")
            {
                return {std::string("function Helper() return 7 end"), {}};
            }
            return {std::nullopt, "not found"};
        });
    ASSERT_TRUE(engine.loadScript(R"(
        -- a leading comment

        import "helpers"
        function NextNotes()
            return { { note = Helper(), velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 7.f);
}

TEST(DroneScriptEngine, PitchHzDefaultsToZeroBeforeAnyAudio)
{
    const DroneScriptEngine engine;
    EXPECT_FLOAT_EQ(engine.currentPitchHz(), 0.f);
    EXPECT_FLOAT_EQ(engine.currentPitchConfidence(), 0.f);
}

TEST(DroneScriptEngine, OnPitchDetectedReceivesConfidenceAsSecondArgument)
{
    DroneScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);
    ASSERT_TRUE(engine.loadScript(R"(
        LastConfidence = -1
        function OnPitchDetected(hz, confidence)
            LastConfidence = confidence
        end
        function NextNotes()
            return { { note = LastConfidence, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 6000));

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_GT(result.notes[0].noteHeight, 0.9f);
    EXPECT_FLOAT_EQ(engine.currentPitchConfidence(), result.notes[0].noteHeight);
}

TEST(DroneScriptEngine, FeedPitchAnalysisTracksSyntheticSineFrequency)
{
    DroneScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 6000));

    EXPECT_NEAR(engine.currentPitchHz(), 220.f, 3.f);
}

TEST(DroneScriptEngine, PitchHzLuaBindingReadsCurrentValue)
{
    DroneScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);
    ASSERT_TRUE(engine.loadScript(R"(
        function NextNotes()
            return { { note = Pitch.Hz(), velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 6000));

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_NEAR(result.notes[0].noteHeight, 220.f, 3.f);
}

TEST(DroneScriptEngine, OnPitchDetectedFiresOncePerAnalysisHop)
{
    DroneScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f); // hop = 2205 samples at 44100 Hz
    ASSERT_TRUE(engine.loadScript(R"(
        PitchCallbacks = 0
        function OnPitchDetected(hz) PitchCallbacks = PitchCallbacks + 1 end
        function NextNotes()
            return { { note = PitchCallbacks, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 2205 * 3));

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 3.f);
}

TEST(DroneScriptEngine, PitchAnalysisGranularityChangesHopCadence)
{
    DroneScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(10.f); // hop = 441 samples at 44100 Hz
    ASSERT_TRUE(engine.loadScript(R"(
        PitchCallbacks = 0
        function OnPitchDetected(hz) PitchCallbacks = PitchCallbacks + 1 end
        function NextNotes()
            return { { note = PitchCallbacks, velocity = 0, channel = 0, length = 0, delay = 0 } }
        end
    )"));

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 4410)); // 10 hops at 441 samples each

    const auto result = engine.nextNotes();
    ASSERT_EQ(result.count, 1u);
    EXPECT_FLOAT_EQ(result.notes[0].noteHeight, 10.f);
}
