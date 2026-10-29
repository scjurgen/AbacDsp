#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <thread>

#include "impl/ListenAndPlayImpl.h"

namespace
{
constexpr size_t kBlock{16};
constexpr float kSampleRate{48000.f};
constexpr float kFastBpm{200.f};

using Impl = ListenAndPlayImpl<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;
using State = TakeSession::State;

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

std::string lesson(const std::string& stem)
{
    return readFile(std::filesystem::path{LISTENANDPLAY_LESSONS_DIR} / (stem + ".lua"));
}

constexpr std::string_view kFixedLesson = R"(SetName("Fixed")
SetPatternBars(2)
SetTonic(60)
ClearPattern()
AddNote(0, 60, 1)
AddNote(1, 62, 1)
AddNote(2, 64, 1)
AddNote(3, 65, 1)
AddNote(4, 67, 2)
)";

struct Rig
{
    Impl impl{kSampleRate};
    Buffer in{};
    Buffer out{};
    size_t blocks{0};

    Rig()
    {
        impl.setImportResolver(resolveFromLessonsDir);
        impl.setBpm(kFastBpm);
    }

    void block()
    {
        impl.processBlock(in, out);
        ++blocks;
    }

    void runBlocks(const size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            block();
        }
    }

    bool runUntil(const State wanted, const size_t limitBlocks = 200000)
    {
        for (size_t i = 0; i < limitBlocks; ++i)
        {
            if (impl.session().state() == wanted)
            {
                return true;
            }
            block();
        }
        return false;
    }

    float outputPeak()
    {
        float peak = 0.f;
        for (size_t i = 0; i < kBlock; ++i)
        {
            peak = std::max({peak, std::abs(out(i, 0)), std::abs(out(i, 1))});
        }
        return peak;
    }
};

bool waitForResult(Impl& impl)
{
    for (int i = 0; i < 1500; ++i)
    {
        if (impl.worker().latestResult() != nullptr)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}
}

TEST(ListenAndPlayImplTest, ALessonScriptBecomesThePhraseOnTheNextBlock)
{
    Rig rig;
    EXPECT_FALSE(rig.impl.session().hasPhrase());
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    EXPECT_EQ(rig.impl.lessonName(), "Fixed");
    rig.block();
    ASSERT_TRUE(rig.impl.session().hasPhrase());
    EXPECT_EQ(rig.impl.session().phrase().noteCount, 5u);
}

TEST(ListenAndPlayImplTest, AFailingScriptReportsTheErrorAndKeepsThePreviousPhrase)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    EXPECT_FALSE(rig.impl.setScript("AddNote(99, 60, 1)\n"));
    EXPECT_TRUE(rig.impl.hasScriptError());
    EXPECT_FALSE(rig.impl.scriptError().empty());
    rig.block();
    EXPECT_EQ(rig.impl.session().phrase().noteCount, 5u);
}

TEST(ListenAndPlayImplTest, TheCurriculumLessonsLoadThroughTheLibrary)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(lesson("d03-melody-pickup"))) << rig.impl.scriptError();
    rig.block();
    EXPECT_GT(rig.impl.session().phrase().pickupBeats, 0.f);
}

TEST(ListenAndPlayImplTest, TheInstrumentSetsTheRangeOfTheLessonsAndReloadsThem)
{
    Rig rig;
    const auto source = lesson("a06-major-scale");
    ASSERT_TRUE(rig.impl.setScript(source));
    rig.impl.setInstrument(1);
    rig.impl.poll();
    rig.block();
    const auto& bass = KsInstrument::presetOf(KsInstrument::Id::Bass);
    const auto& phrase = rig.impl.session().phrase();
    for (size_t i = 0; i < phrase.noteCount; ++i)
    {
        EXPECT_GE(phrase.notes[i].pitch, bass.lowestNote);
        EXPECT_LE(phrase.notes[i].pitch, bass.highestNote());
    }
}

