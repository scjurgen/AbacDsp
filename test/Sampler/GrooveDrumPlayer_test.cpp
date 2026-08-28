#include <gtest/gtest.h>
#include <set>
#include <sstream>
#include <vector>

#include "Sampler/GrooveDrumPlayer.h"

namespace AbacDsp::test
{

namespace
{
constexpr float kSampleRate = 1000.f; // fade in ms maps 1:1 to frames

[[nodiscard]] std::vector<float> makeConstLoop(const size_t frames, const float l, const float r)
{
    std::vector<float> loop(frames * 2, 0.f);
    for (size_t f = 0; f < frames; ++f)
    {
        loop[f * 2] = l;
        loop[f * 2 + 1] = r;
    }
    return loop;
}
}

TEST(GrooveDrumPlayerTest, TriggerAtTickZeroFiresOnFirstSample)
{
    const auto loop = makeConstLoop(20, 1.f, 1.f);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 20}});

    GrooveProgram program{{{0, 0, 1.f}}, 960, 960};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    EXPECT_EQ(player.activeVoiceCount(), 0u);
    static_cast<void>(player.advanceSample(100));
    EXPECT_EQ(player.activeVoiceCount(), 1u);
}

TEST(GrooveDrumPlayerTest, TriggerFiresAtExactTickOffsetNotBeforeOrAfter)
{
    const auto loop = makeConstLoop(20, 1.f, 1.f);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 20}});

    // 9.6 ticks/sample; tick 485 (off an exact multiple, clear of double
    // rounding at the boundary) first crosses at k=51: 50*9.6=480<485<489.6.
    GrooveProgram program{{{485, 0, 1.f}}, 960, 960};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    for (int i = 0; i < 50; ++i)
    {
        static_cast<void>(player.advanceSample(100));
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "must not fire before its exact tick";

    static_cast<void>(player.advanceSample(100));
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "must fire on the sample that crosses its tick";
}

