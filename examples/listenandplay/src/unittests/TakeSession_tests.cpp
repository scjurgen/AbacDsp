#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "TestTakes.h"
#include "impl/PhrasePlayer.h"
#include "impl/TakeSession.h"

using State = TakeSession::State;
using namespace TestTakes;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kChunk{37};

TakeSession::Settings makeSettings(const bool clickOn = true)
{
    TakeSession::Settings settings;
    settings.bpm = kBpm;
    settings.clickOn = clickOn;
    return settings;
}

NotePattern::Pattern shortPattern(const float pickup = 0.f)
{
    NotePattern::Pattern pattern{};
    pattern.pickupBeats = pickup;
    pattern.bpm = kBpm;
    addNote(pattern, -pickup, 1.f, 60);
    addNote(pattern, 1.f, 1.f, 64);
    addNote(pattern, 2.f, 2.f, 67);
    return pattern;
}

// Runs the session for the given number of samples with a constant input, returning what it played.
std::vector<float> run(TakeSession& session, const size_t samples, const float input = 0.f)
{
    std::vector<float> out(samples);
    const std::vector<float> in(samples, input);
    for (size_t at = 0; at < samples; at += kChunk)
    {
        const size_t n = std::min(kChunk, samples - at);
        session.process(std::span<const float>{in}.subspan(at, n), std::span<float>{out}.subspan(at, n));
    }
    return out;
}

float peak(const std::vector<float>& x, const size_t from, const size_t to)
{
    float p = 0.f;
    for (size_t i = from; i < std::min(to, x.size()); ++i)
    {
        p = std::max(p, std::abs(x[i]));
    }
    return p;
}

// Feeds the session a source that is indexed by the session's own sample clock.
std::vector<float> runWithInput(TakeSession& session, const size_t samples, const std::vector<float>& source,
                                const size_t sourceStart)
{
    std::vector<float> out(samples);
    std::vector<float> in(kChunk);
    for (size_t at = 0; at < samples; at += kChunk)
    {
        const size_t n = std::min(kChunk, samples - at);
        for (size_t i = 0; i < n; ++i)
        {
            const size_t t = at + i;
            in[i] = t >= sourceStart && t - sourceStart < source.size() ? source[t - sourceStart] : 0.f;
        }
        session.process(std::span<const float>{in}.first(n), std::span<float>{out}.subspan(at, n));
    }
    return out;
}
}

TEST(TakeSessionTest, StartsIdleAndRefusesToPlayWithoutAPhrase)
{
    TakeSession session{kSampleRate};
    EXPECT_EQ(session.state(), State::Idle);
    EXPECT_FALSE(session.hasPhrase());
    EXPECT_FALSE(session.listen(makeSettings()));
    EXPECT_FALSE(session.startTake(makeSettings()));
    EXPECT_FALSE(session.replayTake());
}

// A take with nothing played into it, all the way to the review.
void finishATake(TakeSession& session)
{
    ASSERT_TRUE(session.startTake(makeSettings()));
    static_cast<void>(run(session, session.timeline().captureEnd + 10));
    ASSERT_EQ(session.state(), State::Analysing);
    session.finishAnalysis(session.takeId());
    static_cast<void>(run(session, 1));
    ASSERT_EQ(session.state(), State::Review);
}

TEST(TakeSessionTest, ListenIsRefusedBeforeATakeOfThePhrase)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    EXPECT_FALSE(session.listen(makeSettings()));
    EXPECT_EQ(session.state(), State::Idle);
    EXPECT_FALSE(session.hasTake());
}

