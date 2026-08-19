#include <array>
#include <cmath>
#include <cstddef>
#include <tuple>

#include "gtest/gtest.h"

#include "impl/SpectraltapScriptEngine.h"

TEST(SpectraltapScriptEngine, NoCommandsPendingByDefault)
{
    SpectraltapScriptEngine engine;
    EXPECT_FALSE(engine.drainMaxTapsCommand().has_value());
    EXPECT_FALSE(engine.drainTapCommand(0).has_value());
    EXPECT_FALSE(engine.drainFrequencyCommand(0).has_value());
    EXPECT_FALSE(engine.drainResonanceCommand(0).has_value());
    EXPECT_FALSE(engine.drainFormantCommand(0).has_value());
    EXPECT_FALSE(engine.drainGainCommand(0).has_value());
    EXPECT_FALSE(engine.drainPanCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, NotifyTimingFiresOnTimingWithBothArguments)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("LastBpm = -1\nLastDivision = -1\n"
                                  "function OnTiming(bpm, divisionIndex)\n"
                                  "    LastBpm = bpm\n"
                                  "    LastDivision = divisionIndex\n"
                                  "end\n"));

    engine.notifyTiming(96.5f, 7);

    ASSERT_TRUE(engine.loadScript("SetGain(0, LastBpm == 96.5 and LastDivision == 7 and 2.0 or 0.0)"));
    const auto gain = engine.drainGainCommand(0);
    ASSERT_TRUE(gain.has_value());
    EXPECT_FLOAT_EQ(*gain, 2.0f);
}

TEST(SpectraltapScriptEngine, SetMaxTapsIsDrainedOnceThenClears)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetMaxTaps(10)"));

    const auto command = engine.drainMaxTapsCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(*command, 10u);
    EXPECT_FALSE(engine.drainMaxTapsCommand().has_value());
}

TEST(SpectraltapScriptEngine, SetMaxTapsClampsAboveCapacity)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetMaxTaps(999)"));

    const auto command = engine.drainMaxTapsCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(*command, SpectraltapScriptEngine::kMaxTaps);
}

TEST(SpectraltapScriptEngine, SetTapIsDrainedOnceThenClears)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTap(2, 150, 3, 0.5, -0.25)"));

    const auto command = engine.drainTapCommand(2);
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->delayMs, 150.f);
    EXPECT_EQ(command->type, TapType::BandPass);
    EXPECT_FLOAT_EQ(command->level, 0.5f);
    EXPECT_FLOAT_EQ(command->pan, -0.25f);
    EXPECT_FALSE(engine.drainTapCommand(2).has_value());
}

TEST(SpectraltapScriptEngine, SetTapAcceptsRingModulatorType)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTap(0, 0, 8, 1.0, 0.0)"));

    const auto command = engine.drainTapCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->type, TapType::RingModulator);
}

TEST(SpectraltapScriptEngine, SetTapRejectsOutOfRangeIndex)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTap(24, 100, 0, 1, 0)"));

    for (size_t i = 0; i < SpectraltapScriptEngine::kMaxTaps; ++i)
    {
        EXPECT_FALSE(engine.drainTapCommand(i).has_value()) << "index " << i;
    }
}

TEST(SpectraltapScriptEngine, SetTapRejectsUnknownType)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTap(0, 100, 9, 1, 0)"));
    EXPECT_FALSE(engine.drainTapCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetTapClampsDelayLevelPan)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTap(0, 999999, 3, 100, 5)"));

    const auto command = engine.drainTapCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->delayMs, SpectraltapScriptEngine::kMaxDelayMs);
    EXPECT_FLOAT_EQ(command->level, SpectraltapScriptEngine::kMaxGain);
    EXPECT_FLOAT_EQ(command->pan, SpectraltapScriptEngine::kMaxPan);
}

TEST(SpectraltapScriptEngine, SetTapRejectsNonFiniteDelay)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTap(0, 0/0, 3, 1, 0)"));
    EXPECT_FALSE(engine.drainTapCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetFrequencyIsDrainedOnceThenClears)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFrequency(1, 440)"));

    const auto command = engine.drainFrequencyCommand(1);
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(*command, 440.f);
    EXPECT_FALSE(engine.drainFrequencyCommand(1).has_value());
}

TEST(SpectraltapScriptEngine, SetFrequencyClampsRange)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFrequency(0, 999999)\nSetFrequency(1, -50)"));

    EXPECT_FLOAT_EQ(*engine.drainFrequencyCommand(0), SpectraltapScriptEngine::kMaxFreqHz);
    EXPECT_FLOAT_EQ(*engine.drainFrequencyCommand(1), SpectraltapScriptEngine::kMinFreqHz);
}

TEST(SpectraltapScriptEngine, SetFrequencyRejectsNonFinite)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFrequency(0, 1/0)"));
    EXPECT_FALSE(engine.drainFrequencyCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetResonanceIsDrainedOnceThenClears)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetResonance(0, 220, 2.5)"));

    const auto command = engine.drainResonanceCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->freqHz, 220.f);
    EXPECT_FLOAT_EQ(command->decaySeconds, 2.5f);
    EXPECT_FALSE(command->negative);
    EXPECT_FALSE(engine.drainResonanceCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetResonanceAcceptsOptionalNegativeFlag)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetResonance(0, 220, 2.5, true)"));

    const auto command = engine.drainResonanceCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_TRUE(command->negative);
}

TEST(SpectraltapScriptEngine, SetResonanceClampsDecay)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetResonance(0, 220, -5)\nSetResonance(1, 220, 999)"));

    EXPECT_FLOAT_EQ(engine.drainResonanceCommand(0)->decaySeconds, SpectraltapScriptEngine::kMinDecaySeconds);
    EXPECT_FLOAT_EQ(engine.drainResonanceCommand(1)->decaySeconds, SpectraltapScriptEngine::kMaxDecaySeconds);
}

