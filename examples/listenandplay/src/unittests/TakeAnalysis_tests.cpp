#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "Numbers/Convert.h"
#include "TestTakes.h"
#include "impl/PhrasePlayer.h"
#include "impl/TakeAnalysis.h"

using namespace TestTakes;
using TakeAnalysis::TakeResult;
using State = TakeSession::State;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kChunk{64};

float hzOfNote(const float note)
{
    return Convert::noteToFrequency<float>(note, 440.f);
}

std::vector<TakeSession::MidiEvent> midiEventsFor(const NoteMatching::TakeSetup& setup)
{
    std::vector<TakeSession::MidiEvent> events;
    for (size_t i = 0; i < setup.pattern.noteCount; ++i)
    {
        const auto& note = setup.pattern.notes[i];
        const auto on =
            static_cast<size_t>((setup.downbeatSeconds + note.positionBeats * kSecondsPerBeat) * kSampleRate);
        const auto off = on + static_cast<size_t>(note.durationBeats * kSecondsPerBeat * kSampleRate);
        events.push_back({on, note.pitch, note.velocity, true});
        events.push_back({off, note.pitch, 0, false});
    }
    std::ranges::sort(events, {}, &TakeSession::MidiEvent::sample);
    return events;
}

TakeSession::Settings settingsFor(const bool midi)
{
    TakeSession::Settings settings;
    settings.bpm = kBpm;
    settings.midiInput = midi;
    return settings;
}

std::vector<float> render(const NotePattern::Pattern& pattern)
{
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, KsInstrument::Id::Guitar);
    std::vector<float> out(player.lengthSamples() + 2000);
    for (size_t i = 0; i < out.size(); i += 16)
    {
        player.process(std::span<float>{out}.subspan(i, std::min<size_t>(16, out.size() - i)));
    }
    return out;
}

// Plays a whole take into the session, the answer coming from `user` placed at its downbeat.
void playTake(TakeSession& session, const std::vector<float>& user, const size_t userDownbeatOffset)
{
    const auto& t = session.timeline();
    const size_t userStart = t.downbeat - userDownbeatOffset;
    std::vector<float> in(kChunk);
    std::vector<float> out(kChunk);
    for (size_t at = 0; at < t.captureEnd + kChunk; at += kChunk)
    {
        for (size_t i = 0; i < kChunk; ++i)
        {
            const size_t time = at + i;
            in[i] = time >= userStart && time - userStart < user.size() ? user[time - userStart] : 0.f;
        }
        session.process(in, out);
    }
}

template <typename Predicate>
bool waitFor(Predicate&& done)
{
    for (int i = 0; i < 1500; ++i)
    {
        if (done())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}
}

TEST(TakeAnalysisTest, TheSegmenterListensForThePhraseAndAnOctaveBeyondWhenTheOctaveIsFree)
{
    auto pattern = makePattern();
    const auto free = TakeAnalysis::segmenterConfigFor(pattern);
    EXPECT_NEAR(free.minHz, hzOfNote(60.f - 12.f - 1.f), 0.01f);
    EXPECT_NEAR(free.maxHz, hzOfNote(67.f + 12.f + 1.f), 0.01f);
    pattern.scoring.octaveFree = false;
    const auto exact = TakeAnalysis::segmenterConfigFor(pattern);
    EXPECT_NEAR(exact.minHz, hzOfNote(59.f), 0.01f);
    EXPECT_NEAR(exact.maxHz, hzOfNote(68.f), 0.01f);
}

TEST(TakeAnalysisTest, MidiNotesPairEachNoteOnWithItsNoteOff)
{
    const std::vector<TakeSession::MidiEvent> events{{4800, 60, 100, true},
                                                     {9600, 62, 50, true},
                                                     {14400, 60, 0, false},
                                                     {24000, 62, 0, true},
                                                     {38400, 64, 80, true}};
    const auto notes = TakeAnalysis::midiNotesOf(events, kSampleRate, 48000);
    ASSERT_EQ(notes.size(), 3u);
    EXPECT_FLOAT_EQ(notes[0].onsetSeconds, 0.1f);
    EXPECT_FLOAT_EQ(notes[0].offsetSeconds, 0.3f);
    EXPECT_FLOAT_EQ(notes[0].midiNote, 60.f);
    EXPECT_FLOAT_EQ(notes[1].offsetSeconds, 0.5f);
    EXPECT_FLOAT_EQ(notes[2].offsetSeconds, 1.f);
    EXPECT_TRUE(notes[0].pitched());
    EXPECT_NEAR(notes[0].attackRmsDb - notes[1].attackRmsDb, 50.f * KsInstrument::kDbPerVelocityStep, 1e-4f);
}