TEST(ListenAndPlayImplTest, NextDrawsAnotherPhraseOfTheSameLesson)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(lesson("a06-major-scale")));
    rig.block();
    const auto first = rig.impl.session().phrase();
    bool changed = false;
    for (int press = 0; press < 5 && !changed; ++press)
    {
        rig.impl.setNext(true);
        rig.impl.poll();
        rig.block();
        const auto& phrase = rig.impl.session().phrase();
        for (size_t i = 0; i < phrase.noteCount; ++i)
        {
            changed = changed || phrase.notes[i].pitch != first.notes[i].pitch;
        }
    }
    EXPECT_TRUE(changed);
}

TEST(ListenAndPlayImplTest, PollDoesNothingWithoutARequest)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(lesson("a06-major-scale")));
    rig.block();
    const auto first = rig.impl.session().phrase();
    rig.impl.poll();
    rig.block();
    for (size_t i = 0; i < first.noteCount; ++i)
    {
        EXPECT_EQ(rig.impl.session().phrase().notes[i].pitch, first.notes[i].pitch);
    }
}

TEST(ListenAndPlayImplTest, TheTakeStartsWithThePhraseAndTheVolumeScalesIt)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    float loud = 0.f;
    for (int i = 0; i < 200; ++i)
    {
        rig.block();
        loud = std::max(loud, rig.outputPeak());
    }
    EXPECT_EQ(rig.impl.session().state(), State::Listening);
    EXPECT_GT(loud, 1e-3f);

    Rig quiet;
    ASSERT_TRUE(quiet.impl.setScript(kFixedLesson));
    quiet.impl.setOutputVolume(-40.f);
    quiet.block();
    quiet.impl.setPlay(true);
    float soft = 0.f;
    for (int i = 0; i < 200; ++i)
    {
        quiet.block();
        soft = std::max(soft, quiet.outputPeak());
    }
    EXPECT_NEAR(soft / loud, 0.01f, 0.002f);
}

TEST(ListenAndPlayImplTest, StopEndsAListeningTake)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    rig.runBlocks(100);
    rig.impl.setStop(true);
    rig.runBlocks(10);
    EXPECT_EQ(rig.impl.session().state(), State::Idle);
}

TEST(ListenAndPlayImplTest, PlayingAnEmptyTakeEndsInAReviewWithAZeroScore)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    ASSERT_TRUE(rig.runUntil(State::Analysing));
    EXPECT_EQ(rig.impl.statusText(), "Analysing...");
    ASSERT_TRUE(waitForResult(rig.impl));
    ASSERT_TRUE(rig.runUntil(State::Review));
    const auto result = rig.impl.takeNewResult();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->score.overall, 0.f);
    EXPECT_FALSE(result->score.passed);
    EXPECT_NE(rig.impl.statusText().find("Fixed: 0 %"), std::string::npos);
}

TEST(ListenAndPlayImplTest, TheStatusFollowsTheTake)
{
    Rig rig;
    EXPECT_EQ(rig.impl.statusText(), "No lesson loaded");
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    EXPECT_EQ(rig.impl.statusText(), "Press Play to hear the phrase");
    rig.impl.setPlay(true);
    rig.block();
    EXPECT_EQ(rig.impl.statusText(), "Listen");
    ASSERT_TRUE(rig.runUntil(State::CountIn));
    EXPECT_EQ(rig.impl.statusText().substr(0, 8), "Count-in");
    ASSERT_TRUE(rig.runUntil(State::Capturing));
    EXPECT_EQ(rig.impl.statusText(), "Play!");
}

TEST(ListenAndPlayImplTest, TheTempoSettingIsUsedForTheTake)
{
    Rig rig;
    rig.impl.setBpm(120.f);
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    rig.block();
    EXPECT_NEAR(rig.impl.session().timeline().samplesPerBeat, 24000.0, 1e-6);
}

