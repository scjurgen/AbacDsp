#include <algorithm>
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

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

TEST(DrumTrainerImplTest, ScriptPatternIsNotPlayedAndKeepsTheClick)
{
    Impl withPattern(kSampleRate);
    Impl withoutPattern(kSampleRate);
    ASSERT_TRUE(withPattern.setScript("ClearPattern()\nAddHit(0.25, Crash, 127)\n"));
    static_cast<void>(runSeconds(withPattern, 0.01f));
    static_cast<void>(runSeconds(withoutPattern, 0.01f));
    withPattern.setOnOff(true);
    withoutPattern.setOnOff(true);
    EXPECT_FLOAT_EQ(runSeconds(withPattern, 2.f), runSeconds(withoutPattern, 2.f));
}

TEST(DrumTrainerImplTest, FailedScriptReportsAnErrorAndReturnsFalse)
{
    Impl impl(kSampleRate);
    EXPECT_FALSE(impl.setScript("AddHit(99, Kick)\n"));
    EXPECT_TRUE(impl.hasScriptError());
    EXPECT_THAT(impl.scriptError(), ::testing::HasSubstr("outside the pattern"));
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
    ASSERT_TRUE(impl.setScript("AddHit(3.5, Kick)\n"));
    impl.setPreset(kPresetThreeFour);
    impl.reloadScriptIfPending();
    EXPECT_TRUE(impl.hasScriptError());
}

TEST(DrumTrainerImplTest, ReloadDoesNothingWithoutAPresetChange)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddHit(1, Kick)\n"));
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
}

TEST(DrumTrainerImplTest, ReloadSucceedsWhenTheScriptFitsTheNewBar)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddHit(BarBeats - 1, Kick)\n"));
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

