#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include "impl/LessonCatalog.h"
#include "impl/LessonProgress.h"

namespace
{
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

ImportLookup resolveFromLessonsDir(const std::string_view name)
{
    const std::filesystem::path path = std::filesystem::path{LISTENANDPLAY_LESSONS_DIR} / (std::string{name} + ".lua");
    return std::filesystem::exists(path) ? ImportLookup{readFile(path), {}} : ImportLookup{};
}

std::vector<std::string> libraryNames()
{
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator{LISTENANDPLAY_LESSONS_DIR})
    {
        names.push_back(entry.path().stem().string());
    }
    return names;
}

LessonCatalog makeCatalog()
{
    LessonCatalog catalog;
    const auto names = libraryNames();
    catalog.rebuild(names, resolveFromLessonsDir);
    return catalog;
}

std::vector<LessonInfo> sampleLessons()
{
    return {{"a01-x", "A1", NotePattern::Focus::Pitch},
            {"a02-x", "A2", NotePattern::Focus::Pitch},
            {"a03-x", "A3", NotePattern::Focus::Pitch},
            {"b01-x", "B1", NotePattern::Focus::Rhythm},
            {"b02-x", "B2", NotePattern::Focus::Rhythm}};
}
}

TEST(LessonCatalogTest, OnlyScriptsNamedLikeLessonsAreLessons)
{
    EXPECT_TRUE(LessonCatalog::isLessonStem("a06-major-scale"));
    EXPECT_TRUE(LessonCatalog::isLessonStem("d05-melody-blues"));
    EXPECT_FALSE(LessonCatalog::isLessonStem("lessonlib"));
    EXPECT_TRUE(LessonCatalog::isLessonStem("e01-cdef-c3"));
    EXPECT_FALSE(LessonCatalog::isLessonStem("f01-x"));
    EXPECT_FALSE(LessonCatalog::isLessonStem("a1-x"));
    EXPECT_FALSE(LessonCatalog::isLessonStem("a01"));
    EXPECT_FALSE(LessonCatalog::isLessonStem("a01-"));
}

TEST(LessonCatalogTest, TheCurriculumIsListedInOrderWithTheNamesTheScriptsGive)
{
    const auto catalog = makeCatalog();
    const auto& lessons = catalog.lessons();
    ASSERT_GE(lessons.size(), 30u);
    EXPECT_TRUE(std::ranges::is_sorted(lessons, {}, &LessonInfo::stem));
    EXPECT_EQ(lessons.front().stem, "a01-triad-3");
    EXPECT_EQ(lessons.front().title, "A1 Triad, 3 notes");
    EXPECT_EQ(lessons.front().focus, NotePattern::Focus::Pitch);
    const auto rhythm = catalog.indexOf("b06-syncopation");
    ASSERT_TRUE(rhythm.has_value());
    EXPECT_EQ(lessons[*rhythm].focus, NotePattern::Focus::Rhythm);
    EXPECT_EQ(lessons[*rhythm].title, "B6 Syncopation");
    EXPECT_FALSE(catalog.indexOf("lessonlib").has_value());
    EXPECT_FALSE(catalog.indexOf("nonsense").has_value());
}

TEST(LessonCatalogTest, TheRecordingExercisesAreFixedScaleFragmentsFromC3C4AndC5)
{
    const auto catalog = makeCatalog();
    const std::vector<std::pair<std::string, uint8_t>> exercises{
        {"e01-cdef-c3", 48}, {"e02-cdef-c4", 60}, {"e03-cdef-c5", 72}};
    for (const auto& [stem, first] : exercises)
    {
        const auto index = catalog.indexOf(stem);
        ASSERT_TRUE(index.has_value()) << stem;
        EXPECT_EQ(catalog.lessons()[*index].focus, NotePattern::Focus::Pitch);
        ListenAndPlayScriptEngine engine;
        engine.setImportResolver(resolveFromLessonsDir);
        ASSERT_TRUE(engine.loadPattern(readFile(std::filesystem::path{LISTENANDPLAY_LESSONS_DIR} / (stem + ".lua"))))
            << stem;
        const auto& pattern = engine.result().pattern;
        ASSERT_EQ(pattern.noteCount, 4u);
        const std::array<uint8_t, 4> steps{0, 2, 4, 5};
        for (size_t i = 0; i < 4; ++i)
        {
            EXPECT_EQ(pattern.notes[i].pitch, first + steps[i]) << stem;
            EXPECT_FLOAT_EQ(pattern.notes[i].positionBeats, static_cast<float>(i));
        }
        EXPECT_EQ(pattern.tonic, first);
        EXPECT_FLOAT_EQ(pattern.bpm, 60.f);
        EXPECT_FALSE(pattern.scoring.octaveFree);
        EXPECT_FLOAT_EQ(pattern.scoring.passScore, 1.f);
    }
}