TEST(TakeSessionTest, ListeningAfterATakePlaysThePhraseAndReturnsToTheReview)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    finishATake(session);
    EXPECT_TRUE(session.hasTake());
    ASSERT_TRUE(session.listen(makeSettings()));
    EXPECT_EQ(session.state(), State::Listening);
    EXPECT_FALSE(session.takeActive());
    const auto out = run(session, 1000);
    EXPECT_GT(peak(out, 0, 200), 1e-3f);
    const size_t length = session.timeline().listenEnd;
    EXPECT_GT(length, 4 * static_cast<size_t>(kSecondsPerBeat * kSampleRate));
    static_cast<void>(run(session, length));
    EXPECT_EQ(session.state(), State::Review);
    EXPECT_FALSE(session.timeline().withTake);
}

TEST(TakeSessionTest, ANewPhraseTakesTheTakeOfTheOldOneAway)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    finishATake(session);
    ASSERT_TRUE(session.setPhrase(shortPattern(1.f)));
    EXPECT_EQ(session.state(), State::Idle);
    EXPECT_FALSE(session.hasTake());
    EXPECT_FALSE(session.listen(makeSettings()));
    EXPECT_FALSE(session.replayTake());
}

TEST(TakeSessionTest, ATakeIsActiveFromItsFirstNoteUntilTheResultIsIn)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    EXPECT_FALSE(session.takeActive());
    ASSERT_TRUE(session.startTake(makeSettings()));
    EXPECT_TRUE(session.takeActive());
    const auto& t = session.timeline();
    static_cast<void>(run(session, t.captureStart + 10));
    EXPECT_TRUE(session.takeActive());
    static_cast<void>(run(session, t.captureEnd - t.captureStart));
    EXPECT_EQ(session.state(), State::Analysing);
    EXPECT_TRUE(session.takeActive());
    session.finishAnalysis(session.takeId());
    static_cast<void>(run(session, 1));
    EXPECT_FALSE(session.takeActive());
}

TEST(TakeSessionTest, TheReferencePlaysOneNoteAndGoesBackToWhereItWas)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    EXPECT_FALSE(session.referencePlaying());
    ASSERT_TRUE(session.playReference(makeSettings(), 60));
    EXPECT_EQ(session.state(), State::Listening);
    EXPECT_TRUE(session.referencePlaying());
    EXPECT_FALSE(session.takeActive());
    const auto out = run(session, 2000);
    EXPECT_GT(peak(out, 0, 400), 1e-3f);
    static_cast<void>(run(session, static_cast<size_t>(1.6 * kSampleRate)));
    EXPECT_EQ(session.state(), State::Idle);
    EXPECT_FALSE(session.referencePlaying());
    EXPECT_EQ(session.takeId(), 0u);

    finishATake(session);
    ASSERT_TRUE(session.playReference(makeSettings(), 64));
    static_cast<void>(run(session, static_cast<size_t>(1.6 * kSampleRate)));
    EXPECT_EQ(session.state(), State::Review);
    EXPECT_EQ(session.takeId(), 1u);
}

TEST(TakeSessionTest, TheReferenceNeedsAPitchAndAFreeSession)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    EXPECT_FALSE(session.playReference(makeSettings(), 0));
    ASSERT_TRUE(session.startTake(makeSettings()));
    EXPECT_FALSE(session.playReference(makeSettings(), 60));
    EXPECT_FALSE(session.referencePlaying());
}

TEST(TakeSessionTest, StopEndsTheReference)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.playReference(makeSettings(), 60));
    static_cast<void>(run(session, 500));
    session.stop();
    EXPECT_EQ(session.state(), State::Idle);
    EXPECT_FALSE(session.referencePlaying());
}