TEST(ListenAndPlayImplTest, TheInputIsRecordedAsTheMeanOfBothChannels)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    ASSERT_TRUE(rig.runUntil(State::Capturing));
    for (size_t i = 0; i < kBlock; ++i)
    {
        rig.in(i, 0) = 0.5f;
        rig.in(i, 1) = 0.3f;
    }
    ASSERT_TRUE(rig.runUntil(State::Analysing));
    ASSERT_FALSE(rig.impl.session().captured().empty());
    EXPECT_NEAR(rig.impl.session().captured().back(), 0.4f, 1e-6f);
}

namespace
{
// Plays the loaded phrase back as MIDI notes in time, then waits for the analysis and the review.
void playPerfectMidiTake(Rig& rig)
{
    rig.impl.setInput(1);
    rig.impl.setPlay(true);
    rig.block();
    const size_t playBlock = rig.blocks - 1;
    const auto& t = rig.impl.session().timeline();
    const auto& phrase = rig.impl.session().phrase();
    struct Event
    {
        size_t sample;
        uint8_t note;
        uint8_t velocity;
        bool on;
    };
    std::vector<Event> events;
    for (size_t i = 0; i < phrase.noteCount; ++i)
    {
        const auto& note = phrase.notes[i];
        const auto on =
            t.downbeat + static_cast<size_t>(std::llround(static_cast<double>(note.positionBeats) * t.samplesPerBeat));
        const auto length =
            static_cast<size_t>(std::llround(static_cast<double>(note.durationBeats) * t.samplesPerBeat));
        events.push_back({on, note.pitch, 100, true});
        events.push_back({on + length - 400, note.pitch, 0, false});
    }
    std::ranges::sort(events, {}, &Event::sample);
    size_t next = 0;
    while (rig.impl.session().state() != State::Analysing && rig.blocks < 200000)
    {
        const size_t blockStart = (rig.blocks - playBlock) * kBlock;
        while (next < events.size() && events[next].sample < blockStart + kBlock)
        {
            const std::array<uint8_t, 3> message{static_cast<uint8_t>(events[next].on ? 0x90 : 0x80), events[next].note,
                                                 events[next].velocity};
            rig.impl.processMidiEvent(message, static_cast<int>(events[next].sample - blockStart));
            ++next;
        }
        rig.block();
    }
    waitForResult(rig.impl);
    rig.runUntil(State::Review);
}
}

TEST(ListenAndPlayImplTest, APerfectMidiTakePassesTheLesson)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    playPerfectMidiTake(rig);
    const auto result = rig.impl.worker().latestResult();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->score.notes, 100.f);
    EXPECT_GT(result->score.rhythm, 99.f);
    EXPECT_TRUE(result->score.passed);
}

TEST(ListenAndPlayImplTest, ReplayPlaysTheTakeBackAfterTheReview)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    rig.block();
    ASSERT_TRUE(rig.runUntil(State::Capturing));
    for (size_t i = 0; i < kBlock; ++i)
    {
        rig.in(i, 0) = 0.2f;
        rig.in(i, 1) = 0.2f;
    }
    ASSERT_TRUE(rig.runUntil(State::Analysing));
    ASSERT_TRUE(waitForResult(rig.impl));
    ASSERT_TRUE(rig.runUntil(State::Review));
    rig.in = Buffer{};
    rig.impl.setReplay(true);
    rig.block();
    EXPECT_EQ(rig.impl.session().state(), State::Replaying);
    rig.runBlocks(10);
    EXPECT_NEAR(rig.outputPeak(), 0.2f, 1e-5f);
}

TEST(ListenAndPlayImplTest, MomentaryButtonsActOnPressesOnly)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(false);
    rig.runBlocks(10);
    EXPECT_EQ(rig.impl.session().state(), State::Idle);
    rig.impl.setPlay(true);
    rig.block();
    EXPECT_EQ(rig.impl.session().state(), State::Listening);
}

namespace
{
std::vector<std::string> libraryNames()
{
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator{LISTENANDPLAY_LESSONS_DIR})
    {
        names.push_back(entry.path().stem().string());
    }
    return names;
}
}

