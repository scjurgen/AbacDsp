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

TEST(MetronomeImplTest, SkeletonScriptCompilesAndDefinesNoPattern)
{
    Impl impl(kSampleRate);
    EXPECT_TRUE(impl.setScript(Impl::scriptSkeleton()));
    impl.setOnOff(true);
    EXPECT_GT(runSeconds(impl, 1.f), 0.001f);
}