TEST(DrumTrainerImplTest, ScriptBarLengthClicksOnBeatsBeyondThePresetBar)
{
    Impl impl(kSampleRate);
    impl.setPreset(kPresetThreeFour);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    static_cast<void>(runSeconds(impl, 2.4f));
    EXPECT_GT(runSeconds(impl, 0.2f), 0.001f);
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

TEST(DrumTrainerImplTest, PatternBarsComeFromTheAdoptedScript)
{
    Impl impl(kSampleRate);
    EXPECT_EQ(impl.patternBars(), 1u);
    ASSERT_TRUE(impl.setScript("SetPatternBars(3)\nAddHit(8, Snare)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    EXPECT_EQ(impl.patternBars(), 3u);
    EXPECT_EQ(impl.patternBarIndex(), 0u);
}

TEST(DrumTrainerImplTest, PatternBarIndexAdvancesEveryBarAndWraps)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetPatternBars(2)\nAddHit(0, Kick)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    static_cast<void>(runSeconds(impl, 2.1f));
    EXPECT_EQ(impl.patternBarIndex(), 1u);
    static_cast<void>(runSeconds(impl, 2.f));
    EXPECT_EQ(impl.patternBarIndex(), 0u);
}

TEST(DrumTrainerImplTest, CaptureStartsOnTheFirstPatternBar)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetPatternBars(3)\nAddHit(0, Kick)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setAnalysisMode(true);
    static_cast<void>(runSeconds(impl, 4.5f));
    ASSERT_EQ(impl.analysisStatusText(), "Analysis: bar 1");
    EXPECT_EQ(impl.patternBarIndex(), 0u);
}

namespace
{
struct TimedMidi
{
    size_t sample{0};
    std::array<uint8_t, 3> bytes{};
};

constexpr size_t kBarSamples{96000};
constexpr size_t kBeatSamples{24000};
constexpr std::array<uint8_t, 3> kKick{0x99, 74, 100};
constexpr std::array<uint8_t, 3> kSnare{0x99, 60, 100};

// Sample of a beat in the captured bars, counted from the moment Analysis is switched on
// (120 BPM, 4/4, two count-in bars).
size_t captureSample(const size_t bar, const size_t beat)
{
    return (2 + bar) * kBarSamples + beat * kBeatSamples;
}

// Switches Analysis on and runs `seconds`, handing each event over in the block it falls into.
void runTake(Impl& impl, std::vector<TimedMidi> events, const float seconds)
{
    std::ranges::sort(events, {}, &TimedMidi::sample);
    impl.setAnalysisMode(true);
    const Buffer silence{};
    Buffer out{};
    auto next = events.begin();
    const auto numBlocks = static_cast<size_t>(seconds * kSampleRate) / kBlock;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const size_t start = block * kBlock;
        for (; next != events.end() && next->sample < start + kBlock; ++next)
        {
            impl.processMidiEvent(next->bytes, static_cast<int>(next->sample - start));
        }
        impl.processBlock(silence, out);
    }
}

std::string reportCard(const std::string& label, const std::string& value)
{
    return label + "</h6><p class=\"fs-3 mb-0\">" + value + "</p>";
}

Impl& prepareKickTake(Impl& impl)
{
    EXPECT_TRUE(impl.setScript("AddHit(0, Kick)\nAddHit(2, Kick)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setAnalysisBars(1);
    return impl;
}
}

TEST(DrumTrainerImplTest, ATakeIsMatchedAgainstThePattern)
{
    Impl impl(kSampleRate);
    std::vector<TimedMidi> events;
    for (size_t bar = 0; bar < 4; ++bar)
    {
        events.push_back({captureSample(bar, 0) + (bar == 1 ? 480u : 0u), kKick});
        if (bar != 3)
        {
            events.push_back({captureSample(bar, 2), kKick});
        }
    }
    events.push_back({captureSample(2, 1), kSnare});
    runTake(prepareKickTake(impl), events, 12.1f);
    ASSERT_TRUE(impl.consumeAnalysisReportReady());
    const std::string html = impl.buildAnalysisReportHtml();
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Hit", "7 / 8")));
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Missed", "1")));
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Extra", "1")));
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Bars", "4")));
    EXPECT_THAT(html, ::testing::HasSubstr("<td>Beat 1</td><td>Kick</td><td>4</td><td>0</td>"));
    EXPECT_THAT(html, ::testing::HasSubstr("2.5 ms late"));
    EXPECT_THAT(html, ::testing::HasSubstr("<td>Beat 3</td><td>Kick</td><td>3</td><td>1</td>"));
}

TEST(DrumTrainerImplTest, LatencyCompensationShiftsEveryCapturedHit)
{
    Impl impl(kSampleRate);
    impl.setLatencyComp(10.f);
    std::vector<TimedMidi> events;
    for (size_t bar = 0; bar < 4; ++bar)
    {
        events.push_back({captureSample(bar, 0) + 480, kKick});
        events.push_back({captureSample(bar, 2) + 480, kKick});
    }
    runTake(prepareKickTake(impl), events, 12.1f);
    ASSERT_TRUE(impl.consumeAnalysisReportReady());
    const std::string html = impl.buildAnalysisReportHtml();
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Hit", "8 / 8")));
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Mean deviation", "on beat")));
}

TEST(DrumTrainerImplTest, AnEarlyFirstHitInTheCountInCounts)
{
    Impl impl(kSampleRate);
    std::vector<TimedMidi> events{{captureSample(0, 0) - 480, kKick}, {kBarSamples, kSnare}};
    runTake(prepareKickTake(impl), events, 12.1f);
    ASSERT_TRUE(impl.consumeAnalysisReportReady());
    const std::string html = impl.buildAnalysisReportHtml();
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Hit", "1 / 8")));
    EXPECT_THAT(html, ::testing::HasSubstr(reportCard("Extra", "0")));
    EXPECT_THAT(html, ::testing::HasSubstr("10.0 ms early"));
}

TEST(DrumTrainerImplTest, SwitchingOffDuringTheCountInWritesNoReport)
{
    Impl impl(kSampleRate);
    runTake(prepareKickTake(impl), {}, 1.f);
    impl.setAnalysisMode(false);
    static_cast<void>(runSeconds(impl, 0.01f));
    EXPECT_FALSE(impl.consumeAnalysisReportReady());
}

TEST(DrumTrainerImplTest, ReportNamesTheScriptInTheFileStem)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Two-Bar Fill')\nAddHit(0, Kick)\n"));
    EXPECT_EQ(impl.analysisFileStem(), "two-bar-fill");
    EXPECT_THAT(impl.buildAnalysisReportHtml(), ::testing::HasSubstr("DrumTrainer Timing Analysis - Two-Bar Fill"));
}