TEST(GrooveDrumPlayerTest, LoopWrapsAndRetriggersOnItsOwnPeriodIndependentOfCaller)
{
    const auto loop = makeConstLoop(5, 1.f, 1.f); // short slice: fully finishes well before a wrap
    SliceLibrary library(5);
    library.extractTrack(loop, std::vector<Slice>{{0, 5}});

    // ticksPerQuarterNote == samplesPerBeat -> exactly 1 tick/sample; loop wraps every 50 samples.
    GrooveProgram program{{{0, 0, 1.f}}, 50, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    size_t risingEdges = 0;
    bool wasActive = false;
    for (int i = 0; i < 205; ++i)
    {
        static_cast<void>(player.advanceSample(1));
        const bool active = player.activeVoiceCount() > 0;
        if (active && !wasActive)
        {
            ++risingEdges;
        }
        wasActive = active;
    }
    EXPECT_EQ(risingEdges, 5u); // fires at sample 1, 51, 101, 151, 201
}

TEST(GrooveDrumPlayerTest, RoundRobinVariesSliceAcrossRepeatedTriggers)
{
    // Slices differ only by length; identify the picked slice by how many
    // samples the voice stays active.
    constexpr size_t kSliceCount = 8;
    constexpr size_t kLengthStep = 5;

    std::vector<float> longLoop;
    std::vector<Slice> slices;
    size_t offset = 0;
    for (size_t i = 0; i < kSliceCount; ++i)
    {
        const size_t length = (i + 1) * kLengthStep;
        const auto piece = makeConstLoop(length, 1.f, 1.f);
        longLoop.insert(longLoop.end(), piece.begin(), piece.end());
        slices.push_back({offset, length});
        offset += length;
    }
    SliceLibrary library(offset);
    library.extractTrack(longLoop, slices);

    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    std::set<size_t> observedLengths;
    for (int trial = 0; trial < 60; ++trial)
    {
        player.resetPosition();
        size_t framesActive = 0;
        bool wasActive = false;
        for (int i = 0; i < static_cast<int>(kSliceCount * kLengthStep) + 5; ++i)
        {
            static_cast<void>(player.advanceSample(1));
            const bool active = player.activeVoiceCount() > 0;
            if (active)
            {
                ++framesActive;
                wasActive = true;
            }
            else if (wasActive)
            {
                // The call that just ran still rendered the voice's final frame
                // before deactivating it internally, so that frame counts too.
                ++framesActive;
                break;
            }
        }
        observedLengths.insert(framesActive);
        EXPECT_EQ(framesActive % kLengthStep, 0u);
    }
    EXPECT_GT(observedLengths.size(), 1u) << "expected slice selection to vary across repeated triggers";
}

TEST(GrooveDrumPlayerTest, GainIsAppliedLinearlyWithNoPeakNormalization)
{
    const auto loop = makeConstLoop(64, 9.f, 9.f); // recorded peak = 9
    SliceLibrary library(64);
    library.extractTrack(loop, std::vector<Slice>{{0, 64}});

    GrooveProgram program{{{0, 0, 0.5f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f); // 1-frame fade at 1000 Hz
    player.setLibrary(&library);
    player.setGroove(&program);

    std::array<float, 2> out{};
    for (int i = 0; i < 10; ++i)
    {
        out = player.advanceSample(1);
    }
    // No 1/peak normalization: raw 9 * event gain 0.5, not (9/9) * 0.5.
    EXPECT_NEAR(out[0], 4.5f, 1e-3f);
    EXPECT_NEAR(out[1], 4.5f, 1e-3f);
}

TEST(GrooveDrumPlayerTest, ResetPositionRetriggersImmediately)
{
    const auto loop = makeConstLoop(10, 1.f, 1.f);
    SliceLibrary library(10);
    library.extractTrack(loop, std::vector<Slice>{{0, 10}});

    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    for (int i = 0; i < 10; ++i)
    {
        static_cast<void>(player.advanceSample(1));
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "the 10-frame slice should have finished";

    player.resetPosition();
    static_cast<void>(player.advanceSample(1));
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "resetPosition() should re-arm the tick-0 event";
}

TEST(GrooveDrumPlayerTest, LogsOnlyOnceWhenTheLoopRepeats)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setLibrary(&library);
    player.setGroove(&program);

    std::ostringstream captured;
    std::streambuf* originalCoutBuffer = std::cout.rdbuf(captured.rdbuf());
    for (int i = 0; i < 999; ++i)
    {
        static_cast<void>(player.advanceSample(1)); // one tick per call; loop is 1000 ticks
    }
    const std::string beforeWrap = captured.str();
    static_cast<void>(player.advanceSample(1)); // crosses the loop boundary
    std::cout.rdbuf(originalCoutBuffer);

    const std::string afterWrap = captured.str();
    EXPECT_EQ(beforeWrap.find("loop repeat"), std::string::npos);
    EXPECT_NE(afterWrap.find("loop repeat"), std::string::npos);
}

TEST(GrooveDrumPlayerTest, MissingLibraryOrProgramProducesSilenceAndNoCrash)
{
    GrooveDrumPlayer player(kSampleRate);
    for (int i = 0; i < 10; ++i)
    {
        const auto out = player.advanceSample(100);
        EXPECT_FLOAT_EQ(out[0], 0.f);
        EXPECT_FLOAT_EQ(out[1], 0.f);
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

TEST(GrooveDrumPlayerTest, SyncToPpqWithNoGrooveIsNoOp)
{
    GrooveDrumPlayer player(kSampleRate);
    EXPECT_FALSE(player.syncToPpq(1.0));
}

TEST(GrooveDrumPlayerTest, SyncToPpqSubTickDriftIsNoOp)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    GrooveProgram program{{{0, 0, 1.f}, {500, 0, 1.f}}, 960, 480};

    GrooveDrumPlayer player(kSampleRate);
    player.setLibrary(&library);
    player.setGroove(&program);
    for (int i = 0; i < 100; ++i) // 1 tick/sample: now at tick 100
    {
        static_cast<void>(player.advanceSample(480));
    }
    EXPECT_FALSE(player.syncToPpq(100.0 / 480.0)) << "same position (in quarter notes) must not resync";
}

TEST(GrooveDrumPlayerTest, SyncToPpqRealJumpSkipsAlreadyPassedTriggers)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    GrooveProgram program{{{0, 0, 1.f}, {200, 0, 1.f}, {400, 0, 1.f}}, 480, 480};

    GrooveDrumPlayer player(kSampleRate);
    player.setLibrary(&library);
    player.setGroove(&program);

    EXPECT_TRUE(player.syncToPpq(300.0 / 480.0)); // jump past ticks 0 and 200

    size_t risingEdges = 0;
    bool wasActive = false;
    for (int i = 0; i < 150; ++i) // 300 -> 450 ticks at 1 tick/sample
    {
        static_cast<void>(player.advanceSample(480));
        const bool active = player.activeVoiceCount() > 0;
        if (active && !wasActive)
        {
            ++risingEdges;
        }
        wasActive = active;
    }
    EXPECT_EQ(risingEdges, 1u) << "only the tick-400 trigger should fire, not the already-passed 0/200";
}

TEST(GrooveDrumPlayerTest, SyncToPpqWrapsPositionAcrossLoopBoundary)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    GrooveProgram program{{{50, 0, 1.f}}, 100, 100};

    GrooveDrumPlayer player(kSampleRate);
    player.setLibrary(&library);
    player.setGroove(&program);

    // 2.5 quarter notes * 100 ticks/quarter = 250 ticks -> wraps to tick 50.
    EXPECT_TRUE(player.syncToPpq(2.5));
    for (int i = 0; i < 40; ++i)
    {
        static_cast<void>(player.advanceSample(100));
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "landing exactly on tick 50 must not refire it";

    size_t risingEdges = 0;
    bool wasActive = false;
    for (int i = 0; i < 110; ++i) // wraps once more, past tick 50 again
    {
        static_cast<void>(player.advanceSample(100));
        const bool active = player.activeVoiceCount() > 0;
        if (active && !wasActive)
        {
            ++risingEdges;
        }
        wasActive = active;
    }
    EXPECT_EQ(risingEdges, 1u) << "expected exactly one refire on the next loop pass";
}

TEST(GrooveDrumPlayerTest, SetTrackGainMutesOneTrackWithoutAffectingOthers)
{
    const auto loopA = makeConstLoop(20, 2.f, 2.f);
    const auto loopB = makeConstLoop(20, 3.f, 3.f);
    SliceLibrary library(40);
    library.extractTrack(loopA, std::vector<Slice>{{0, 20}});
    library.extractTrack(loopB, std::vector<Slice>{{0, 20}});

    GrooveProgram program{{{0, 0, 1.f}, {0, 1, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);
    player.setTrackGain(0, 0.f);

    std::array<float, 2> out{};
    for (int i = 0; i < 10; ++i)
    {
        out = player.advanceSample(1);
    }
    EXPECT_NEAR(out[0], 3.f, 1e-3f) << "muted track 0 must not contribute";
    EXPECT_NEAR(out[1], 3.f, 1e-3f);
}

TEST(GrooveDrumPlayerTest, PerTrackOutputReportsEachTracksOwnPostGainContribution)
{
    const auto loopA = makeConstLoop(20, 2.f, -1.f);
    const auto loopB = makeConstLoop(20, 3.f, 1.f);
    SliceLibrary library(40);
    library.extractTrack(loopA, std::vector<Slice>{{0, 20}});
    library.extractTrack(loopB, std::vector<Slice>{{0, 20}});

    GrooveProgram program{{{0, 0, 1.f}, {0, 1, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);
    player.setTrackGain(1, 0.5f);

    std::array<std::array<float, GrooveDrumPlayer::kChannels>, GrooveDrumPlayer::kMaxTracks> perTrack{};
    std::array<float, 2> mix{};
    for (int i = 0; i < 10; ++i)
    {
        mix = player.advanceSample(1, &perTrack);
    }
    EXPECT_NEAR(perTrack[0][0], 2.f, 1e-3f);
    EXPECT_NEAR(perTrack[0][1], -1.f, 1e-3f);
    EXPECT_NEAR(perTrack[1][0], 1.5f, 1e-3f) << "track 1's own 0.5 gain must be reflected here too";
    EXPECT_NEAR(perTrack[1][1], 0.5f, 1e-3f);
    EXPECT_NEAR(mix[0], 3.5f, 1e-3f);
    EXPECT_NEAR(mix[1], -0.5f, 1e-3f);
    for (size_t t = 2; t < GrooveDrumPlayer::kMaxTracks; ++t)
    {
        EXPECT_FLOAT_EQ(perTrack[t][0], 0.f);
        EXPECT_FLOAT_EQ(perTrack[t][1], 0.f);
    }
}

TEST(GrooveDrumPlayerTest, RenderBurstProducesRequestedFrameCount)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    const auto burst = GrooveDrumPlayer::renderBurst(kSampleRate, 120.f, library, program, 37);
    EXPECT_EQ(burst.audio.size(), 37u * 2);
}

TEST(GrooveDrumPlayerTest, PrimedPlayerContinuesEquivalentToDirectRender)
{
    // A single slice per track: round-robin picks it deterministically (sliceCount
    // == 1), so this isolates tick-continuity from the two players' independent
    // (and deliberately not transferred) RNG streams.
    const auto loop = makeConstLoop(6, 3.f, 3.f);
    SliceLibrary library(6);
    library.extractTrack(loop, std::vector<Slice>{{0, 6}});
    GrooveProgram program{{{5, 0, 1.f}, {20, 0, 1.f}, {45, 0, 1.f}}, 60, 60};

    constexpr float kBurstBpm = 1000.f; // samplesPerBeat == ticksPerQuarterNote == 60: 1 tick/sample
    constexpr size_t kBurstFrames = 30;
    constexpr size_t kContinueFrames = 25;
    const auto samplesPerBeat = static_cast<size_t>(kSampleRate * 60.f / kBurstBpm);

    const auto burst = GrooveDrumPlayer::renderBurst(kSampleRate, kBurstBpm, library, program, kBurstFrames);

    GrooveDrumPlayer primed(kSampleRate);
    primed.setLibrary(&library);
    primed.setGroove(&program);
    primed.primeTickState(burst.tickPos, burst.nextTriggerIndex);

    GrooveDrumPlayer reference(kSampleRate);
    reference.setLibrary(&library);
    reference.setGroove(&program);
    for (size_t i = 0; i < kBurstFrames; ++i)
    {
        static_cast<void>(reference.advanceSample(samplesPerBeat));
    }

    for (size_t i = 0; i < kContinueFrames; ++i)
    {
        const auto primedOut = primed.advanceSample(samplesPerBeat);
        const auto referenceOut = reference.advanceSample(samplesPerBeat);
        EXPECT_FLOAT_EQ(primedOut[0], referenceOut[0]) << "frame " << i;
        EXPECT_FLOAT_EQ(primedOut[1], referenceOut[1]) << "frame " << i;
    }
}

TEST(GrooveDrumPlayerTest, AdvanceToPositionFiresTriggerAtCorrectAbsoluteBeats)
{
    const auto loop = makeConstLoop(10, 1.f, 1.f);
    SliceLibrary library(10);
    library.extractTrack(loop, std::vector<Slice>{{0, 10}});
    // 100 ticks/quarter note; trigger at tick 250 -> 2.5 beats.
    GrooveProgram program{{{250, 0, 1.f}}, 1000, 100};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    static_cast<void>(player.advanceToPosition(2.4));
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "must not fire before its exact beat position";

    static_cast<void>(player.advanceToPosition(2.6));
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "must fire once the position crosses its tick";
}

TEST(GrooveDrumPlayerTest, AdvanceToPositionWrapsAndRefiresOnItsOwnPeriod)
{
    const auto loop = makeConstLoop(5, 1.f, 1.f); // short slice: fully finishes well before a wrap
    SliceLibrary library(5);
    library.extractTrack(loop, std::vector<Slice>{{0, 5}});
    // loop is 1000 ticks / 100 ticks-per-quarter = 10 beats.
    GrooveProgram program{{{0, 0, 1.f}}, 1000, 100};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    size_t risingEdges = 0;
    bool wasActive = false;
    for (int step = 1; step <= 205; ++step) // 0.1 beats/step -> sweeps 3.5 loop periods
    {
        static_cast<void>(player.advanceToPosition(static_cast<double>(step) * 0.1));
        const bool active = player.activeVoiceCount() > 0;
        if (active && !wasActive)
        {
            ++risingEdges;
        }
        wasActive = active;
    }
    EXPECT_EQ(risingEdges, 3u) << "expected three fires as the swept position crosses beats 0, 10, and 20";
}

TEST(GrooveDrumPlayerTest, AdvanceToPositionBackwardJumpFiresAtMostOnce)
{
    // 2-frame slice: the first firing's voice has finished by the second call's
    // own render step, so activeVoiceCount() afterward reflects only that call.
    const auto loop = makeConstLoop(2, 1.f, 1.f);
    SliceLibrary library(2);
    library.extractTrack(loop, std::vector<Slice>{{0, 2}});
    GrooveProgram program{{{0, 0, 1.f}}, 1000, 100}; // 10 beats/loop; single trigger at beat 0.

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    static_cast<void>(player.advanceToPosition(9.0)); // past the trigger, near loop end
    static_cast<void>(player.advanceToPosition(1.0)); // discontinuous jump backward - a clock reset
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "the tick-0 trigger should refire exactly once, not twice";
}

TEST(GrooveDrumPlayerTest, ResyncToPositionPicksUpTriggerIndexWithoutFiring)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    // 10 beats/loop; triggers at beat 0 and beat 5.
    GrooveProgram program{{{0, 0, 1.f}, {500, 0, 1.f}}, 1000, 100};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    player.resyncToPosition(6.0); // past the beat-5 trigger
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "resyncToPosition() must not fire anything itself";

    // Advancing on toward the loop end must not refire the already-passed beat-5
    // trigger, only wrap around to beat 0.
    static_cast<void>(player.advanceToPosition(9.9));
    EXPECT_EQ(player.activeVoiceCount(), 0u);
    static_cast<void>(player.advanceToPosition(10.1));
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "expected only the wrapped beat-0 trigger to fire";
}

TEST(GrooveDrumPlayerTest, ResyncToPositionWithNoProgramIsHarmless)
{
    GrooveDrumPlayer player(kSampleRate);
    player.resyncToPosition(3.0);
    static_cast<void>(player.advanceToPosition(3.1));
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

}
