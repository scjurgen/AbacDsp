#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include "impl/ResonikScriptEngine.h"

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

TEST(ResonikScriptEngine, NoRangeCommandPendingByDefault)
{
    ResonikScriptEngine engine;
    EXPECT_FALSE(engine.drainFreqRangeCommand().has_value());
    EXPECT_FALSE(engine.drainDecayRangeCommand().has_value());
}

TEST(ResonikScriptEngine, SetFreqRangeIsDrainedOnceThenClears)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFreqRange(220, 880)"));

    const auto command = engine.drainFreqRangeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->low, 220.f);
    EXPECT_FLOAT_EQ(command->high, 880.f);
    EXPECT_FALSE(engine.drainFreqRangeCommand().has_value());
}

TEST(ResonikScriptEngine, SetDecayRangeIsDrainedOnceThenClears)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetDecayRange(0.2, 3.5)"));

    const auto command = engine.drainDecayRangeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->low, 0.2f);
    EXPECT_FLOAT_EQ(command->high, 3.5f);
    EXPECT_FALSE(engine.drainDecayRangeCommand().has_value());
}

TEST(ResonikScriptEngine, OnPitchDetectedDrivesFreqRangeFromDetectedPitch)
{
    ResonikScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);
    ASSERT_TRUE(engine.loadScript(R"(
        function OnPitchDetected(hz)
            if hz > 0 then
                SetFreqRange(hz, hz * 8)
            end
        end
    )"));

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 6000));

    const auto command = engine.drainFreqRangeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_NEAR(command->low, 220.f, 3.f);
    EXPECT_NEAR(command->high, command->low * 8.f, 24.f);
}

TEST(ResonikScriptEngine, StubScriptFollowsPitchOutOfTheBox)
{
    ResonikScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 6000));

    const auto command = engine.drainFreqRangeCommand();
    ASSERT_TRUE(command.has_value()) << "kStubScript should retune off OnPitchDetected without any script load";
    EXPECT_NEAR(command->low, 220.f, 3.f);
}

TEST(ResonikScriptEngine, StubScriptAppliesOldDialDefaultsAfterOneTick)
{
    ResonikScriptEngine engine;
    engine.setSampleRate(44100.f);
    engine.tickBlock(64); // fires the Timer.After(1, ...) deferred in kStubScript

    const auto decay = engine.drainDecayRangeCommand();
    ASSERT_TRUE(decay.has_value());
    EXPECT_FLOAT_EQ(decay->low, 0.3f);
    EXPECT_FLOAT_EQ(decay->high, 4.0f);

    const auto gain = engine.drainGainRangeCommand();
    ASSERT_TRUE(gain.has_value());
    EXPECT_FLOAT_EQ(gain->low, -18.f);
    EXPECT_FLOAT_EQ(gain->high, 0.f);

    const auto delay = engine.drainDelayRangeCommand();
    ASSERT_TRUE(delay.has_value());
    EXPECT_FLOAT_EQ(delay->low, 0.f);
    EXPECT_FLOAT_EQ(delay->high, 300.f);

    const auto q = engine.drainQCommand();
    ASSERT_TRUE(q.has_value());
    EXPECT_FLOAT_EQ(*q, 6.f);
}

TEST(ResonikScriptEngine, SetFreqRangeWithDistributionSetsTheThirdArgument)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFreqRange(100, 400, 0)"));

    const auto command = engine.drainFreqRangeCommand();
    ASSERT_TRUE(command.has_value());
    ASSERT_TRUE(command->distribution.has_value());
    EXPECT_EQ(*command->distribution, 0u);
}

TEST(ResonikScriptEngine, SetFreqRangeWithoutDistributionLeavesItUnset)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFreqRange(100, 400)"));

    const auto command = engine.drainFreqRangeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FALSE(command->distribution.has_value());
}

TEST(ResonikScriptEngine, SetGainRangeIsDrainedOnceThenClears)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetGainRange(-24, -3)"));

    const auto command = engine.drainGainRangeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->low, -24.f);
    EXPECT_FLOAT_EQ(command->high, -3.f);
    EXPECT_FALSE(engine.drainGainRangeCommand().has_value());
}

TEST(ResonikScriptEngine, SetDelayRangeIsDrainedOnceThenClears)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetDelayRange(10, 500)"));

    const auto command = engine.drainDelayRangeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->low, 10.f);
    EXPECT_FLOAT_EQ(command->high, 500.f);
    EXPECT_FALSE(engine.drainDelayRangeCommand().has_value());
}

