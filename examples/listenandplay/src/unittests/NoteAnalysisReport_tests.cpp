#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "TestTakes.h"
#include "impl/NoteAnalysisReport.h"

using namespace TestTakes;
using ::testing::HasSubstr;
using ::testing::Not;

namespace
{
std::string reportFor(const NoteMatching::TakeSetup& setup, const std::vector<AbacDsp::PlayedNote>& played,
                      const std::string_view name = "Lesson")
{
    const auto match = NoteMatching::matchNotes(setup, played);
    return NoteReport::buildReportHtml(setup, match, NoteScoring::scoreTake(setup, match), name);
}
}

TEST(NoteAnalysisReportTest, IsAWholeHtmlPageWithTheScoresAndOneRowPerNote)
{
    const auto setup = makeSetup(makePattern());
    const auto html = reportFor(setup, perfectTake(setup), "A6 Major scale");
    EXPECT_THAT(html, HasSubstr("<!doctype html>"));
    EXPECT_THAT(html, HasSubstr("A6 Major scale"));
    EXPECT_THAT(html, HasSubstr("passed"));
    EXPECT_THAT(html, HasSubstr("100 %"));
    EXPECT_THAT(html, HasSubstr("8 / 8"));
    EXPECT_THAT(html, HasSubstr("</html>"));
    size_t rows = 0;
    for (size_t at = html.find("<tr><td>"); at != std::string::npos; at = html.find("<tr><td>", at + 1))
    {
        ++rows;
    }
    EXPECT_EQ(rows, 8u);
}

TEST(NoteAnalysisReportTest, NamesPlayedAndExpectedNotesAndShowsTheirErrors)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played[0].midiNote = 62.2f;
    played[0].onsetSeconds += 0.045f;
    played[0].offsetSeconds += 0.045f;
    const auto html = reportFor(setup, played);
    EXPECT_THAT(html, HasSubstr("C4"));
    EXPECT_THAT(html, HasSubstr("D4"));
    EXPECT_THAT(html, HasSubstr("+45 ms"));
    EXPECT_THAT(html, HasSubstr("wrong note"));
    EXPECT_THAT(html, HasSubstr("+20 cents"));
}

TEST(NoteAnalysisReportTest, ListsMissedNotesAndMentionsExtraOnes)
{
    const auto setup = makeSetup(makePattern());
    auto played = perfectTake(setup);
    played.erase(played.begin() + 3);
    AbacDsp::PlayedNote extra = played[0];
    extra.onsetSeconds = setup.downbeatSeconds + 12.f * kSecondsPerBeat;
    extra.offsetSeconds = extra.onsetSeconds + 0.2f;
    played.push_back(extra);
    const auto html = reportFor(setup, played);
    EXPECT_THAT(html, HasSubstr("not played"));
    EXPECT_THAT(html, HasSubstr("missed"));
    EXPECT_THAT(html, HasSubstr("1 note(s) were played that the phrase does not have"));
}

TEST(NoteAnalysisReportTest, DescribesTheTakeSettings)
{
    auto setup = makeSetup(makePattern(), false);
    auto played = perfectTake(setup);
    for (auto& note : played)
    {
        note.midiNote += 12.f;
    }
    const auto html = reportFor(setup, played);
    EXPECT_THAT(html, HasSubstr("click off"));
    EXPECT_THAT(html, HasSubstr("% of the tempo"));
    EXPECT_THAT(html, HasSubstr("1 octave up"));
    const auto clicked = reportFor(makeSetup(makePattern()), perfectTake(makeSetup(makePattern())));
    EXPECT_THAT(clicked, HasSubstr("click on"));
    EXPECT_THAT(clicked, HasSubstr("suggested latency"));
}

TEST(NoteAnalysisReportTest, EscapesTheLessonName)
{
    const auto setup = makeSetup(makePattern());
    const auto html = reportFor(setup, perfectTake(setup), "<script>alert('x')</script>");
    EXPECT_THAT(html, Not(HasSubstr("<script>alert")));
    EXPECT_THAT(html, HasSubstr("&lt;script&gt;"));
}

TEST(NoteAnalysisReportTest, ReportsAMissingPhraseInsteadOfScoring)
{
    const NoteMatching::TakeSetup setup;
    const auto html = reportFor(setup, {});
    EXPECT_THAT(html, HasSubstr("No phrase to score"));
    EXPECT_THAT(html, HasSubstr("</html>"));
}

TEST(NoteAnalysisReportTest, PositionLabelsCountBarsAndBeatsFromOneAndShowPickups)
{
    EXPECT_EQ(NoteReport::positionLabel(0.f, 4), "1.1.00");
    EXPECT_EQ(NoteReport::positionLabel(5.5f, 4), "2.2.50");
    EXPECT_EQ(NoteReport::positionLabel(-0.5f, 4), "pickup -0.50");
}
