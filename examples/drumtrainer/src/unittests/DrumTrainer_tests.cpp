#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <thread>

#include "impl/DrumTrainerImpl.h"

namespace
{
constexpr size_t kBlock{16};
constexpr float kSampleRate{48000.f};
constexpr int kPresetThreeFour{0};
constexpr int kPresetFourFourEighth{6};

using Impl = DrumTrainerImpl<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;

// Runs the impl on silent input and returns the loudest output sample.
float runSeconds(Impl& impl, const float seconds)
{
    const Buffer silence{};
    Buffer out{};
    float peak = 0.f;
    const auto numBlocks = static_cast<size_t>(seconds * kSampleRate) / kBlock;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        impl.processBlock(silence, out);
        for (size_t i = 0; i < kBlock; ++i)
        {
            peak = std::max({peak, std::abs(out(i, 0)), std::abs(out(i, 1))});
        }
    }
    return peak;
}

// The drum kit loads on a background thread; keep playing bars until a hit is audible.
float runUntilAudible(Impl& impl, const float barSeconds)
{
    float peak = 0.f;
    for (int attempt = 0; attempt < 100 && peak < 0.001f; ++attempt)
    {
        peak = runSeconds(impl, barSeconds);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return peak;
}

}

TEST(DrumTrainerImplTest, BuiltInClickSoundsWhenRunningWithoutAScript)
{
    Impl impl(kSampleRate);
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(DrumTrainerImplTest, EmptyScriptKeepsTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("-- nothing\n"));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(DrumTrainerImplTest, ClearPatternSilencesTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("ClearPattern()\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_EQ(runSeconds(impl, 2.f), 0.f);
}

TEST(DrumTrainerImplTest, ScriptedKickIsAudible)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddInstrument(0, Kick, 0)\nAddInstrument(2, Kick, 0)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_GT(runUntilAudible(impl, 2.f), 0.001f);
}

TEST(DrumTrainerImplTest, ScriptedLevelScalesTheOutput)
{
    Impl loud(kSampleRate);
    Impl quiet(kSampleRate);
    ASSERT_TRUE(loud.setScript("AddInstrument(0, Kick, 0)\n"));
    ASSERT_TRUE(quiet.setScript("AddInstrument(0, Kick, -20)\n"));
    static_cast<void>(runSeconds(loud, 0.01f));
    static_cast<void>(runSeconds(quiet, 0.01f));
    loud.setOnOff(true);
    quiet.setOnOff(true);
    const float loudPeak = runUntilAudible(loud, 2.f);
    const float quietPeak = runUntilAudible(quiet, 2.f);
    ASSERT_GT(quietPeak, 0.f);
    EXPECT_NEAR(loudPeak / quietPeak, 10.f, 1.f);
}

TEST(DrumTrainerImplTest, FailedScriptReportsAnErrorAndReturnsFalse)
{
    Impl impl(kSampleRate);
    EXPECT_FALSE(impl.setScript("AddInstrument(99, Kick, 0)\n"));
    EXPECT_TRUE(impl.hasScriptError());
    EXPECT_THAT(impl.scriptError(), ::testing::HasSubstr("outside the bar"));
}

TEST(DrumTrainerImplTest, FailedScriptDoesNotSilenceTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_FALSE(impl.setScript("ClearPattern()\nthis is not lua\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(DrumTrainerImplTest, PresetChangeReloadsTheScriptForTheNewBarLength)
{
    Impl impl(kSampleRate);
    impl.setPreset(kPresetFourFourEighth);
    ASSERT_TRUE(impl.setScript("AddInstrument(3.5, Kick, 0)\n"));
    impl.setPreset(kPresetThreeFour);
    impl.reloadScriptIfPending();
    EXPECT_TRUE(impl.hasScriptError());
}

TEST(DrumTrainerImplTest, ReloadDoesNothingWithoutAPresetChange)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddInstrument(1, Kick, 0)\n"));
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
}

TEST(DrumTrainerImplTest, ReloadSucceedsWhenTheScriptFitsTheNewBar)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddInstrument(BarBeats - 1, Kick, 0)\n"));
    impl.setPreset(kPresetThreeFour);
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
}

TEST(DrumTrainerImplTest, ScriptBarLengthReplacesThePresetBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_EQ(impl.getBarBeats(), 4);
    EXPECT_FALSE(impl.scriptSetsBeatsPerBar());
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    EXPECT_TRUE(impl.scriptSetsBeatsPerBar());
    EXPECT_EQ(impl.getBarBeats(), 7);
}

TEST(DrumTrainerImplTest, ScriptBarLengthIsAdoptedByTheSequencer)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(2)\n"));
    static_cast<void>(runSeconds(impl, 0.76f));
    EXPECT_NEAR(impl.getBarPhase(), 0.75f, 0.05f);
}

TEST(DrumTrainerImplTest, SetBeatsPerBarSilencesTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_EQ(runSeconds(impl, 2.f), 0.f);
}

TEST(DrumTrainerImplTest, PresetChangeDoesNotOverrideAScriptBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    impl.setPreset(kPresetThreeFour);
    EXPECT_EQ(impl.getBarBeats(), 7);
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
    EXPECT_EQ(impl.getBarBeats(), 7);
}

TEST(DrumTrainerImplTest, ScriptWithoutSetBeatsPerBarFallsBackToThePresetBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    ASSERT_TRUE(impl.setScript("-- nothing\n"));
    EXPECT_FALSE(impl.scriptSetsBeatsPerBar());
    EXPECT_EQ(impl.getBarBeats(), 4);
    impl.setPreset(kPresetThreeFour);
    EXPECT_EQ(impl.getBarBeats(), 3);
}

TEST(DrumTrainerImplTest, FailedScriptKeepsThePreviousScriptBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    ASSERT_FALSE(impl.setScript("SetBeatsPerBar(9)\nthis is not lua\n"));
    EXPECT_EQ(impl.getBarBeats(), 7);
}

TEST(DrumTrainerImplTest, ScriptGlobalBarBeatsStartsAtThePresetLength)
{
    Impl impl(kSampleRate);
    impl.setPreset(kPresetThreeFour);
    EXPECT_TRUE(impl.setScript("if BarBeats ~= 3 then error('wrong bar length') end\n"));
    EXPECT_FALSE(impl.scriptSetsBeatsPerBar());
}

TEST(DrumTrainerImplTest, SkeletonScriptCompilesAndDefinesNoPattern)
{
    Impl impl(kSampleRate);
    EXPECT_TRUE(impl.setScript(Impl::scriptSkeleton()));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(DrumTrainerImplTest, AnalysisStopsItselfAfterTheChosenBarCount)
{
    Impl impl(kSampleRate);
    impl.setAnalysisBars(1);
    impl.setAnalysisMode(true);
    static_cast<void>(runSeconds(impl, 11.9f));
    EXPECT_FALSE(impl.consumeAnalysisAutoStopped());
    static_cast<void>(runSeconds(impl, 0.3f));
    EXPECT_TRUE(impl.consumeAnalysisAutoStopped());
    EXPECT_FALSE(impl.consumeAnalysisAutoStopped());
    EXPECT_TRUE(impl.analysisStatusText().empty());
}

TEST(DrumTrainerImplTest, AnalysisWithOpenEndKeepsCapturingUntilSwitchedOff)
{
    Impl impl(kSampleRate);
    impl.setAnalysisMode(true);
    static_cast<void>(runSeconds(impl, 21.f));
    EXPECT_FALSE(impl.consumeAnalysisAutoStopped());
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: bar 9");
}

TEST(DrumTrainerImplTest, AnalysisStatusTextFollowsCountInAndCapture)
{
    Impl impl(kSampleRate);
    EXPECT_TRUE(impl.analysisStatusText().empty());
    impl.setAnalysisBars(2);
    impl.setAnalysisMode(true);
    static_cast<void>(runSeconds(impl, 1.f));
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: count-in bar 1/2");
    static_cast<void>(runSeconds(impl, 2.f));
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: count-in bar 2/2");
    static_cast<void>(runSeconds(impl, 2.f));
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: bar 1/8");
}

TEST(DrumTrainerImplTest, AnalysisCountInIsAudibleWithStartOffAndCaptureIsNot)
{
    Impl impl(kSampleRate);
    impl.setAnalysisMode(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
    static_cast<void>(runSeconds(impl, 3.2f));
    EXPECT_EQ(runSeconds(impl, 1.f), 0.f);
}

TEST(DrumTrainerImplTest, SwitchingAnalysisOffDuringTheCountInEndsTheTake)
{
    Impl impl(kSampleRate);
    impl.setAnalysisMode(true);
    static_cast<void>(runSeconds(impl, 1.f));
    impl.setAnalysisMode(false);
    static_cast<void>(runSeconds(impl, 0.01f));
    EXPECT_TRUE(impl.analysisStatusText().empty());
}

TEST(DrumTrainerImplTest, AnalysisUnderHostSyncWaitsForTheNextBarBeforeTheCountIn)
{
    Impl impl(kSampleRate);
    impl.setHostSync(true);
    impl.setAnalysisMode(true);
    static_cast<void>(runSeconds(impl, 0.5f));
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: waiting for the next bar");
    static_cast<void>(runSeconds(impl, 2.f));
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: count-in bar 1/2");
}

TEST(DrumTrainerImplTest, SetNameStoresTheScriptName)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Bossa Nova')\n"));
    EXPECT_EQ(impl.scriptName(), "Bossa Nova");
}

TEST(DrumTrainerImplTest, DisplayNameFallsBackToThePresetThenToScript)
{
    Impl impl(kSampleRate);
    EXPECT_EQ(impl.displayName(), "4/4 8th");
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    EXPECT_EQ(impl.displayName(), "Script");
    ASSERT_TRUE(impl.setScript("SetName('Seven')\nSetBeatsPerBar(7)\n"));
    EXPECT_EQ(impl.displayName(), "Seven");
}

TEST(DrumTrainerImplTest, ScriptWithoutSetNameClearsThePreviousName)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Rock')\n"));
    ASSERT_TRUE(impl.setScript("-- nothing\n"));
    EXPECT_TRUE(impl.scriptName().empty());
}

TEST(DrumTrainerImplTest, FailedScriptKeepsThePreviousName)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Rock')\n"));
    ASSERT_FALSE(impl.setScript("SetName('Jazz')\nthis is not lua\n"));
    EXPECT_EQ(impl.scriptName(), "Rock");
}

