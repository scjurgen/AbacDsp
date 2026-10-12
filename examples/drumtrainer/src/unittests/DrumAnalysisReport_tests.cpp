#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "impl/DrumAnalysisReport.h"

using namespace DrumAnalysis;
using DrumInput::PlayedVoice;
using DrumPattern::Instrument;

namespace
{
TakeSetup backbeatSetup()
{
    TakeSetup setup{};
    setup.pattern.active = true;
    setup.pattern.hits[0] = {0.f, Instrument::Kick, 100};
    setup.pattern.hits[1] = {1.f, Instrument::Snare, 100};
    setup.pattern.hits[2] = {1.5f, Instrument::HihatOpen, 100};
    setup.pattern.hits[3] = {2.f, Instrument::Kick, 100};
    setup.pattern.hitCount = 4;
    setup.beatsPerBar = 4;
    setup.capturedBars = 1;
    setup.bpm = 120.f;
    return setup;
}

std::string card(const std::string& label, const std::string& value)
{
    return label + "</h6><p class=\"fs-3 mb-0\">" + value + "</p>";
}
}

TEST(DrumAnalysisReportTest, PositionLabelsNameBarAndBeat)
{
    EXPECT_EQ(positionLabel(2.f, 4, 1), "Beat 3");
    EXPECT_EQ(positionLabel(1.5f, 4, 1), "Beat 2 + 0.5");
    EXPECT_EQ(positionLabel(4.75f, 4, 2), "Bar 2, Beat 1 + 0.75");
    EXPECT_EQ(positionLabel(2.9999999f, 4, 1), "Beat 4");
}

TEST(DrumAnalysisReportTest, SummaryCardsCountHitsMissesExtrasAndHihatErrors)
{
    const std::vector<CapturedHit> played{{0.f, PlayedVoice::Kick, 100, 127},
                                          {1.f, PlayedVoice::Snare, 100, 127},
                                          {1.5f, PlayedVoice::HihatClosed, 100, 127},
                                          {3.f, PlayedVoice::TomFloor, 100, 127},
                                          {3.5f, PlayedVoice::HihatPedal, 100, 127}};
    const std::string html = buildReportHtml(backbeatSetup(), played, "4/4 8th");
    EXPECT_THAT(html, ::testing::HasSubstr(card("Hit", "3 / 4")));
    EXPECT_THAT(html, ::testing::HasSubstr(card("Hit rate", "75 %")));
    EXPECT_THAT(html, ::testing::HasSubstr(card("Missed", "1")));
    EXPECT_THAT(html, ::testing::HasSubstr(card("Extra", "1")));
    EXPECT_THAT(html, ::testing::HasSubstr(card("Wrong hihat", "1")));
    EXPECT_THAT(html, ::testing::HasSubstr(card("Hihat pedal (not scored)", "1")));
    EXPECT_THAT(html, ::testing::HasSubstr(card("Take", "120 BPM, 1 x 4 beats, 4/4 8th")));
}

TEST(DrumAnalysisReportTest, TablesListOnlyInstrumentsThatTookPart)
{
    const std::vector<CapturedHit> played{{0.f, PlayedVoice::Kick, 100, 127}, {3.f, PlayedVoice::TomFloor, 100, 127}};
    const std::string html = buildReportHtml(backbeatSetup(), played, {});
    EXPECT_THAT(html, ::testing::HasSubstr("<td>Tom floor</td>"));
    EXPECT_THAT(html, ::testing::HasSubstr("<td>Hihat open</td>"));
    EXPECT_THAT(html, ::testing::Not(::testing::HasSubstr("<td>Crash</td>")));
    EXPECT_THAT(html, ::testing::HasSubstr("<td>Beat 2 + 0.5</td>"));
}

TEST(DrumAnalysisReportTest, ScriptNameIsEscapedIntoTitleAndHeading)
{
    const std::string html = buildReportHtml(backbeatSetup(), {}, {}, "Rock & <Roll>");
    EXPECT_THAT(html, ::testing::HasSubstr("<title>DrumTrainer Timing Analysis - Rock &amp; &lt;Roll&gt;</title>"));
    EXPECT_THAT(html, ::testing::HasSubstr("<p class=\"fs-4 text-muted mb-4\">Rock &amp; &lt;Roll&gt;</p>"));
}

TEST(DrumAnalysisReportTest, ATakeWithoutAPatternSaysSo)
{
    TakeSetup setup = backbeatSetup();
    setup.pattern.active = false;
    const std::string html = buildReportHtml(setup, {}, {});
    EXPECT_THAT(html, ::testing::HasSubstr("No target pattern"));
    EXPECT_THAT(html, ::testing::Not(::testing::HasSubstr("Hit rate")));
}

TEST(DrumAnalysisReportTest, FileNameStemIsLowercaseWithDashes)
{
    EXPECT_EQ(fileNameStem("Two-Bar Fill!"), "two-bar-fill");
    EXPECT_EQ(fileNameStem(""), "");
}