TEST(TakeSessionTest, ATakeRunsThroughListenCountInCaptureAndAnalysis)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    EXPECT_TRUE(t.withTake);
    EXPECT_EQ(t.countInBeats, 4u);
    const auto spb = static_cast<size_t>(std::llround(t.samplesPerBeat));
    EXPECT_EQ(spb, static_cast<size_t>(kSecondsPerBeat * kSampleRate));
    EXPECT_EQ(t.countInStart, t.listenEnd + spb);
    EXPECT_EQ(t.downbeat, t.countInStart + 4 * spb);
    EXPECT_EQ(t.captureStart, t.downbeat - spb);
    EXPECT_EQ(t.captureEnd, t.downbeat + 4 * spb + spb);

    static_cast<void>(run(session, t.countInStart - 1));
    EXPECT_EQ(session.state(), State::Listening);
    static_cast<void>(run(session, 1));
    EXPECT_EQ(session.state(), State::CountIn);
    static_cast<void>(run(session, t.captureStart - t.countInStart));
    EXPECT_EQ(session.state(), State::Capturing);
    static_cast<void>(run(session, t.captureEnd - t.captureStart));
    EXPECT_EQ(session.state(), State::Analysing);
    EXPECT_EQ(session.takeId(), 1u);
    EXPECT_EQ(session.captured().size(), t.captureEnd - t.captureStart);
    static_cast<void>(run(session, 100));
    EXPECT_EQ(session.state(), State::Analysing);
    session.finishAnalysis(1);
    static_cast<void>(run(session, 1));
    EXPECT_EQ(session.state(), State::Review);
}

TEST(TakeSessionTest, TheCountInClicksOnEveryBeatAndNumbersThem)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    const auto spb = static_cast<size_t>(std::llround(t.samplesPerBeat));
    const auto out = run(session, t.captureEnd);
    for (size_t beat = 0; beat < 4; ++beat)
    {
        const size_t at = t.countInStart + beat * spb;
        EXPECT_GT(peak(out, at, at + 200), 1e-3f) << "beat " << beat;
        EXPECT_LT(peak(out, at - 400, at - 200), 1e-4f) << "before beat " << beat;
    }

    TakeSession counter{kSampleRate};
    ASSERT_TRUE(counter.setPhrase(shortPattern()));
    ASSERT_TRUE(counter.startTake(makeSettings()));
    static_cast<void>(run(counter, t.countInStart + spb / 2));
    EXPECT_EQ(counter.countInBeat(), 1);
    static_cast<void>(run(counter, spb));
    EXPECT_EQ(counter.countInBeat(), 2);
}

TEST(TakeSessionTest, ClickKeepsGoingDuringCaptureOnlyWhenOn)
{
    const auto capturePeak = [](const bool clickOn)
    {
        TakeSession session{kSampleRate};
        EXPECT_TRUE(session.setPhrase(shortPattern()));
        EXPECT_TRUE(session.startTake(makeSettings(clickOn)));
        const auto& t = session.timeline();
        const auto out = run(session, t.captureEnd);
        return peak(out, t.downbeat, t.captureEnd);
    };
    EXPECT_GT(capturePeak(true), 1e-3f);
    EXPECT_LT(capturePeak(false), 1e-4f);
}

TEST(TakeSessionTest, TheRecordingHoldsExactlyTheInputOfTheCaptureWindow)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    std::vector<float> ramp(t.captureEnd + 1000);
    for (size_t i = 0; i < ramp.size(); ++i)
    {
        ramp[i] = static_cast<float>(i) * 1e-6f;
    }
    static_cast<void>(runWithInput(session, t.captureEnd + 500, ramp, 0));
    const auto captured = session.captured();
    ASSERT_EQ(captured.size(), t.captureEnd - t.captureStart);
    EXPECT_FLOAT_EQ(captured.front(), static_cast<float>(t.captureStart) * 1e-6f);
    EXPECT_FLOAT_EQ(captured.back(), static_cast<float>(t.captureEnd - 1) * 1e-6f);
    EXPECT_NEAR(session.setup().downbeatSeconds, static_cast<float>(t.downbeat - t.captureStart) / kSampleRate, 1e-6f);
    EXPECT_TRUE(session.setup().clickOn);
    EXPECT_FLOAT_EQ(session.setup().bpm, kBpm);
}

