#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "impl/ListenAndPlayScriptEngine.h"

using namespace NotePattern;

namespace
{
constexpr long long kSeedsPerLesson{60};
constexpr float kBeatGrid{12.f};
constexpr float kGridTolerance{1e-3f};

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
    if (!std::filesystem::exists(path))
    {
        return {};
    }
    return {readFile(path), {}};
}

struct LessonFile
{
    std::string stem;
    std::string source;
};

std::vector<LessonFile> lessonFiles()
{
    std::vector<LessonFile> files;
    for (const auto& entry : std::filesystem::directory_iterator{LISTENANDPLAY_LESSONS_DIR})
    {
        const auto stem = entry.path().stem().string();
        const bool fixedExercise = stem.starts_with('e');
        if (entry.path().extension() == ".lua" && stem != "lessonlib" && !fixedExercise)
        {
            files.push_back({stem, readFile(entry.path())});
        }
    }
    std::ranges::sort(files, {}, &LessonFile::stem);
    return files;
}

struct LessonEngine : ListenAndPlayScriptEngine
{
    LessonEngine()
    {
        setImportResolver(resolveFromLessonsDir);
    }
};

std::vector<Note> notesOf(const Pattern& pattern)
{
    return {pattern.notes.begin(), pattern.notes.begin() + static_cast<std::ptrdiff_t>(pattern.noteCount)};
}

Focus expectedFocus(const char group)
{
    switch (group)
    {
        case 'a':
            return Focus::Pitch;
        case 'b':
            return Focus::Rhythm;
        case 'c':
            return Focus::Dynamics;
        default:
            return Focus::Combined;
    }
}

bool onGrid(const float beats)
{
    const float scaled = beats * kBeatGrid;
    return std::abs(scaled - std::round(scaled)) < kGridTolerance;
}

int pitchClass(const int pitch, const int tonic)
{
    return ((pitch - tonic) % 12 + 12) % 12;
}
}

TEST(LessonCurriculumTest, ShipsLessonsOfEveryGroup)
{
    std::set<char> groups;
    for (const auto& file : lessonFiles())
    {
        groups.insert(file.stem.front());
    }
    EXPECT_EQ(groups, (std::set<char>{'a', 'b', 'c', 'd'}));
}

TEST(LessonCurriculumTest, EveryLessonProducesAValidPhraseForManySeeds)
{
    for (const auto& file : lessonFiles())
    {
        LessonEngine engine;
        for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
        {
            SCOPED_TRACE(file.stem + " seed " + std::to_string(seed));
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto& pattern = engine.result().pattern;
            ASSERT_TRUE(pattern.active);
            ASSERT_FALSE(engine.result().name.empty());
            EXPECT_EQ(pattern.focus, expectedFocus(file.stem.front()));
            EXPECT_GE(pattern.bpm, kMinBpm);
            EXPECT_LE(pattern.bpm, kMaxBpm);
            ASSERT_GE(pattern.noteCount, 2u);
            for (const auto& note : notesOf(pattern))
            {
                EXPECT_GE(note.pitch, ListenAndPlayScriptEngine::kDefaultRangeLow);
                EXPECT_LE(note.pitch, ListenAndPlayScriptEngine::kDefaultRangeHigh);
                EXPECT_TRUE(onGrid(note.positionBeats));
                EXPECT_TRUE(onGrid(note.durationBeats));
            }
        }
    }
}

TEST(LessonCurriculumTest, SameSeedGivesTheSamePhraseAndSeedsVary)
{
    for (const auto& file : lessonFiles())
    {
        SCOPED_TRACE(file.stem);
        LessonEngine engine;
        std::set<std::vector<int>> distinct;
        for (long long seed = 1; seed <= 20; ++seed)
        {
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto first = notesOf(engine.result().pattern);
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto second = notesOf(engine.result().pattern);
            ASSERT_EQ(first.size(), second.size());
            std::vector<int> signature;
            for (size_t i = 0; i < first.size(); ++i)
            {
                EXPECT_EQ(first[i].pitch, second[i].pitch);
                EXPECT_FLOAT_EQ(first[i].positionBeats, second[i].positionBeats);
                EXPECT_EQ(first[i].velocity, second[i].velocity);
                signature.push_back(first[i].pitch);
                signature.push_back(first[i].velocity);
                signature.push_back(static_cast<int>(std::lround(first[i].positionBeats * kBeatGrid)));
            }
            distinct.insert(signature);
        }
        EXPECT_GT(distinct.size(), 1u);
    }
}