TEST(ListenAndPlayImplTest, TheLessonRowsFollowTheCurriculumAndTheProgress)
{
    Rig rig;
    rig.impl.setLessonLister(libraryNames);
    const auto rows = rig.impl.lessonRows();
    ASSERT_GE(rows.size(), 30u);
    EXPECT_EQ(rows.front().stem, "a01-triad-3");
    EXPECT_TRUE(rows.front().recommended);
    EXPECT_TRUE(rows.front().unlocked);
    EXPECT_EQ(rows.front().focus, "Pitch");
    EXPECT_EQ(rows.front().group, 'a');
    EXPECT_FALSE(rows[1].unlocked);
    EXPECT_FALSE(std::ranges::any_of(rows, [](const auto& row) { return row.passed || row.current; }));
    const auto firstRhythm = std::ranges::find(rows, 'b', &ListenPlayDisplay::LessonRow::group);
    ASSERT_NE(firstRhythm, rows.end());
    EXPECT_TRUE(firstRhythm->unlocked);
    EXPECT_FALSE(firstRhythm->recommended);
}

TEST(ListenAndPlayImplTest, APreparedLessonNameIsUsedOnceAndAnyOtherScriptIsTheUsersOwn)
{
    Rig rig;
    rig.impl.prepareLesson("a01-triad-3");
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    EXPECT_EQ(rig.impl.lessonStem(), "a01-triad-3");
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    EXPECT_TRUE(rig.impl.lessonStem().empty());
    rig.impl.setLessonLister(libraryNames);
    rig.impl.prepareLesson("a02-triad-4");
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    const auto rows = rig.impl.lessonRows();
    EXPECT_TRUE(std::ranges::any_of(rows, [](const auto& row) { return row.current && row.stem == "a02-triad-4"; }));
}

TEST(ListenAndPlayImplTest, OpeningALessonIsRememberedAsTheLastOne)
{
    Rig rig;
    rig.impl.prepareLesson("a03-pentatonic-steps");
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    EXPECT_EQ(rig.impl.lastLessonStem(), "a03-pentatonic-steps");
    EXPECT_TRUE(rig.impl.consumeProgressDirty());
    EXPECT_FALSE(rig.impl.consumeProgressDirty());
    EXPECT_NE(rig.impl.progressJson().find("a03-pentatonic-steps"), std::string::npos);
}

TEST(ListenAndPlayImplTest, AFinishedTakeIsRecordedAsProgressOfItsLesson)
{
    Rig rig;
    rig.impl.setLessonLister(libraryNames);
    rig.impl.prepareLesson("a01-triad-3");
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    static_cast<void>(rig.impl.consumeProgressDirty());
    const auto version = rig.impl.lessonRowsVersion();
    playPerfectMidiTake(rig);
    const auto result = rig.impl.takeNewResult();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->lessonStem, "a01-triad-3");
    EXPECT_TRUE(rig.impl.consumeProgressDirty());
    EXPECT_GT(rig.impl.lessonRowsVersion(), version);
    const auto rows = rig.impl.lessonRows();
    EXPECT_TRUE(rows.front().passed);
    EXPECT_EQ(rows.front().attempts, 1u);
    EXPECT_GT(rows.front().best, 90.f);
    EXPECT_TRUE(rows[1].unlocked);
    EXPECT_TRUE(rows[1].recommended);
    EXPECT_EQ(rig.impl.recommendedLesson(), "a02-triad-4");
}

TEST(ListenAndPlayImplTest, ATakeOfAScriptOfTheUsersOwnLeavesTheProgressAlone)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    static_cast<void>(rig.impl.consumeProgressDirty());
    playPerfectMidiTake(rig);
    ASSERT_NE(rig.impl.takeNewResult(), nullptr);
    EXPECT_FALSE(rig.impl.consumeProgressDirty());
}

