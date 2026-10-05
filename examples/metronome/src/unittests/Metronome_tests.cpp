#include <algorithm>
#include <chrono>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <thread>

#include "impl/MetronomeImpl.h"

namespace
{
constexpr size_t kBlock{16};
constexpr float kSampleRate{48000.f};
constexpr int kPresetThreeFour{0};
constexpr int kPresetFourFourEighth{6};

using Impl = MetronomeImpl<kBlock>;
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

// Feeds a loud burst on every beat at the default 120 BPM, so each beat yields one onset.
void runWithBeatBursts(Impl& impl, const float seconds, size_t& samplePos)
{
    constexpr size_t kBeatSamples{24000};
    constexpr size_t kBurstSamples{200};
    Buffer in{};
    Buffer out{};
    const auto numBlocks = static_cast<size_t>(seconds * kSampleRate) / kBlock;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        for (size_t i = 0; i < kBlock; ++i)
        {
            const float value = (samplePos % kBeatSamples) < kBurstSamples ? 0.5f : 0.f;
            in(i, 0) = value;
            in(i, 1) = value;
            ++samplePos;
        }
        impl.processBlock(in, out);
    }
}

size_t reportedHits(const std::string& html)
{
    const std::string marker{"Hits</h6><p class=\"fs-3 mb-0\">"};
    const auto pos = html.find(marker);
    return pos == std::string::npos ? std::string::npos : std::stoul(html.substr(pos + marker.size()));
}
}

TEST(MetronomeImplTest, BuiltInClickSoundsWhenRunningWithoutAScript)
{
    Impl impl(kSampleRate);
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(MetronomeImplTest, EmptyScriptKeepsTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("-- nothing\n"));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(MetronomeImplTest, ClearPatternSilencesTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("ClearPattern()\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_EQ(runSeconds(impl, 2.f), 0.f);
}

TEST(MetronomeImplTest, ScriptedKickIsAudible)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddInstrument(0, Kick, 0)\nAddInstrument(2, Kick, 0)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_GT(runUntilAudible(impl, 2.f), 0.001f);
}

TEST(MetronomeImplTest, ScriptedLevelScalesTheOutput)
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

TEST(MetronomeImplTest, FailedScriptReportsAnErrorAndReturnsFalse)
{
    Impl impl(kSampleRate);
    EXPECT_FALSE(impl.setScript("AddInstrument(99, Kick, 0)\n"));
    EXPECT_TRUE(impl.hasScriptError());
    EXPECT_THAT(impl.scriptError(), ::testing::HasSubstr("outside the bar"));
}

TEST(MetronomeImplTest, FailedScriptDoesNotSilenceTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_FALSE(impl.setScript("ClearPattern()\nthis is not lua\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(MetronomeImplTest, PresetChangeReloadsTheScriptForTheNewBarLength)
{
    Impl impl(kSampleRate);
    impl.setPreset(kPresetFourFourEighth);
    ASSERT_TRUE(impl.setScript("AddInstrument(3.5, Kick, 0)\n"));
    impl.setPreset(kPresetThreeFour);
    impl.reloadScriptIfPending();
    EXPECT_TRUE(impl.hasScriptError());
}

TEST(MetronomeImplTest, ReloadDoesNothingWithoutAPresetChange)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddInstrument(1, Kick, 0)\n"));
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
}

TEST(MetronomeImplTest, ReloadSucceedsWhenTheScriptFitsTheNewBar)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddInstrument(BarBeats - 1, Kick, 0)\n"));
    impl.setPreset(kPresetThreeFour);
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
}

TEST(MetronomeImplTest, ReportUsesTheDropdownGridWithoutScriptPositions)
{
    Impl impl(kSampleRate);
    const std::string html = impl.buildAnalysisReportHtml();
    EXPECT_THAT(html, ::testing::HasSubstr("Quarter"));
    EXPECT_THAT(html, ::testing::Not(::testing::HasSubstr("Script (")));
}

TEST(MetronomeImplTest, ReportUsesScriptAnalysisPositionsWhenDefined)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("AddAnalysisPosition(0)\nAddAnalysisPosition(1.5)\n"));
    const std::string html = impl.buildAnalysisReportHtml();
    EXPECT_THAT(html, ::testing::HasSubstr("Script (2 positions)"));
    EXPECT_THAT(html, ::testing::HasSubstr("Beat 2 + 0.5"));
}