TEST(LessonCurriculumTest, PitchLessonsKeepToTheirMelodicRules)
{
    constexpr int kLargestLeapSemitones{9};
    for (const auto& file : lessonFiles())
    {
        if (file.stem.front() != 'a' || file.stem.starts_with("a10") || file.stem.starts_with("a11") ||
            file.stem.starts_with("a12") || file.stem.starts_with("a18"))
        {
            continue;
        }
        LessonEngine engine;
        for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
        {
            SCOPED_TRACE(file.stem + " seed " + std::to_string(seed));
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto notes = notesOf(engine.result().pattern);
            for (size_t i = 1; i < notes.size(); ++i)
            {
                const int leap = std::abs(notes[i].pitch - notes[i - 1].pitch);
                EXPECT_NE(leap, 0);
                EXPECT_LE(leap, kLargestLeapSemitones);
                EXPECT_FLOAT_EQ(notes[i].durationBeats, 1.f);
            }
        }
    }
}

TEST(LessonCurriculumTest, IntervalPairLessonsUseOnlyTheirIntervals)
{
    const std::vector<std::pair<std::string, std::set<int>>> lessons{{"a10", {1, 2, 3, 4}},
                                                                     {"a11", {5, 7, 12}},
                                                                     {"a12", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}},
                                                                     {"a18", {8, 9, 10, 11}}};
    for (const auto& file : lessonFiles())
    {
        for (const auto& [prefix, intervals] : lessons)
        {
            if (!file.stem.starts_with(prefix))
            {
                continue;
            }
            LessonEngine engine;
            for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
            {
                SCOPED_TRACE(file.stem + " seed " + std::to_string(seed));
                ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
                const auto notes = notesOf(engine.result().pattern);
                ASSERT_EQ(notes.size() % 2, 0u);
                for (size_t i = 0; i < notes.size(); i += 2)
                {
                    EXPECT_TRUE(intervals.contains(std::abs(notes[i + 1].pitch - notes[i].pitch)));
                }
            }
        }
    }
}

TEST(LessonCurriculumTest, RhythmLessonsStartOnTheirFirstNoteAndUseFixedPitches)
{
    for (const auto& file : lessonFiles())
    {
        if (file.stem.front() != 'b')
        {
            continue;
        }
        LessonEngine engine;
        for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
        {
            SCOPED_TRACE(file.stem + " seed " + std::to_string(seed));
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto& pattern = engine.result().pattern;
            const auto notes = notesOf(pattern);
            EXPECT_NEAR(notes.front().positionBeats, -pattern.pickupBeats, 1e-4f);
            std::set<int> pitches;
            for (const auto& note : notes)
            {
                pitches.insert(note.pitch);
            }
            EXPECT_LE(pitches.size(), 2u);
        }
    }
}

TEST(LessonCurriculumTest, AnacrusisLessonsPlaceNotesBeforeTheDownbeat)
{
    size_t checked = 0;
    for (const auto& file : lessonFiles())
    {
        LessonEngine engine;
        ASSERT_TRUE(engine.loadPattern(file.source, 4, 1)) << engine.lastError();
        if (engine.result().pattern.pickupBeats <= 0.f)
        {
            continue;
        }
        ++checked;
        for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
        {
            SCOPED_TRACE(file.stem + " seed " + std::to_string(seed));
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto& pattern = engine.result().pattern;
            EXPECT_LT(pattern.notes.front().positionBeats, 0.f);
            const auto notes = notesOf(pattern);
            const bool hasNoteOnDownbeat =
                std::ranges::any_of(notes, [](const Note& note) { return std::abs(note.positionBeats) < 1e-4f; });
            EXPECT_TRUE(hasNoteOnDownbeat);
        }
    }
    EXPECT_GE(checked, 3u);
}

TEST(LessonCurriculumTest, DynamicsLessonsVaryTheVelocityAndRhythmLessonsDoNot)
{
    for (const auto& file : lessonFiles())
    {
        const char group = file.stem.front();
        if (group != 'a' && group != 'b' && group != 'c')
        {
            continue;
        }
        LessonEngine engine;
        for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
        {
            SCOPED_TRACE(file.stem + " seed " + std::to_string(seed));
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            std::set<int> velocities;
            for (const auto& note : notesOf(engine.result().pattern))
            {
                velocities.insert(note.velocity);
            }
            if (group == 'c')
            {
                EXPECT_GT(velocities.size(), 1u);
            }
            else
            {
                EXPECT_EQ(velocities.size(), 1u);
            }
        }
    }
}