TEST(LessonCatalogTest, ABrokenLessonIsStillListedUnderItsFileName)
{
    LessonCatalog catalog;
    const std::vector<std::string> names{"a01-broken", "a02-fine"};
    catalog.rebuild(names,
                    [](const std::string_view name) -> ImportLookup
                    {
                        return {name == "a01-broken" ? std::string{"AddNote(99, 1, 1)"}
                                                     : std::string{"SetName('Fine')\nAddNote(0, 60, 1)"},
                                {}};
                    });
    ASSERT_EQ(catalog.lessons().size(), 2u);
    EXPECT_EQ(catalog.lessons()[0].title, "a01-broken");
    EXPECT_EQ(catalog.lessons()[1].title, "Fine");
}

TEST(LessonProgressTest, RecordsBestLastAttemptsAndPassing)
{
    LessonProgress progress;
    EXPECT_EQ(progress.find("a01-x"), nullptr);
    progress.record("a01-x", 60.f, false);
    progress.record("a01-x", 85.f, true);
    progress.record("a01-x", 70.f, false);
    const auto* record = progress.find("a01-x");
    ASSERT_NE(record, nullptr);
    EXPECT_FLOAT_EQ(record->best, 85.f);
    EXPECT_FLOAT_EQ(record->last, 70.f);
    EXPECT_EQ(record->attempts, 3u);
    EXPECT_TRUE(record->passed);
    EXPECT_TRUE(progress.isPassed("a01-x"));
    EXPECT_FALSE(progress.isPassed("a02-x"));
}

TEST(LessonProgressTest, ALessonOpensWhenTheOneBeforeItInItsGroupIsPassed)
{
    LessonProgress progress;
    const auto lessons = sampleLessons();
    EXPECT_TRUE(progress.isUnlocked(lessons, 0));
    EXPECT_FALSE(progress.isUnlocked(lessons, 1));
    EXPECT_TRUE(progress.isUnlocked(lessons, 3));
    EXPECT_FALSE(progress.isUnlocked(lessons, 4));
    progress.record("a01-x", 90.f, true);
    EXPECT_TRUE(progress.isUnlocked(lessons, 1));
    EXPECT_FALSE(progress.isUnlocked(lessons, 2));
    progress.record("b01-x", 50.f, false);
    EXPECT_FALSE(progress.isUnlocked(lessons, 4));
}

TEST(LessonProgressTest, TheRecommendedLessonIsTheFirstOpenOneNotPassedYet)
{
    LessonProgress progress;
    const auto lessons = sampleLessons();
    EXPECT_EQ(progress.recommended(lessons), 0u);
    progress.record("a01-x", 90.f, true);
    EXPECT_EQ(progress.recommended(lessons), 1u);
    progress.record("a02-x", 90.f, true);
    progress.record("a03-x", 90.f, true);
    EXPECT_EQ(progress.recommended(lessons), 3u);
    progress.record("b01-x", 90.f, true);
    progress.record("b02-x", 90.f, true);
    EXPECT_FALSE(progress.recommended(lessons).has_value());
}

TEST(LessonProgressTest, SurvivesAJsonRoundTrip)
{
    LessonProgress progress;
    progress.record("a01-x", 77.5f, true);
    progress.record("a01-x", 60.f, false);
    progress.setLastLesson("a02-x");
    LessonProgress restored;
    ASSERT_TRUE(restored.fromJson(progress.toJson()));
    EXPECT_EQ(restored.lastLesson(), "a02-x");
    const auto* record = restored.find("a01-x");
    ASSERT_NE(record, nullptr);
    EXPECT_FLOAT_EQ(record->best, 77.5f);
    EXPECT_FLOAT_EQ(record->last, 60.f);
    EXPECT_EQ(record->attempts, 2u);
    EXPECT_TRUE(record->passed);
}

TEST(LessonProgressTest, AnInvalidTextLeavesTheProgressAsItWas)
{
    LessonProgress progress;
    progress.record("a01-x", 80.f, true);
    EXPECT_FALSE(progress.fromJson("not json"));
    EXPECT_FALSE(progress.fromJson("[1, 2]"));
    EXPECT_TRUE(progress.isPassed("a01-x"));
    EXPECT_TRUE(progress.fromJson("{}"));
    EXPECT_FALSE(progress.isPassed("a01-x"));
}