TEST(SpectraltapScriptEngine, SetFormantIsDrainedOnceThenClears)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFormant(0, 200, 2.0, 0.6, 3.5, 0.35)"));

    const auto command = engine.drainFormantCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->freqHz, 200.f);
    EXPECT_FLOAT_EQ(command->f1Factor, 2.0f);
    EXPECT_FLOAT_EQ(command->f1Gain, 0.6f);
    EXPECT_FLOAT_EQ(command->f2Factor, 3.5f);
    EXPECT_FLOAT_EQ(command->f2Gain, 0.35f);
    EXPECT_FALSE(engine.drainFormantCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetFormantRejectsNonFinite)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFormant(0, 0/0, 2.0, 0.6, 3.5, 0.35)"));
    EXPECT_FALSE(engine.drainFormantCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetPanIsDrainedOnceThenClearsAndClamps)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetPan(0, -5)\nSetPan(1, 5)"));

    EXPECT_FLOAT_EQ(*engine.drainPanCommand(0), SpectraltapScriptEngine::kMinPan);
    EXPECT_FLOAT_EQ(*engine.drainPanCommand(1), SpectraltapScriptEngine::kMaxPan);
    EXPECT_FALSE(engine.drainPanCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetGainIsDrainedOnceThenClearsAndClamps)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetGain(0, -5)\nSetGain(1, 999)"));

    EXPECT_FLOAT_EQ(*engine.drainGainCommand(0), SpectraltapScriptEngine::kMinGain);
    EXPECT_FLOAT_EQ(*engine.drainGainCommand(1), SpectraltapScriptEngine::kMaxGain);
    EXPECT_FALSE(engine.drainGainCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, SetTapFeedbackIsDrainedOnceThenClearsAndClamps)
{
    SpectraltapScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetTapFeedback(0, -5)\nSetTapFeedback(1, 5)"));

    EXPECT_FLOAT_EQ(*engine.drainTapFeedbackCommand(0), SpectraltapScriptEngine::kMinTapFeedback);
    EXPECT_FLOAT_EQ(*engine.drainTapFeedbackCommand(1), SpectraltapScriptEngine::kMaxTapFeedback);
    EXPECT_FALSE(engine.drainTapFeedbackCommand(0).has_value());
}

TEST(SpectraltapScriptEngine, StubScriptClaimsRootAndScaleLuaControls)
{
    SpectraltapScriptEngine engine;

    const auto& slots = engine.uiParamSlots();
    size_t claimedCount = 0;
    bool foundRoot = false;
    bool foundScale = false;
    for (const auto& slot : slots)
    {
        if (!slot.claimed)
        {
            continue;
        }
        ++claimedCount;
        foundRoot = foundRoot || slot.id == "root";
        foundScale = foundScale || slot.id == "scale";
    }
    EXPECT_EQ(claimedCount, 2u);
    EXPECT_TRUE(foundRoot);
    EXPECT_TRUE(foundScale);
}

TEST(SpectraltapScriptEngine, RootChangeRetunesTapsUsingLastKnownTiming)
{
    SpectraltapScriptEngine engine;
    engine.notifyTiming(100.f, 4);
    for (size_t i = 0; i < 4; ++i)
    {
        std::ignore = engine.drainTapCommand(i);
        std::ignore = engine.drainResonanceCommand(i);
        std::ignore = engine.drainFormantCommand(i);
    }

    // Slot 0 is "root". notifyUiParameterChanged() takes a normalized 0..1 value, mapped
    // through the slot's range before OnRootChanged sees it - 1.0 selects the last item.
    engine.notifyUiParameterChanged(0, 1.f);

    const auto tap0 = engine.drainTapCommand(0);
    ASSERT_TRUE(tap0.has_value()) << "raising Root should re-run RetuneTaps()";
    const auto resonance0 = engine.drainResonanceCommand(0);
    ASSERT_TRUE(resonance0.has_value());
    EXPECT_GT(resonance0->freqHz, 0.f);
}

TEST(SpectraltapScriptEngine, StubScriptConfiguresFourTapsOnTiming)
{
    SpectraltapScriptEngine engine;
    engine.notifyTiming(120.f, 4); // "1/4" - what SpectraltapImpl fires on its first block

    const auto maxTaps = engine.drainMaxTapsCommand();
    ASSERT_TRUE(maxTaps.has_value());
    EXPECT_EQ(*maxTaps, 4u);

    constexpr std::array<TapType, 4> expectedTypes{TapType::BandPass, TapType::Resonator, TapType::Formant,
                                                   TapType::Notch};
    constexpr std::array<float, 4> expectedPans{-0.6f, 0.6f, -0.6f, 0.6f};
    for (size_t i = 0; i < 4; ++i)
    {
        const auto tap = engine.drainTapCommand(i);
        ASSERT_TRUE(tap.has_value()) << "tap " << i;
        EXPECT_EQ(tap->type, expectedTypes[i]) << "tap " << i;
        EXPECT_FLOAT_EQ(tap->pan, expectedPans[i]) << "tap " << i;
        EXPECT_FLOAT_EQ(tap->level, 0.8f) << "tap " << i;
        EXPECT_GT(tap->delayMs, 0.f) << "tap " << i;
        EXPECT_TRUE(std::isfinite(tap->delayMs)) << "tap " << i;
    }

    const auto formant = engine.drainFormantCommand(2);
    ASSERT_TRUE(formant.has_value());
    EXPECT_GT(formant->freqHz, 0.f);

    const auto resonance = engine.drainResonanceCommand(0);
    ASSERT_TRUE(resonance.has_value());
    EXPECT_GT(resonance->freqHz, 0.f);
}