TEST(LessonCurriculumTest, CrescendoLessonIsMonotonicInOneDirection)
{
    for (const auto& file : lessonFiles())
    {
        if (!file.stem.starts_with("c02"))
        {
            continue;
        }
        LessonEngine engine;
        std::set<bool> rising;
        for (long long seed = 1; seed <= kSeedsPerLesson; ++seed)
        {
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto notes = notesOf(engine.result().pattern);
            const bool up = notes.back().velocity > notes.front().velocity;
            rising.insert(up);
            for (size_t i = 1; i < notes.size(); ++i)
            {
                EXPECT_TRUE(up ? notes[i].velocity >= notes[i - 1].velocity
                               : notes[i].velocity <= notes[i - 1].velocity);
            }
        }
        EXPECT_EQ(rising.size(), 2u);
    }
}

TEST(LessonCurriculumTest, MelodyStartsAndEndsOnTonicThirdOrFifthAndMovesByStep)
{
    constexpr std::string_view kScript = R"(import "lessonlib"
ClearPattern()
SetPatternBars(2)
local ladder = ScaleLadder(0, Music.Scales.Major, 48, 72)
local pitches = Melody(ladder, 8, { maxLeap = 2 })
for i, pitch in ipairs(pitches) do
    AddNote(i - 1, pitch, 1)
end
)";
    LessonEngine engine;
    for (long long seed = 1; seed <= 200; ++seed)
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        ASSERT_TRUE(engine.loadPattern(kScript, 4, seed)) << engine.lastError();
        const auto notes = notesOf(engine.result().pattern);
        ASSERT_EQ(notes.size(), 8u);
        const std::set<int> stable{0, 4, 7};
        const std::set<int> majorScale{0, 2, 4, 5, 7, 9, 11};
        EXPECT_TRUE(stable.contains(pitchClass(notes.front().pitch, 0)));
        EXPECT_TRUE(stable.contains(pitchClass(notes.back().pitch, 0)));
        for (size_t i = 1; i < notes.size(); ++i)
        {
            const int leap = std::abs(notes[i].pitch - notes[i - 1].pitch);
            EXPECT_GE(leap, 1);
            EXPECT_LE(leap, 4);
            EXPECT_TRUE(majorScale.contains(pitchClass(notes[i].pitch, 0)));
        }
    }
}

TEST(LessonCurriculumTest, RhythmCellsFillTheirBeatsExactlyAndKeepTheirGrid)
{
    constexpr std::string_view kScript = R"(import "lessonlib"
ClearPattern()
SetPatternBars(2)
local notes = BuildRhythm({ "quarter", "half", "eighths", "restQuarter", "syncopation", "dottedQuarter" }, 8, 0, 3)
for _, note in ipairs(notes) do
    AddNote(note.position, 60, note.length)
end
)";
    LessonEngine engine;
    for (long long seed = 1; seed <= 200; ++seed)
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        ASSERT_TRUE(engine.loadPattern(kScript, 4, seed)) << engine.lastError();
        const auto notes = notesOf(engine.result().pattern);
        ASSERT_GE(notes.size(), 3u);
        EXPECT_FLOAT_EQ(notes.front().positionBeats, 0.f);
        for (const auto& note : notes)
        {
            EXPECT_LE(note.positionBeats + note.durationBeats, 8.f + 1e-4f);
        }
    }
}

TEST(LessonCurriculumTest, UnknownRhythmCellIsReportedAsAnError)
{
    constexpr std::string_view kScript = R"(import "lessonlib"
BuildRhythm({ "nonsense" }, 4, 0, 2)
)";
    LessonEngine engine;
    EXPECT_FALSE(engine.loadPattern(kScript, 4, 1));
    EXPECT_THAT(engine.lastError(), ::testing::HasSubstr("unknown rhythm cell"));
}