TEST(ListenAndPlayImplTest, SavedProgressComesBackThroughTheJson)
{
    Rig first;
    first.impl.setLessonLister(libraryNames);
    first.impl.prepareLesson("a01-triad-3");
    ASSERT_TRUE(first.impl.setScript(kFixedLesson));
    first.block();
    playPerfectMidiTake(first);
    static_cast<void>(first.impl.takeNewResult());
    Rig second;
    second.impl.setLessonLister(libraryNames);
    ASSERT_TRUE(second.impl.loadProgress(first.impl.progressJson()));
    EXPECT_EQ(second.impl.lastLessonStem(), "a01-triad-3");
    EXPECT_TRUE(second.impl.lessonRows().front().passed);
    EXPECT_FALSE(second.impl.loadProgress("garbage"));
}

TEST(ListenAndPlayImplTest, ThePianoRollTellsNothingAboutThePhraseUntilTheTakeIsScored)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    const auto expectHidden = [&](const ListenPlayDisplay::Phase phase)
    {
        const auto roll = rig.impl.pianoRollSnapshot();
        EXPECT_EQ(roll.phase, phase);
        EXPECT_FALSE(roll.revealed);
        EXPECT_EQ(roll.targetCount, 0u);
        EXPECT_EQ(roll.playedCount, 0u);
        EXPECT_TRUE(roll.hasPhrase);
        EXPECT_TRUE(roll.hasReference);
        EXPECT_FALSE(rig.impl.scoreSnapshot().hasResult);
    };
    expectHidden(ListenPlayDisplay::Phase::Idle);
    rig.impl.setPlay(true);
    rig.runBlocks(2000);
    expectHidden(ListenPlayDisplay::Phase::Listening);
    ASSERT_TRUE(rig.runUntil(State::CountIn));
    rig.block();
    expectHidden(ListenPlayDisplay::Phase::CountIn);
    EXPECT_GE(rig.impl.pianoRollSnapshot().countInBeat, 1);
    EXPECT_EQ(rig.impl.pianoRollSnapshot().countInBeats, 4u);
    ASSERT_TRUE(rig.runUntil(State::Capturing));
    expectHidden(ListenPlayDisplay::Phase::Capturing);
    ASSERT_TRUE(rig.runUntil(State::Analysing));
    expectHidden(ListenPlayDisplay::Phase::Analysing);
    EXPECT_FLOAT_EQ(rig.impl.pianoRollSnapshot().progress, 1.f);
}

TEST(ListenAndPlayImplTest, TheProgressBarFollowsTheListenAndTheTake)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    EXPECT_FLOAT_EQ(rig.impl.pianoRollSnapshot().progress, 0.f);
    rig.impl.setPlay(true);
    rig.runBlocks(1500);
    const float early = rig.impl.pianoRollSnapshot().progress;
    rig.runBlocks(3000);
    const float later = rig.impl.pianoRollSnapshot().progress;
    EXPECT_GT(early, 0.f);
    EXPECT_GT(later, early);
    EXPECT_LE(later, 1.f);
    ASSERT_TRUE(rig.runUntil(State::Capturing));
    rig.runBlocks(10);
    EXPECT_LT(rig.impl.pianoRollSnapshot().progress, 0.2f);
}

TEST(ListenAndPlayImplTest, AfterATakeThePianoRollAndTheScoreShowTheResult)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    EXPECT_FALSE(rig.impl.scoreSnapshot().hasResult);
    playPerfectMidiTake(rig);
    const auto roll = rig.impl.pianoRollSnapshot();
    EXPECT_TRUE(roll.revealed);
    EXPECT_EQ(roll.targetCount, 5u);
    EXPECT_EQ(roll.target[4].pitch, 67);
    EXPECT_FLOAT_EQ(roll.patternBeats, 8.f);
    EXPECT_EQ(roll.phase, ListenPlayDisplay::Phase::Review);
    ASSERT_EQ(roll.playedCount, 5u);
    EXPECT_NEAR(roll.played[2].startBeats, roll.target[2].startBeats, 0.01f);
    EXPECT_GT(roll.played[2].credit, 0.99f);
    EXPECT_TRUE(roll.played[2].pitchRight);
    const auto score = rig.impl.scoreSnapshot();
    EXPECT_TRUE(score.hasResult);
    EXPECT_TRUE(score.passed);
    EXPECT_GT(score.overall, 95.f);
    EXPECT_EQ(score.matched, 5u);
}