namespace
{
void sendMidi(Impl& impl, const std::array<uint8_t, 3> bytes, const int sampleOffset = 0)
{
    impl.processMidiEvent(bytes, sampleOffset);
}
}

TEST(DrumTrainerImplTest, MonitorWaitsForInputWithTheBuiltInMap)
{
    Impl impl(kSampleRate);
    EXPECT_EQ(impl.midiMonitorText(), "Drum map: Built-in default | waiting for MIDI input");
}

TEST(DrumTrainerImplTest, MonitorShowsTheMappedNoteAndTheDecodedHit)
{
    Impl impl(kSampleRate);
    sendMidi(impl, {0x99, 60, 100});
    EXPECT_EQ(impl.midiMonitorText(),
              "Drum map: Built-in default | MIDI in: note 60 (C4) vel 100 ch 10 = Snare | Last hit: Snare vel 100");
}

TEST(DrumTrainerImplTest, MonitorShowsTheHihatArticulationAfterTheGraceWindow)
{
    Impl impl(kSampleRate);
    sendMidi(impl, {0xB9, 4, 0});
    sendMidi(impl, {0x99, 64, 90}, 5);
    EXPECT_THAT(impl.midiMonitorText(), ::testing::Not(::testing::HasSubstr("Last hit")));
    static_cast<void>(runSeconds(impl, 0.01f));
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("Last hit: Hihat open vel 90, pedal 0"));
}