TEST(LessonCurriculumTest, EveryLessonScoresWithSaneToleranceAndPassMark)
{
    for (const auto& file : lessonFiles())
    {
        SCOPED_TRACE(file.stem);
        LessonEngine engine;
        ASSERT_TRUE(engine.loadPattern(file.source, 4, 1)) << engine.lastError();
        const auto& scoring = engine.result().pattern.scoring;
        EXPECT_LT(scoring.timingGoodMs, scoring.timingZeroMs);
        EXPECT_LT(scoring.dynamicsGoodDb, scoring.dynamicsZeroDb);
        EXPECT_GE(scoring.passScore, 50.f);
        EXPECT_LE(scoring.passScore, 90.f);
    }
}

TEST(LessonCurriculumTest, TimingToleranceNeverGetsLooserWithinAGroup)
{
    std::map<char, float> previous;
    for (const auto& file : lessonFiles())
    {
        LessonEngine engine;
        ASSERT_TRUE(engine.loadPattern(file.source, 4, 1)) << engine.lastError();
        const float good = engine.result().pattern.scoring.timingGoodMs;
        const char group = file.stem.front();
        if (previous.contains(group))
        {
            EXPECT_LE(good, previous[group]) << file.stem;
        }
        previous[group] = good;
    }
}

namespace
{
NotePattern::Pattern patternOf(const std::string& stem, const long long seed)
{
    LessonEngine engine;
    const auto source = readFile(std::filesystem::path{LISTENANDPLAY_LESSONS_DIR} / (stem + ".lua"));
    EXPECT_TRUE(engine.loadPattern(source, 4, seed)) << engine.lastError();
    return engine.result().pattern;
}
}

TEST(LessonCurriculumTest, TheWaltzAndFiveFourLessonsUseTheirBarLength)
{
    for (const auto* stem : {"b11-waltz", "b12-waltz-rests", "b16-waltz-pickup", "c09-waltz-levels", "d06-waltz-melody",
                             "d09-minor-waltz-motif"})
    {
        for (long long seed = 1; seed <= 20; ++seed)
        {
            const auto pattern = patternOf(stem, seed);
            EXPECT_EQ(pattern.beatsPerBar, 3u) << stem;
            const auto& last = pattern.notes[pattern.noteCount - 1];
            EXPECT_LE(last.positionBeats + last.durationBeats, pattern.patternBeats() + 1e-4f) << stem;
        }
    }
    for (long long seed = 1; seed <= 20; ++seed)
    {
        EXPECT_EQ(patternOf("b13-five-four", seed).beatsPerBar, 5u);
    }
}

TEST(LessonCurriculumTest, MotifLessonsRepeatTheirRhythmAndContourOnOtherScaleSteps)
{
    const std::vector<std::pair<std::string, size_t>> lessons{
        {"d07-motif", 2}, {"d08-motif-sequence", 3}, {"d09-minor-waltz-motif", 2}, {"d10-blues-motif", 2}};
    for (const auto& [stem, repeats] : lessons)
    {
        for (long long seed = 1; seed <= 40; ++seed)
        {
            SCOPED_TRACE(stem + " seed " + std::to_string(seed));
            const auto pattern = patternOf(stem, seed);
            ASSERT_EQ(pattern.noteCount % repeats, 0u);
            const size_t length = pattern.noteCount / repeats;
            const float motifBeats = pattern.notes[length].positionBeats - pattern.notes[0].positionBeats;
            for (size_t r = 1; r < repeats; ++r)
            {
                for (size_t i = 0; i < length; ++i)
                {
                    const auto& first = pattern.notes[i];
                    const auto& again = pattern.notes[r * length + i];
                    EXPECT_NEAR(again.positionBeats, first.positionBeats + static_cast<float>(r) * motifBeats, 1e-3f);
                    EXPECT_NEAR(again.durationBeats, first.durationBeats, 1e-3f);
                    EXPECT_NE(again.pitch, first.pitch);
                    if (i > 0)
                    {
                        const int before = pattern.notes[i].pitch - pattern.notes[i - 1].pitch;
                        const int after = pattern.notes[r * length + i].pitch - pattern.notes[r * length + i - 1].pitch;
                        EXPECT_EQ(before > 0, after > 0);
                        EXPECT_EQ(before < 0, after < 0);
                    }
                }
            }
        }
    }
}