TEST(ListenAndPlayImplTest, ADifferentPhraseHidesTheOldResult)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(lesson("a06-major-scale")));
    rig.block();
    playPerfectMidiTake(rig);
    ASSERT_TRUE(rig.impl.scoreSnapshot().hasResult);
    bool hidden = false;
    for (int press = 0; press < 5 && !hidden; ++press)
    {
        rig.impl.setNext(true);
        rig.impl.poll();
        rig.block();
        hidden = !rig.impl.scoreSnapshot().hasResult && !rig.impl.pianoRollSnapshot().revealed;
    }
    EXPECT_TRUE(hidden);
}

TEST(ListenAndPlayImplTest, AnotherTakeOfTheSamePhraseHidesItAgainUntilItIsScored)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    playPerfectMidiTake(rig);
    ASSERT_TRUE(rig.impl.pianoRollSnapshot().revealed);
    rig.impl.setPlay(true);
    rig.runBlocks(2000);
    EXPECT_FALSE(rig.impl.pianoRollSnapshot().revealed);
    EXPECT_EQ(rig.impl.pianoRollSnapshot().targetCount, 0u);
    EXPECT_FALSE(rig.impl.scoreSnapshot().hasResult);
    ASSERT_TRUE(rig.runUntil(State::Capturing));
    EXPECT_FALSE(rig.impl.pianoRollSnapshot().revealed);
}

TEST(ListenAndPlayImplTest, ThePhraseCanBeHeardAgainOnlyAfterATakeOfIt)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(lesson("a06-major-scale")));
    rig.block();
    rig.impl.setListen(true);
    rig.runBlocks(200);
    EXPECT_EQ(rig.impl.session().state(), State::Idle);
    EXPECT_LT(rig.outputPeak(), 1e-6f);
    playPerfectMidiTake(rig);
    rig.impl.setListen(true);
    rig.runBlocks(200);
    EXPECT_EQ(rig.impl.session().state(), State::Listening);
    EXPECT_TRUE(rig.impl.pianoRollSnapshot().revealed);
    rig.impl.setStop(true);
    rig.runBlocks(20);
    bool changed = false;
    for (int press = 0; press < 5 && !changed; ++press)
    {
        const auto before = rig.impl.session().phrase();
        rig.impl.setNext(true);
        rig.impl.poll();
        rig.block();
        changed = !ListenPlayDisplay::samePhrase(before, rig.impl.session().phrase());
    }
    ASSERT_TRUE(changed);
    EXPECT_EQ(rig.impl.session().state(), State::Idle);
    rig.impl.setListen(true);
    rig.runBlocks(200);
    EXPECT_EQ(rig.impl.session().state(), State::Idle);
}

TEST(ListenAndPlayImplTest, TheReferenceButtonPlaysTheKeyNoteAndNothingElse)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setReference(true);
    float peak = 0.f;
    for (int i = 0; i < 300; ++i)
    {
        rig.block();
        peak = std::max(peak, rig.outputPeak());
    }
    EXPECT_EQ(rig.impl.session().state(), State::Listening);
    EXPECT_EQ(rig.impl.statusText(), "Reference tone");
    EXPECT_GT(peak, 1e-3f);
    EXPECT_FALSE(rig.impl.pianoRollSnapshot().revealed);
    EXPECT_EQ(rig.impl.pianoRollSnapshot().targetCount, 0u);
    ASSERT_TRUE(rig.runUntil(State::Idle));
    EXPECT_FALSE(rig.impl.session().referencePlaying());
    EXPECT_EQ(rig.impl.session().takeId(), 0u);
}

TEST(ListenAndPlayImplTest, TheReferenceIsIgnoredDuringATake)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.block();
    rig.impl.setPlay(true);
    rig.runBlocks(100);
    rig.impl.setReference(true);
    rig.runBlocks(10);
    EXPECT_FALSE(rig.impl.session().referencePlaying());
    EXPECT_EQ(rig.impl.statusText(), "Listen");
}