TEST(TakeAnalysisTest, APerfectMidiTakeScoresFullMarks)
{
    const auto setup = makeSetup(makePattern());
    const auto events = midiEventsFor(setup);
    const auto result = TakeAnalysis::analyzeMidi(events, 480000, kSampleRate, setup, "Lesson", 3);
    EXPECT_EQ(result.takeId, 3u);
    EXPECT_EQ(result.lessonName, "Lesson");
    EXPECT_FLOAT_EQ(result.score.notes, 100.f);
    EXPECT_GT(result.score.rhythm, 99.f);
    EXPECT_GT(result.score.dynamics, 99.f);
    EXPECT_TRUE(result.score.passed);
    EXPECT_NE(result.reportHtml.find("passed"), std::string::npos);
}

TEST(TakeAnalysisTest, ARenderedPerformanceOfThePhraseIsScoredFromItsAudio)
{
    auto setup = makeSetup(makePattern());
    setup.downbeatSeconds = 0.f;
    const auto audio = render(setup.pattern);
    const auto result = TakeAnalysis::analyzeAudio(audio, kSampleRate, setup, "Lesson", 1);
    EXPECT_FLOAT_EQ(result.score.notes, 100.f);
    EXPECT_GT(result.score.rhythm, 99.f);
    EXPECT_GT(result.score.dynamics, 90.f);
    EXPECT_EQ(result.played.size(), 8u);
}

TEST(TakeAnalysisTest, TheWorkerAnalysesAFinishedAudioTakeAndHandsOutTheResultOnce)
{
    TakeSession session{kSampleRate};
    const auto pattern = makePattern();
    ASSERT_TRUE(session.setPhrase(pattern));
    TakeAnalysis::Worker worker{session, kSampleRate};
    worker.setLessonName("Scales");
    ASSERT_TRUE(session.startTake(settingsFor(false)));
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, KsInstrument::Id::Guitar);
    playTake(session, render(pattern), player.downbeatSample());
    ASSERT_EQ(session.state(), State::Analysing);
    ASSERT_TRUE(waitFor([&] { return worker.latestResult() != nullptr; }));
    const auto result = worker.takeNewResult();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->lessonName, "Scales");
    EXPECT_EQ(result->takeId, 1u);
    EXPECT_FLOAT_EQ(result->score.notes, 100.f);
    EXPECT_GT(result->score.rhythm, 99.f);
    EXPECT_EQ(worker.takeNewResult(), nullptr);
    EXPECT_NE(worker.latestResult(), nullptr);
    std::vector<float> in(kChunk);
    std::vector<float> out(kChunk);
    session.process(in, out);
    EXPECT_EQ(session.state(), State::Review);
}

TEST(TakeAnalysisTest, TheResultCarriesTheAudioOnlyWhileTheWorkerKeepsIt)
{
    for (const bool keep : {false, true})
    {
        TakeSession session{kSampleRate};
        const auto pattern = makePattern();
        ASSERT_TRUE(session.setPhrase(pattern));
        TakeAnalysis::Worker worker{session, kSampleRate};
        worker.setKeepAudio(keep);
        ASSERT_TRUE(session.startTake(settingsFor(false)));
        PhrasePlayer player{kSampleRate};
        player.start(pattern, kBpm, KsInstrument::Id::Guitar);
        playTake(session, render(pattern), player.downbeatSample());
        ASSERT_TRUE(waitFor([&] { return worker.latestResult() != nullptr; }));
        const auto result = worker.latestResult();
        EXPECT_FLOAT_EQ(result->sampleRate, kSampleRate);
        EXPECT_FALSE(result->midi);
        EXPECT_EQ(result->audio.size(), keep ? session.captured().size() : 0u);
    }
}

TEST(TakeAnalysisTest, TheWorkerScoresAMidiTakeWithoutLookingAtTheAudio)
{
    TakeSession session{kSampleRate};
    const auto pattern = makePattern();
    ASSERT_TRUE(session.setPhrase(pattern));
    TakeAnalysis::Worker worker{session, kSampleRate};
    ASSERT_TRUE(session.startTake(settingsFor(true)));
    const auto& t = session.timeline();
    const auto events = midiEventsFor(session.setup());
    const size_t startOffset = t.captureStart;
    std::vector<float> silence(kChunk);
    std::vector<float> out(kChunk);
    size_t next = 0;
    for (size_t at = 0; at < t.captureEnd + kChunk; at += kChunk)
    {
        while (next < events.size() && startOffset + events[next].sample < at + kChunk)
        {
            const auto& e = events[next++];
            session.recordMidi(e.note, e.velocity, e.on,
                               static_cast<int>(startOffset + e.sample) - static_cast<int>(at));
        }
        session.process(silence, out);
    }
    ASSERT_TRUE(waitFor([&] { return worker.latestResult() != nullptr; }));
    const auto result = worker.latestResult();
    EXPECT_FLOAT_EQ(result->score.notes, 100.f);
    EXPECT_GT(result->score.rhythm, 99.f);
    EXPECT_TRUE(result->score.passed);
}