TEST(DrumTrainerImplTest, MonitorFlagsUnmappedNotesAndThePedalCc)
{
    Impl impl(kSampleRate);
    sendMidi(impl, {0x99, 61, 100});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("note 61 (C#4) vel 100 ch 10 (not mapped)"));
    sendMidi(impl, {0xB9, 4, 127});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("CC 4 = 127 ch 10 (hihat pedal)"));
    sendMidi(impl, {0xA9, 64, 50});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("CC 4 = 127"));
}

TEST(DrumTrainerImplTest, LoadedDrumMapReplacesTheNoteMapping)
{
    Impl impl(kSampleRate);
    ASSERT_FALSE(impl.setDrumMapJson(R"({"name": "GM", "inputChannel": 10, "notes": {"snare": 38}})").has_value());
    EXPECT_EQ(impl.drumMapName(), "GM");
    sendMidi(impl, {0x99, 38, 100});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("note 38 (D2) vel 100 ch 10 = Snare | Last hit: Snare"));
    sendMidi(impl, {0x91, 38, 100});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("ch 2 (channel ignored)"));
}

TEST(DrumTrainerImplTest, InvalidDrumMapKeepsTheCurrentOne)
{
    Impl impl(kSampleRate);
    const auto error = impl.setDrumMapJson(R"({"notes": {"bass": 36}})");
    ASSERT_TRUE(error.has_value());
    EXPECT_THAT(*error, ::testing::HasSubstr("unknown key \"bass\""));
    EXPECT_EQ(impl.drumMapName(), "Built-in default");
    sendMidi(impl, {0x99, 74, 100});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("Last hit: Kick vel 100"));
}

TEST(DrumTrainerImplTest, UseDefaultDrumMapRestoresTheBuiltInMap)
{
    Impl impl(kSampleRate);
    ASSERT_FALSE(impl.setDrumMapJson(R"({"name": "GM", "notes": {"kick": 36}})").has_value());
    impl.useDefaultDrumMap();
    EXPECT_EQ(impl.drumMapName(), "Built-in default");
    sendMidi(impl, {0x99, 74, 100});
    EXPECT_THAT(impl.midiMonitorText(), ::testing::HasSubstr("= Kick"));
}
