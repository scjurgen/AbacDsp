#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#include "Analysis/NoteSegmenter.h"
#include "Numbers/Convert.h"
#include "impl/ListenAndPlayScriptEngine.h"
#include "impl/NoteMatcher.h"
#include "impl/PhrasePlayer.h"
#include "impl/TakeScore.h"

using KsInstrument::Id;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr double kOnsetToleranceSeconds{0.003};
constexpr double kPitchToleranceSemitones{0.2};
constexpr double kLevelToleranceDb{3.0};

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

struct LessonEngine : ListenAndPlayScriptEngine
{
    LessonEngine()
    {
        setImportResolver(resolveFromLessonsDir);
    }
};

std::vector<float> renderPhrase(const NotePattern::Pattern& pattern, const Id id)
{
    PhrasePlayer player{kSampleRate};
    player.start(pattern, pattern.bpm, id);
    std::vector<float> out(player.lengthSamples() + 4800);
    for (size_t i = 0; i < out.size(); i += 16)
    {
        player.process(std::span<float>{out}.subspan(i, std::min<size_t>(16, out.size() - i)));
    }
    return out;
}

std::vector<AbacDsp::PlayedNote> segment(const std::vector<float>& audio, const Id id)
{
    const auto& preset = KsInstrument::presetOf(id);
    AbacDsp::NoteSegmenterConfig config;
    config.minHz = 0.85f * Convert::noteToFrequency<float>(static_cast<float>(preset.lowestNote), 440.f);
    config.maxHz = 1.2f * Convert::noteToFrequency<float>(static_cast<float>(preset.highestNote()), 440.f);
    return AbacDsp::NoteSegmenter{kSampleRate, config}.analyze(audio);
}

void expectRecovered(const std::string& lessonStem, const Id id, const long long seed)
{
    SCOPED_TRACE(lessonStem + " " + std::string{KsInstrument::presetOf(id).name} + " seed " + std::to_string(seed));
    LessonEngine engine;
    const auto& preset = KsInstrument::presetOf(id);
    engine.setRange(preset.lowestNote, preset.highestNote());
    const auto path = std::filesystem::path{LISTENANDPLAY_LESSONS_DIR} / (lessonStem + ".lua");
    ASSERT_TRUE(engine.loadPattern(readFile(path), 4, seed)) << engine.lastError();
    const auto& pattern = engine.result().pattern;
    const auto notes = segment(renderPhrase(pattern, id), id);
    ASSERT_EQ(notes.size(), pattern.noteCount);
    const double secondsPerBeat = 60.0 / static_cast<double>(pattern.bpm);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& expected = pattern.notes[i];
        const double onset =
            (static_cast<double>(expected.positionBeats) + static_cast<double>(pattern.pickupBeats)) * secondsPerBeat;
        EXPECT_NEAR(notes[i].onsetSeconds, onset, kOnsetToleranceSeconds) << "note " << i;
        EXPECT_NEAR(notes[i].midiNote, expected.pitch, kPitchToleranceSemitones) << "note " << i;
        const double level = static_cast<double>(KsInstrument::kReferenceLevelDb) +
                             static_cast<double>(KsInstrument::velocityDb(expected.velocity));
        EXPECT_NEAR(notes[i].attackRmsDb, level, kLevelToleranceDb) << "note " << i;
    }
}

}

TEST(PhrasePlayerAnalysisTest, GuitarLessonsComeBackAsTheirPatterns)
{
    for (const char* stem : {"a01-triad-3", "a03-pentatonic-steps", "a06-major-scale", "a09-chromatic",
                             "a10-pairs-small", "b03-eighths", "b06-syncopation", "b09-anacrusis", "c02-crescendo",
                             "c05-levels-rhythm", "d02-melody-scale", "d05-melody-blues"})
    {
        expectRecovered(stem, Id::Guitar, 1);
    }
}

TEST(PhrasePlayerAnalysisTest, BassAndHarpLessonsComeBackAsTheirPatterns)
{
    for (const char* stem : {"a08-modes", "b05-dotted", "d03-melody-pickup"})
    {
        expectRecovered(stem, Id::Bass, 2);
        expectRecovered(stem, Id::Harp, 2);
    }
}