TEST(MetronomeImplTest, EmptyScriptAnalysisFallsBackToTheDropdownGrid)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("ClearAnalysis()\n"));
    EXPECT_THAT(impl.buildAnalysisReportHtml(), ::testing::HasSubstr("Quarter"));
}

TEST(MetronomeImplTest, ScriptBarLengthReplacesThePresetBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_EQ(impl.getBarBeats(), 4);
    EXPECT_FALSE(impl.scriptSetsBeatsPerBar());
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    EXPECT_TRUE(impl.scriptSetsBeatsPerBar());
    EXPECT_EQ(impl.getBarBeats(), 7);
}

TEST(MetronomeImplTest, ScriptBarLengthIsAdoptedByTheSequencer)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(2)\n"));
    static_cast<void>(runSeconds(impl, 0.76f));
    EXPECT_NEAR(impl.getBarPhase(), 0.75f, 0.05f);
}

TEST(MetronomeImplTest, SetBeatsPerBarSilencesTheBuiltInClick)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    static_cast<void>(runSeconds(impl, 0.01f));
    impl.setOnOff(true);
    EXPECT_EQ(runSeconds(impl, 2.f), 0.f);
}

TEST(MetronomeImplTest, PresetChangeDoesNotOverrideAScriptBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    impl.setPreset(kPresetThreeFour);
    EXPECT_EQ(impl.getBarBeats(), 7);
    impl.reloadScriptIfPending();
    EXPECT_FALSE(impl.hasScriptError());
    EXPECT_EQ(impl.getBarBeats(), 7);
}

TEST(MetronomeImplTest, ScriptWithoutSetBeatsPerBarFallsBackToThePresetBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    ASSERT_TRUE(impl.setScript("-- nothing\n"));
    EXPECT_FALSE(impl.scriptSetsBeatsPerBar());
    EXPECT_EQ(impl.getBarBeats(), 4);
    impl.setPreset(kPresetThreeFour);
    EXPECT_EQ(impl.getBarBeats(), 3);
}

TEST(MetronomeImplTest, FailedScriptKeepsThePreviousScriptBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    ASSERT_FALSE(impl.setScript("SetBeatsPerBar(9)\nthis is not lua\n"));
    EXPECT_EQ(impl.getBarBeats(), 7);
}

TEST(MetronomeImplTest, ScriptGlobalBarBeatsStartsAtThePresetLength)
{
    Impl impl(kSampleRate);
    impl.setPreset(kPresetThreeFour);
    EXPECT_TRUE(impl.setScript("if BarBeats ~= 3 then error('wrong bar length') end\n"));
    EXPECT_FALSE(impl.scriptSetsBeatsPerBar());
}

TEST(MetronomeImplTest, ReportUsesTheScriptBarLength)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    const std::string html = impl.buildAnalysisReportHtml();
    EXPECT_THAT(html, ::testing::HasSubstr("Beat 7"));
    EXPECT_THAT(html, ::testing::Not(::testing::HasSubstr("Beat 8")));
}

TEST(MetronomeImplTest, SkeletonScriptCompilesAndDefinesNoPattern)
{
    Impl impl(kSampleRate);
    EXPECT_TRUE(impl.setScript(Impl::scriptSkeleton()));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}

TEST(MetronomeImplTest, AnalysisIgnoresOnsetsDuringTheTwoBarCountIn)
{
    Impl impl(kSampleRate);
    size_t samplePos{0};
    impl.setAnalysisMode(true);
    runWithBeatBursts(impl, 6.f, samplePos);
    impl.setAnalysisMode(false);
    runWithBeatBursts(impl, 0.01f, samplePos);
    ASSERT_TRUE(impl.consumeAnalysisReportReady());
    EXPECT_EQ(reportedHits(impl.buildAnalysisReportHtml()), 4u);
}

TEST(MetronomeImplTest, AnalysisStopsItselfAfterTheChosenBarCount)
{
    Impl impl(kSampleRate);
    size_t samplePos{0};
    impl.setAnalysisBars(1);
    impl.setAnalysisMode(true);
    runWithBeatBursts(impl, 11.9f, samplePos);
    EXPECT_FALSE(impl.consumeAnalysisAutoStopped());
    runWithBeatBursts(impl, 0.3f, samplePos);
    EXPECT_TRUE(impl.consumeAnalysisAutoStopped());
    EXPECT_FALSE(impl.consumeAnalysisAutoStopped());
    EXPECT_TRUE(impl.consumeAnalysisReportReady());
    EXPECT_EQ(reportedHits(impl.buildAnalysisReportHtml()), 16u);
    EXPECT_TRUE(impl.analysisStatusText().empty());
}