TEST(TakeSessionTest, ThePickupMovesTheStartOfTheCaptureEarlier)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern(2.f)));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    const auto spb = static_cast<size_t>(std::llround(t.samplesPerBeat));
    EXPECT_EQ(t.captureStart, std::max(t.countInStart, t.downbeat - 3 * spb));
    EXPECT_EQ(t.captureStart, t.countInStart + spb);
}

TEST(TakeSessionTest, StopEndsAListenAndDiscardsATakeInProgress)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    static_cast<void>(run(session, 2000));
    session.stop();
    EXPECT_EQ(session.state(), State::Idle);
    const auto fade = run(session, 1500);
    EXPECT_LT(peak(fade, 500, fade.size()), 1e-4f);

    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    static_cast<void>(run(session, t.captureStart + 1000));
    EXPECT_EQ(session.state(), State::Capturing);
    session.stop();
    EXPECT_EQ(session.state(), State::Idle);
    EXPECT_EQ(session.takeId(), 0u);
}

TEST(TakeSessionTest, ABusySessionRefusesNewPhrasesAndCommands)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    EXPECT_FALSE(session.setPhrase(shortPattern(1.f)));
    EXPECT_FALSE(session.listen(makeSettings()));
    EXPECT_FALSE(session.startTake(makeSettings()));
    EXPECT_FALSE(session.replayTake());
}

TEST(TakeSessionTest, ReplayPlaysTheRecordingBackAndReturnsToReview)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    const std::vector<float> tone(t.captureEnd, 0.25f);
    static_cast<void>(runWithInput(session, t.captureEnd + 10, tone, 0));
    session.finishAnalysis(1);
    static_cast<void>(run(session, 1));
    ASSERT_EQ(session.state(), State::Review);
    ASSERT_TRUE(session.replayTake());
    EXPECT_EQ(session.state(), State::Replaying);
    const auto out = run(session, 2000);
    EXPECT_FLOAT_EQ(out.front(), 0.25f);
    EXPECT_FLOAT_EQ(out.back(), 0.25f);
    static_cast<void>(run(session, session.captured().size()));
    EXPECT_EQ(session.state(), State::Review);
}

TEST(TakeSessionTest, MidiEventsAreStampedRelativeToTheStartOfTheCapture)
{
    TakeSession session{kSampleRate};
    ASSERT_TRUE(session.setPhrase(shortPattern()));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    session.recordMidi(60, 90, true, 0);
    static_cast<void>(run(session, t.captureStart + 1000));
    session.recordMidi(64, 100, true, 10);
    session.recordMidi(64, 0, false, 500);
    const auto events = session.midiEvents();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].sample, 1010u);
    EXPECT_EQ(events[0].note, 64);
    EXPECT_TRUE(events[0].on);
    EXPECT_EQ(events[1].sample, 1500u);
    EXPECT_FALSE(events[1].on);
}

TEST(TakeSessionTest, ARenderedPerformanceAtTheDownbeatComesBackAsTheCapture)
{
    TakeSession session{kSampleRate};
    const auto pattern = shortPattern(1.f);
    ASSERT_TRUE(session.setPhrase(pattern));
    ASSERT_TRUE(session.startTake(makeSettings()));
    const auto& t = session.timeline();
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, KsInstrument::Id::Guitar);
    std::vector<float> user(player.lengthSamples());
    for (size_t i = 0; i < user.size(); i += 16)
    {
        player.process(std::span<float>{user}.subspan(i, std::min<size_t>(16, user.size() - i)));
    }
    const size_t userStart = t.downbeat - player.downbeatSample();
    static_cast<void>(runWithInput(session, t.captureEnd + 10, user, userStart));
    const auto captured = session.captured();
    const size_t firstSound = userStart - t.captureStart;
    EXPECT_LT(peak({captured.begin(), captured.end()}, 0, firstSound - 100), 1e-6f);
    EXPECT_GT(peak({captured.begin(), captured.end()}, firstSound, firstSound + 200), 1e-3f);
}