TEST(ListenAndPlayImplTest, ALessonWithoutAKeyNoteSaysSoAndStaysSilent)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript("SetName('No key')\nSetPatternBars(1)\nClearPattern()\nAddNote(0, 60, 1)\n"));
    rig.block();
    EXPECT_FALSE(rig.impl.pianoRollSnapshot().hasReference);
    rig.impl.setReference(true);
    float peak = 0.f;
    for (int i = 0; i < 100; ++i)
    {
        rig.block();
        peak = std::max(peak, rig.outputPeak());
    }
    EXPECT_EQ(rig.impl.session().state(), State::Idle);
    EXPECT_EQ(rig.impl.statusText(), "This lesson has no reference tone");
    EXPECT_LT(peak, 1e-6f);
    rig.impl.setPlay(true);
    rig.block();
    EXPECT_EQ(rig.impl.statusText(), "Listen");
}

namespace
{
float peakOverBlocks(Rig& rig, const size_t count)
{
    float peak = 0.f;
    for (size_t i = 0; i < count; ++i)
    {
        rig.block();
        peak = std::max(peak, rig.outputPeak());
    }
    return peak;
}
}

TEST(ListenAndPlayImplTest, TheLiveInputAlwaysPassesThroughScaledByTheInputGain)
{
    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    for (size_t i = 0; i < kBlock; ++i)
    {
        rig.in(i, 0) = 0.2f;
        rig.in(i, 1) = -0.1f;
    }
    rig.runBlocks(2);
    EXPECT_NEAR(rig.out(0, 0), 0.2f, 1e-6f);
    EXPECT_NEAR(rig.out(0, 1), -0.1f, 1e-6f);

    rig.impl.setInputGain(-6.f);
    rig.impl.setOutputVolume(-6.f);
    rig.runBlocks(2);
    EXPECT_NEAR(rig.out(0, 0), 0.2f * 0.5012f * 0.5012f, 1e-4f);

    rig.impl.setInput(1);
    rig.runBlocks(2);
    EXPECT_NEAR(rig.out(0, 0), 0.2f * 0.5012f * 0.5012f, 1e-4f);
}

TEST(ListenAndPlayImplTest, TheInstrumentLevelScalesThePhraseAndNotTheInput)
{
    Rig loud;
    ASSERT_TRUE(loud.impl.setScript(kFixedLesson));
    loud.block();
    loud.impl.setPlay(true);
    const float full = peakOverBlocks(loud, 200);

    Rig soft;
    ASSERT_TRUE(soft.impl.setScript(kFixedLesson));
    soft.impl.setInstrumentLevel(-20.f);
    soft.block();
    soft.impl.setPlay(true);
    const float low = peakOverBlocks(soft, 200);
    ASSERT_GT(full, 1e-3f);
    EXPECT_NEAR(low / full, 0.1f, 0.01f);
}

TEST(ListenAndPlayImplTest, TheClickLevelScalesTheClickAndNotThePhrase)
{
    const auto countInPeak = [](const float clickDb)
    {
        Rig rig;
        EXPECT_TRUE(rig.impl.setScript(kFixedLesson));
        rig.impl.setClickLevel(clickDb);
        rig.block();
        rig.impl.setPlay(true);
        EXPECT_TRUE(rig.runUntil(State::CountIn));
        return peakOverBlocks(rig, 300);
    };
    const float full = countInPeak(0.f);
    ASSERT_GT(full, 1e-3f);
    EXPECT_NEAR(countInPeak(-20.f) / full, 0.1f, 0.01f);

    Rig rig;
    ASSERT_TRUE(rig.impl.setScript(kFixedLesson));
    rig.impl.setClickLevel(-60.f);
    rig.block();
    rig.impl.setPlay(true);
    EXPECT_GT(peakOverBlocks(rig, 200), 1e-3f);
}