TEST(MetronomeImplTest, AnalysisWithOpenEndKeepsCapturingUntilSwitchedOff)
{
    Impl impl(kSampleRate);
    size_t samplePos{0};
    impl.setAnalysisMode(true);
    runWithBeatBursts(impl, 21.f, samplePos);
    EXPECT_FALSE(impl.consumeAnalysisAutoStopped());
    EXPECT_FALSE(impl.consumeAnalysisReportReady());
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: bar 9");
}

TEST(MetronomeImplTest, AnalysisStatusTextFollowsCountInAndCapture)
{
    Impl impl(kSampleRate);
    size_t samplePos{0};
    EXPECT_TRUE(impl.analysisStatusText().empty());
    impl.setAnalysisBars(2);
    impl.setAnalysisMode(true);
    runWithBeatBursts(impl, 1.f, samplePos);
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: count-in bar 1/2");
    runWithBeatBursts(impl, 2.f, samplePos);
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: count-in bar 2/2");
    runWithBeatBursts(impl, 2.f, samplePos);
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: bar 1/8");
}

TEST(MetronomeImplTest, AnalysisCountInIsAudibleWithStartOffAndCaptureIsNot)
{
    Impl impl(kSampleRate);
    impl.setAnalysisMode(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
    static_cast<void>(runSeconds(impl, 3.2f));
    EXPECT_EQ(runSeconds(impl, 1.f), 0.f);
}

TEST(MetronomeImplTest, SwitchingAnalysisOffDuringTheCountInWritesNoReport)
{
    Impl impl(kSampleRate);
    size_t samplePos{0};
    impl.setAnalysisMode(true);
    runWithBeatBursts(impl, 1.f, samplePos);
    impl.setAnalysisMode(false);
    runWithBeatBursts(impl, 0.01f, samplePos);
    EXPECT_FALSE(impl.consumeAnalysisReportReady());
    EXPECT_TRUE(impl.analysisStatusText().empty());
}

TEST(MetronomeImplTest, AnalysisUnderHostSyncWaitsForTheNextBarBeforeTheCountIn)
{
    Impl impl(kSampleRate);
    size_t samplePos{0};
    impl.setHostSync(true);
    impl.setAnalysisMode(true);
    runWithBeatBursts(impl, 0.5f, samplePos);
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: waiting for the next bar");
    runWithBeatBursts(impl, 2.f, samplePos);
    EXPECT_EQ(impl.analysisStatusText(), "Analysis: count-in bar 1/2");
}

TEST(MetronomeImplTest, ScriptNameAppearsInTheReportAndTheFileStem)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Bossa Nova')\n"));
    EXPECT_EQ(impl.scriptName(), "Bossa Nova");
    EXPECT_EQ(impl.analysisFileStem(), "bossa-nova");
    EXPECT_THAT(impl.buildAnalysisReportHtml(),
                ::testing::HasSubstr("<title>Metronome Timing Analysis - Bossa Nova</title>"));
}

TEST(MetronomeImplTest, DisplayNameFallsBackToThePresetThenToScript)
{
    Impl impl(kSampleRate);
    EXPECT_EQ(impl.displayName(), "4/4 8th");
    EXPECT_TRUE(impl.analysisFileStem().empty());
    ASSERT_TRUE(impl.setScript("SetBeatsPerBar(7)\n"));
    EXPECT_EQ(impl.displayName(), "Script");
    ASSERT_TRUE(impl.setScript("SetName('Seven')\nSetBeatsPerBar(7)\n"));
    EXPECT_EQ(impl.displayName(), "Seven");
}

TEST(MetronomeImplTest, ScriptWithoutSetNameClearsThePreviousName)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Rock')\n"));
    ASSERT_TRUE(impl.setScript("-- nothing\n"));
    EXPECT_TRUE(impl.scriptName().empty());
}

TEST(MetronomeImplTest, FailedScriptKeepsThePreviousName)
{
    Impl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript("SetName('Rock')\n"));
    ASSERT_FALSE(impl.setScript("SetName('Jazz')\nthis is not lua\n"));
    EXPECT_EQ(impl.scriptName(), "Rock");
}