TEST(ResonikScriptEngine, SetQIsDrainedOnceThenClears)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetQ(12.5)"));

    const auto command = engine.drainQCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(*command, 12.5f);
    EXPECT_FALSE(engine.drainQCommand().has_value());
}

TEST(ResonikScriptEngine, SetResonanceBodyOnlySetsProvidedFields)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetResonanceBody(3, { freq = 440, q = 10 })"));

    const auto& overrides = engine.bodyOverrides();
    ASSERT_TRUE(overrides[3].has_value());
    EXPECT_FLOAT_EQ(*overrides[3]->freq, 440.f);
    EXPECT_FLOAT_EQ(*overrides[3]->q, 10.f);
    EXPECT_FALSE(overrides[3]->decay.has_value());
    EXPECT_FALSE(overrides[3]->gainDb.has_value());
    EXPECT_FALSE(overrides[3]->delayMs.has_value());
}

TEST(ResonikScriptEngine, SetResonanceBodyIsPersistentNotDrained)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetResonanceBody(0, { freq = 220 })"));

    EXPECT_TRUE(engine.bodyOverrides()[0].has_value());
    EXPECT_TRUE(engine.bodyOverrides()[0].has_value()) << "reading it again must not clear it";
}

TEST(ResonikScriptEngine, SetResonanceBodyReplacesThePreviousOverrideWholesale)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(R"(
        SetResonanceBody(0, { freq = 220, decay = 1.5 })
        SetResonanceBody(0, { gainDb = -6 })
    )"));

    const auto& overrides = engine.bodyOverrides();
    ASSERT_TRUE(overrides[0].has_value());
    EXPECT_FALSE(overrides[0]->freq.has_value()) << "second call replaces, not merges";
    EXPECT_FALSE(overrides[0]->decay.has_value());
    EXPECT_FLOAT_EQ(*overrides[0]->gainDb, -6.f);
}

namespace
{
// Mirrors base-scripts/pitch-follow-resonance.lua - kept inline so a regression here is
// caught without a filesystem dependency on that file.
constexpr std::string_view kPitchFollowResonanceScript = R"(
    UICreateParameterSet({
        { id = "root", name = "Root", type = "drop",
          items = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, default = 0 },
        { id = "scale", name = "Scale", type = "drop",
          items = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone",
                    "Chromatic" },
          default = 0 },
    })

    local kScaleNames = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues",
                          "WholeTone", "Chromatic" }

    RootNote = 60
    ScaleName = kScaleNames[1]

    function OnRootChanged(index)
        RootNote = 60 + index
    end

    function OnScaleChanged(index)
        ScaleName = kScaleNames[index + 1]
    end

    local kWindowSize = 4
    local kNumWindows = 8
    WindowIndex = 0

    function OnPitchDetected(hz, confidence)
        if hz <= 0 or confidence < 0.5 then
            return
        end

        local note = Music.HarmonizeToScale(Music.HzToNote(hz), RootNote, ScaleName)
        local fundamental = Music.NoteToHz(note)

        SetFreqRange(fundamental, fundamental * 8)
        SetDecayRange(0.3, 4.0)
        SetGainRange(-18, 0)
        SetDelayRange(0, 300)
        SetQ(6)

        local firstIndex = WindowIndex * kWindowSize
        for i = 0, kWindowSize - 1 do
            local bodyIndex = firstIndex + i
            local bodyNote = Music.HarmonizeToScale(note + i * 2, RootNote, ScaleName)
            local bodyFreq = Music.NoteToHz(bodyNote) * (1 + math.floor(bodyIndex / 7))

            SetResonanceBody(bodyIndex, {
                freq    = bodyFreq,
                decay   = 0.5 + i * 0.8,
                gainDb  = -6 - i * 3,
                q       = 4 + i * 3,
                delayMs = i * 40,
            })
        end

        WindowIndex = (WindowIndex + 1) % kNumWindows
    end
)";
}

TEST(ResonikScriptEngine, PitchFollowResonanceDemoDeclaresRootAndScaleDials)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(kPitchFollowResonanceScript));

    const auto& slots = engine.uiParamSlots();
    ASSERT_TRUE(slots[0].claimed);
    EXPECT_EQ(slots[0].name, "Root");
    ASSERT_TRUE(slots[1].claimed);
    EXPECT_EQ(slots[1].name, "Scale");
}

