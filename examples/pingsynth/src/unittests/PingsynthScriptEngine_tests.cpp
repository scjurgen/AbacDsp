#include <cstddef>
#include <gtest/gtest.h>
#include <string>

#include "impl/PingsynthScriptEngine.h"

TEST(PingsynthScriptEngine, NoRequestsPendingByDefault)
{
    PingsynthScriptEngine engine;
    const auto pending = engine.drainSetHarmonicsRequests();
    EXPECT_EQ(pending.count, 0u);
}

TEST(PingsynthScriptEngine, SetHarmonicsIsDrainedOnceThenClears)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(
        "SetHarmonics(0, 60, 0.8, { harmonics = { { freq = 440, gain = 1, decay = 0.5, delayMs = 10 } } })"));

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    const auto& request = pending.requests[0];
    EXPECT_EQ(request.channel, 0);
    EXPECT_EQ(request.note, 60);
    EXPECT_FLOAT_EQ(request.velocity, 0.8f);
    ASSERT_EQ(request.harmonicCount, 1u);
    EXPECT_FLOAT_EQ(request.harmonics[0].freq, 440.f);
    EXPECT_FLOAT_EQ(request.harmonics[0].gain, 1.f);
    EXPECT_FLOAT_EQ(request.harmonics[0].decay, 0.5f);
    EXPECT_FLOAT_EQ(request.harmonics[0].delayMs, 10.f);

    EXPECT_EQ(engine.drainSetHarmonicsRequests().count, 0u);
}

TEST(PingsynthScriptEngine, HarmonicFieldsDefaultWhenOmitted)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetHarmonics(0, 60, 1, { harmonics = { { freq = 220 } } })"));

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    const auto& harmonic = pending.requests[0].harmonics[0];
    EXPECT_FLOAT_EQ(harmonic.gain, 1.f);
    EXPECT_FLOAT_EQ(harmonic.decay, 0.3f);
    EXPECT_FLOAT_EQ(harmonic.delayMs, 0.f);
}

TEST(PingsynthScriptEngine, AttackAndSoftExcitationDefaultToZero)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetHarmonics(0, 60, 1, { harmonics = { { freq = 220 } } })"));

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    EXPECT_FLOAT_EQ(pending.requests[0].attackMs, 0.f);
    EXPECT_FLOAT_EQ(pending.requests[0].softExcitation, 0.f);
}

TEST(PingsynthScriptEngine, AttackAndSoftExcitationAreReadWhenProvided)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(
        "SetHarmonics(0, 60, 1, { harmonics = { { freq = 220 } }, attackMs = 5, softExcitation = 0.25 })"));

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    EXPECT_FLOAT_EQ(pending.requests[0].attackMs, 5.f);
    EXPECT_FLOAT_EQ(pending.requests[0].softExcitation, 0.25f);
}

TEST(PingsynthScriptEngine, HarmonicsWithoutFreqAreSkipped)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetHarmonics(0, 60, 1, { harmonics = { { gain = 1 }, { freq = 220 } } })"));

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    ASSERT_EQ(pending.requests[0].harmonicCount, 1u);
    EXPECT_FLOAT_EQ(pending.requests[0].harmonics[0].freq, 220.f);
}

TEST(PingsynthScriptEngine, HarmonicCountIsCappedAtMaxPerVoice)
{
    PingsynthScriptEngine engine;
    std::string script = "local h = {}\n";
    for (size_t i = 0; i < PingsynthScriptEngine::kMaxHarmonicsPerVoice + 10; ++i)
    {
        script += "h[#h + 1] = { freq = 100 + #h }\n";
    }
    script += "SetHarmonics(0, 60, 1, { harmonics = h })\n";
    ASSERT_TRUE(engine.loadScript(script));

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    EXPECT_EQ(pending.requests[0].harmonicCount, PingsynthScriptEngine::kMaxHarmonicsPerVoice);
}

TEST(PingsynthScriptEngine, RequestsBeyondPerBlockCapAreDropped)
{
    PingsynthScriptEngine engine;
    std::string script;
    for (size_t i = 0; i < PingsynthScriptEngine::kMaxSetHarmonicsRequestsPerBlock + 5; ++i)
    {
        script += "SetHarmonics(0, " + std::to_string(i) + ", 1, { harmonics = { { freq = 220 } } })\n";
    }
    ASSERT_TRUE(engine.loadScript(script));

    const auto pending = engine.drainSetHarmonicsRequests();
    EXPECT_EQ(pending.count, PingsynthScriptEngine::kMaxSetHarmonicsRequestsPerBlock);
}

TEST(PingsynthScriptEngine, StubScriptFiresSetHarmonicsFromNoteOn)
{
    PingsynthScriptEngine engine;
    engine.notifyNoteOn(0, 60, 100);

    const auto pending = engine.drainSetHarmonicsRequests();
    ASSERT_EQ(pending.count, 1u);
    EXPECT_EQ(pending.requests[0].channel, 0);
    EXPECT_EQ(pending.requests[0].note, 60);
    ASSERT_GT(pending.requests[0].harmonicCount, 0u);
    EXPECT_GT(pending.requests[0].harmonics[0].gain, 0.f);
}

TEST(PingsynthScriptEngine, NotifyMpeModeFiresOnMpeModeChangedHook)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        LastMpeMode = nil
        function OnMpeModeChanged(mpeMode)
            LastMpeMode = mpeMode
        end
    )"));

    engine.notifyMpeMode(true);
    EXPECT_FALSE(engine.hasError());
}

TEST(PingsynthScriptEngine, NotifyMpeModeIsANoOpWithoutTheHook)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("-- no OnMpeModeChanged defined"));

    engine.notifyMpeMode(true);
    EXPECT_FALSE(engine.hasError());
}

TEST(PingsynthScriptEngine, PitchBendRangeDefaultsToTwelveSemitonesInMpeMode)
{
    PingsynthScriptEngine engine;
    engine.notifyMpeMode(true);
    EXPECT_FLOAT_EQ(engine.pitchBendRangeSemitones(), 12.f);
}

TEST(PingsynthScriptEngine, PitchBendRangeDefaultsToTwoSemitonesInPolyphonicMode)
{
    PingsynthScriptEngine engine;
    engine.notifyMpeMode(false);
    EXPECT_FLOAT_EQ(engine.pitchBendRangeSemitones(), 2.f);
}

TEST(PingsynthScriptEngine, SetPitchBendRangeOverridesTheDefaultInEitherMode)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetPitchBendRange(7)"));

    engine.notifyMpeMode(true);
    EXPECT_FLOAT_EQ(engine.pitchBendRangeSemitones(), 7.f);
    engine.notifyMpeMode(false);
    EXPECT_FLOAT_EQ(engine.pitchBendRangeSemitones(), 7.f);
}

TEST(PingsynthScriptEngine, LoadTimeInfiniteLoopIsRejectedWithoutHanging)
{
    PingsynthScriptEngine engine;
    EXPECT_FALSE(engine.loadScript("while true do end"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("infinite loop"), std::string::npos) << engine.lastError();
}

TEST(PingsynthScriptEngine, HandlerInfiniteLoopIsRejectedWithoutHanging)
{
    PingsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function OnNoteOn(channel, note, velocity) while true do end end"));
    engine.notifyNoteOn(0, 60, 100);
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("infinite loop"), std::string::npos) << engine.lastError();
}