TEST(LessonCurriculumTest, AMotifMelodyShiftsByScaleStepsAndEndsOnAStableNote)
{
    constexpr std::string_view kScript = R"(import "lessonlib"
ClearPattern()
SetPatternBars(2)
local ladder = ScaleLadder(0, Music.Scales.Major, 48, 72)
local pitches = MotifMelody(ladder, 4, 2, { maxLeap = 2 })
for i, pitch in ipairs(pitches) do
    AddNote(i - 1, pitch, 1)
end
)";
    LessonEngine engine;
    const std::set<int> stable{0, 4, 7};
    const std::set<int> majorScale{0, 2, 4, 5, 7, 9, 11};
    for (long long seed = 1; seed <= 200; ++seed)
    {
        SCOPED_TRACE("seed " + std::to_string(seed));
        ASSERT_TRUE(engine.loadPattern(kScript, 4, seed)) << engine.lastError();
        const auto notes = notesOf(engine.result().pattern);
        ASSERT_EQ(notes.size(), 8u);
        EXPECT_TRUE(stable.contains(pitchClass(notes.front().pitch, 0)));
        EXPECT_TRUE(stable.contains(pitchClass(notes.back().pitch, 0)));
        for (const auto& note : notes)
        {
            EXPECT_TRUE(majorScale.contains(pitchClass(note.pitch, 0)));
        }
        EXPECT_NE(notes[4].pitch, notes[0].pitch);
    }
}

TEST(LessonCurriculumTest, TheDynamicsPatternsHaveTheirShape)
{
    const auto velocities = [](const std::string& mode)
    {
        LessonEngine engine;
        const std::string script =
            "import \"lessonlib\"\nClearPattern()\nSetPatternBars(2)\nfor i, v in ipairs(BuildDynamics(\"" + mode +
            "\", 8)) do AddNote(i - 1, 60, 1, v) end\n";
        EXPECT_TRUE(engine.loadPattern(script, 4, 1)) << engine.lastError();
        std::vector<int> result;
        for (const auto& note : notesOf(engine.result().pattern))
        {
            result.push_back(note.velocity);
        }
        return result;
    };
    const auto backbeat = velocities("backbeat");
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(backbeat[i] > 90, i % 2 == 1) << i;
    }
    const auto swell = velocities("swell");
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(swell[i], swell[7 - i]);
        EXPECT_LT(swell[i], swell[i + 1] + (i == 3 ? 1 : 0));
    }
    EXPECT_EQ(*std::ranges::max_element(swell), swell[3]);
    const auto echo = velocities("echo");
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(echo[i], echo[i % 2]);
    }
    EXPECT_GT(echo[0], echo[1]);
}

TEST(LessonCurriculumTest, EveryLessonNamesAKeyNoteInTheRange)
{
    for (const auto& file : lessonFiles())
    {
        SCOPED_TRACE(file.stem);
        LessonEngine engine;
        for (long long seed = 1; seed <= 20; ++seed)
        {
            ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
            const auto tonic = engine.result().pattern.tonic;
            EXPECT_GE(tonic, ListenAndPlayScriptEngine::kDefaultRangeLow);
            EXPECT_LE(tonic, ListenAndPlayScriptEngine::kDefaultRangeHigh);
        }
    }
}

TEST(LessonCurriculumTest, TheNotesOfAScaleLessonBelongToTheScaleOfItsKeyNote)
{
    const std::vector<std::pair<std::string, std::set<int>>> lessons{
        {"a03-pentatonic-steps", {0, 2, 4, 7, 9}}, {"a06-major-scale", {0, 2, 4, 5, 7, 9, 11}},
        {"a13-seventh-arpeggio", {0, 4, 7, 11}},   {"a14-minor-arpeggio", {0, 3, 7, 10}},
        {"a16-blues-scale", {0, 3, 5, 6, 7, 10}},  {"d07-motif", {0, 2, 4, 5, 7, 9, 11}}};
    for (const auto& file : lessonFiles())
    {
        for (const auto& [stem, scale] : lessons)
        {
            if (file.stem != stem)
            {
                continue;
            }
            LessonEngine engine;
            for (long long seed = 1; seed <= 40; ++seed)
            {
                SCOPED_TRACE(stem + " seed " + std::to_string(seed));
                ASSERT_TRUE(engine.loadPattern(file.source, 4, seed)) << engine.lastError();
                const auto& pattern = engine.result().pattern;
                for (const auto& note : notesOf(pattern))
                {
                    EXPECT_TRUE(scale.contains(pitchClass(note.pitch, pattern.tonic)));
                }
            }
        }
    }
}