TEST(ResonikScriptEngine, PitchFollowResonanceDemoTunesEveryFieldOfTheFirstWindow)
{
    ResonikScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);
    ASSERT_TRUE(engine.loadScript(kPitchFollowResonanceScript));

    // 220 Hz is A3 (note 57), already in C major - HarmonizeToScale should leave it be.
    // Exactly one hop (2205 samples at 50 ms/44100 Hz), so the window advances once.
    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 2205));

    const auto freqRange = engine.drainFreqRangeCommand();
    ASSERT_TRUE(freqRange.has_value());
    EXPECT_NEAR(freqRange->low, 220.f, 3.f);
    EXPECT_NEAR(freqRange->high, freqRange->low * 8.f, 24.f);
    EXPECT_TRUE(engine.drainDecayRangeCommand().has_value());
    EXPECT_TRUE(engine.drainGainRangeCommand().has_value());
    EXPECT_TRUE(engine.drainDelayRangeCommand().has_value());
    EXPECT_TRUE(engine.drainQCommand().has_value());

    const auto& overrides = engine.bodyOverrides();
    for (size_t i = 0; i < 4; ++i)
    {
        ASSERT_TRUE(overrides[i].has_value()) << "body " << i;
        const auto& body = *overrides[i];
        ASSERT_TRUE(body.freq.has_value()) << "body " << i;
        EXPECT_GT(*body.freq, 0.f) << "body " << i;
        ASSERT_TRUE(body.decay.has_value()) << "body " << i;
        EXPECT_FLOAT_EQ(*body.decay, 0.5f + static_cast<float>(i) * 0.8f);
        ASSERT_TRUE(body.gainDb.has_value()) << "body " << i;
        EXPECT_FLOAT_EQ(*body.gainDb, -6.f - static_cast<float>(i) * 3.f);
        ASSERT_TRUE(body.q.has_value()) << "body " << i;
        EXPECT_FLOAT_EQ(*body.q, 4.f + static_cast<float>(i) * 3.f);
        ASSERT_TRUE(body.delayMs.has_value()) << "body " << i;
        EXPECT_FLOAT_EQ(*body.delayMs, static_cast<float>(i) * 40.f);
    }
    EXPECT_FALSE(overrides[4].has_value()) << "second window shouldn't be touched yet";
}

TEST(ResonikScriptEngine, PitchFollowResonanceDemoWindowMovesButKeepsEarlierBodiesActive)
{
    ResonikScriptEngine engine;
    constexpr float kSampleRate = 44100.f;
    engine.setSampleRate(kSampleRate);
    engine.setPitchAnalysisGranularity(50.f);
    ASSERT_TRUE(engine.loadScript(kPitchFollowResonanceScript));

    // Two separate feeds of exactly one hop each (2205 samples), so the window advances
    // by exactly one step per feed.
    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 2205));
    ASSERT_TRUE(engine.bodyOverrides()[0].has_value());
    ASSERT_TRUE(engine.bodyOverrides()[3].has_value());

    engine.feedPitchAnalysis(makeSineBlock(kSampleRate, 220.f, 2205));

    const auto& overrides = engine.bodyOverrides();
    EXPECT_TRUE(overrides[0].has_value()) << "first window's bodies stay tuned - persistent, not drained";
    EXPECT_TRUE(overrides[3].has_value());
    ASSERT_TRUE(overrides[4].has_value()) << "second window (bodies 4..7) now tuned too";
    ASSERT_TRUE(overrides[7].has_value());
    EXPECT_FALSE(overrides[8].has_value()) << "third window shouldn't be touched yet";
}

TEST(ResonikScriptEngine, LoadTimeInfiniteLoopIsRejectedWithoutHanging)
{
    ResonikScriptEngine engine;
    EXPECT_FALSE(engine.loadScript("while true do end"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("infinite loop"), std::string::npos) << engine.lastError();
}

TEST(ResonikScriptEngine, HandlerInfiniteLoopIsRejectedWithoutHanging)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function OnNoteOn(channel, note, velocity) while true do end end"));
    engine.notifyNoteOn(0, 60, 100);
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("infinite loop"), std::string::npos) << engine.lastError();
}

TEST(ResonikScriptEngine, SetResonanceBodyOutOfRangeIndexIsIgnored)
{
    ResonikScriptEngine engine;
    ASSERT_TRUE(
        engine.loadScript("SetResonanceBody(" + std::to_string(ResonikScriptEngine::kMaxBodies) + ", { freq = 1 })"));
    EXPECT_FALSE(engine.hasError());
}