namespace
{
NotePattern::Pattern loadLesson(const std::string& stem, const Id id, const long long seed)
{
    LessonEngine engine;
    const auto& preset = KsInstrument::presetOf(id);
    engine.setRange(preset.lowestNote, preset.highestNote());
    const auto path = std::filesystem::path{LISTENANDPLAY_LESSONS_DIR} / (stem + ".lua");
    EXPECT_TRUE(engine.loadPattern(readFile(path), 4, seed)) << engine.lastError();
    return engine.result().pattern;
}

// The take of a player who plays `played` at playBpm, scored against `target` played at its own tempo.
NoteScoring::TakeScore scorePlayer(const NotePattern::Pattern& target, NotePattern::Pattern played, const float playBpm,
                                   const bool clickOn, const Id id)
{
    played.bpm = playBpm;
    PhrasePlayer player{kSampleRate};
    player.start(played, playBpm, id);
    std::vector<float> audio(player.lengthSamples() + 4800);
    for (size_t i = 0; i < audio.size(); i += 16)
    {
        player.process(std::span<float>{audio}.subspan(i, std::min<size_t>(16, audio.size() - i)));
    }
    NoteMatching::TakeSetup setup;
    setup.pattern = target;
    setup.bpm = target.bpm;
    setup.clickOn = clickOn;
    setup.downbeatSeconds = target.pickupBeats * 60.f / target.bpm;
    const auto notes = segment(audio, id);
    return NoteScoring::scoreTake(setup, NoteMatching::matchNotes(setup, notes));
}
}

TEST(PhrasePlayerAnalysisTest, ARenderedLessonScoresFullMarksAgainstItself)
{
    for (const char* stem : {"a06-major-scale", "b06-syncopation", "c03-levels", "d03-melody-pickup"})
    {
        SCOPED_TRACE(stem);
        const auto pattern = loadLesson(stem, Id::Guitar, 1);
        const auto score = scorePlayer(pattern, pattern, pattern.bpm, true, Id::Guitar);
        EXPECT_FLOAT_EQ(score.notes, 100.f);
        EXPECT_GT(score.rhythm, 99.f);
        EXPECT_GT(score.dynamics, 95.f);
        EXPECT_TRUE(score.passed);
    }
}

TEST(PhrasePlayerAnalysisTest, WrongNotesCostOnlyTheNotesScore)
{
    const auto target = loadLesson("a06-major-scale", Id::Guitar, 1);
    auto played = target;
    played.notes[1].pitch += 2;
    played.notes[4].pitch += 1;
    const auto score = scorePlayer(target, played, target.bpm, true, Id::Guitar);
    EXPECT_NEAR(score.notes, 100.f * static_cast<float>(target.noteCount - 2) / static_cast<float>(target.noteCount),
                0.1f);
    EXPECT_GT(score.rhythm, 99.f);
}

TEST(PhrasePlayerAnalysisTest, ASlowerPlayerIsOnlyForgivenWithoutTheClick)
{
    const auto target = loadLesson("b06-syncopation", Id::Guitar, 1);
    const float slower = target.bpm * 0.85f;
    const auto free = scorePlayer(target, target, slower, false, Id::Guitar);
    EXPECT_GT(free.rhythm, 95.f);
    EXPECT_NEAR(free.tempoRatio, 0.85f, 0.02f);
    EXPECT_LT(scorePlayer(target, target, slower, true, Id::Guitar).rhythm, 60.f);
}

TEST(PhrasePlayerAnalysisTest, FlatPlayingOfALevelLessonLowersOnlyTheDynamicsScore)
{
    const auto target = loadLesson("c03-levels", Id::Guitar, 1);
    auto played = target;
    for (size_t i = 0; i < played.noteCount; ++i)
    {
        played.notes[i].velocity = 90;
    }
    const auto score = scorePlayer(target, played, target.bpm, true, Id::Guitar);
    EXPECT_FLOAT_EQ(score.notes, 100.f);
    EXPECT_GT(score.rhythm, 99.f);
    EXPECT_LT(score.dynamics, 70.f);
}

TEST(PhrasePlayerAnalysisTest, TheOtherPresetsLessonsComeBackAsTheirPatterns)
{
    expectRecovered("a03-pentatonic-steps", Id::Nylon, 3);
    expectRecovered("b05-dotted", Id::Ukulele, 3);
    expectRecovered("a06-major-scale", Id::MusicBox, 3);
}
