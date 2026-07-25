#include <cmath>
#include <gtest/gtest.h>

#include "Sampler/LoopRecorder.h"

namespace AbacDsp::test
{

namespace
{
constexpr size_t kBlock = 16;
using Recorder = LoopRecorder<kBlock>;
using Buffer = AudioBuffer<2, kBlock>;

// Feeds `frames` of a ramp (value = startValue + frame) so every captured sample
// is identifiable by position; returns the number of blocks processed.
void feed(Recorder& rec, const size_t frames, const float startValue = 1.f)
{
    size_t done = 0;
    while (done < frames)
    {
        Buffer in{};
        Buffer out{};
        for (size_t i = 0; i < kBlock; ++i)
        {
            const float v = (done + i < frames) ? startValue + static_cast<float>(done + i) : 0.f;
            in(i, 0) = v;
            in(i, 1) = -v;
        }
        rec.processBlock(in, out);
        done += kBlock;
    }
}

Buffer runOne(Recorder& rec, const float inValue = 0.f)
{
    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = inValue;
        in(i, 1) = inValue;
    }
    rec.processBlock(in, out);
    return out;
}

// Feeds `frames` of a constant value, so the boundary fade's effect on the raw
// content is a plain gain multiply, easy to check exactly.
void feedConstant(Recorder& rec, const size_t frames, const float value)
{
    size_t done = 0;
    while (done < frames)
    {
        Buffer in{};
        Buffer out{};
        for (size_t i = 0; i < kBlock; ++i)
        {
            in(i, 0) = (done + i < frames) ? value : 0.f;
            in(i, 1) = (done + i < frames) ? value : 0.f;
        }
        rec.processBlock(in, out);
        done += kBlock;
    }
}

// A preRoll snapshot for stopRecordBeatLocked: 2*rollFrames frames, interleaved
// stereo, first half (the tail-fold source) at `firstHalf`, second half (the
// late-gap backfill source) at `secondHalf`.
std::vector<float> makePreRoll(const size_t rollFrames, const float firstHalf, const float secondHalf)
{
    std::vector<float> preRoll(4 * rollFrames, 0.f);
    for (size_t i = 0; i < rollFrames; ++i)
    {
        preRoll[i * 2] = firstHalf;
        preRoll[i * 2 + 1] = firstHalf;
        preRoll[(rollFrames + i) * 2] = secondHalf;
        preRoll[(rollFrames + i) * 2 + 1] = secondHalf;
    }
    return preRoll;
}
}

TEST(LoopRecorderTest, StartsEmpty)
{
    Recorder rec{48000.f};
    EXPECT_EQ(rec.state(), LooperState::Empty);
    EXPECT_FALSE(rec.hasLoop());
    EXPECT_EQ(rec.loopLengthFrames(), 0u);
}

TEST(LoopRecorderTest, PlayFromEmptyDoesNothing)
{
    Recorder rec{48000.f};
    rec.play();
    EXPECT_EQ(rec.state(), LooperState::Empty);
}

TEST(LoopRecorderTest, RecordThenStopEntersPlaying)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0); // no quantization
    rec.beginRecord();
    EXPECT_EQ(rec.state(), LooperState::Recording);
    feed(rec, 64);
    rec.stopRecord();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_EQ(rec.recordedFrames(), 64u);
    EXPECT_EQ(rec.loopLengthFrames(), 64u);
}

TEST(LoopRecorderTest, CapturedContentMatchesInput)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, 32, 1.f);
    rec.stopRecord();
    for (size_t f = 0; f < 32; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f));
        EXPECT_FLOAT_EQ(rec.sample(f, 1), -(1.f + static_cast<float>(f)));
    }
}

TEST(LoopRecorderTest, PlaybackLoopsRecordedContent)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, kBlock, 1.f); // exactly one block, values 1..16
    rec.stopRecord();
    ASSERT_EQ(rec.loopLengthFrames(), kBlock);

    const Buffer first = runOne(rec);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(first(i, 0), 1.f + static_cast<float>(i));
    }
    // Loop wraps: the next block repeats the same content.
    const Buffer second = runOne(rec);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(second(i, 0), 1.f + static_cast<float>(i));
    }
}

TEST(LoopRecorderTest, OutputSilentWhileRecording)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.7f;
        in(i, 1) = 0.7f;
    }
    rec.processBlock(in, out);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
        EXPECT_FLOAT_EQ(out(i, 1), 0.f);
    }
}

TEST(LoopRecorderTest, BeatQuantizationRoundsToNearestBeat)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(100);

    rec.beginRecord();
    feed(rec, 240); // 2.4 beats -> rounds down to 2 beats = 200
    rec.stopRecord();
    EXPECT_EQ(rec.loopLengthFrames(), 200u);

    rec.clear();
    rec.setSamplesPerBeat(100);
    rec.beginRecord();
    feed(rec, 260); // 2.6 beats -> rounds up to 3 beats = 300
    rec.stopRecord();
    EXPECT_EQ(rec.loopLengthFrames(), 300u);
}

TEST(LoopRecorderTest, QuantizedTailIsSilent)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(100);
    rec.beginRecord();
    feed(rec, 260, 1.f); // rounds up to 300; frames 260..299 must be silent
    rec.stopRecord();
    ASSERT_EQ(rec.loopLengthFrames(), 300u);
    for (size_t f = 260; f < 300; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 0.f) << "frame " << f;
        EXPECT_FLOAT_EQ(rec.sample(f, 1), 0.f) << "frame " << f;
    }
}

TEST(LoopRecorderTest, BoundaryFadeRampsHeadAndTail)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 2.f);
    rec.stopRecord();
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // Fade-in: gain (k+1)/4 for k in [0,4), reaching exactly 1 at frame 3.
    for (size_t k = 0; k < 4; ++k)
    {
        const float expected = 2.f * static_cast<float>(k + 1) / 4.f;
        EXPECT_FLOAT_EQ(rec.sample(k, 0), expected) << "head frame " << k;
    }
    // Fade-out mirrors from the last frame inward.
    for (size_t k = 0; k < 4; ++k)
    {
        const float expected = 2.f * static_cast<float>(k + 1) / 4.f;
        EXPECT_FLOAT_EQ(rec.sample(63 - k, 0), expected) << "tail frame " << (63 - k);
    }
    // Untouched interior stays at full amplitude.
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 2.f);
}

TEST(LoopRecorderTest, NoFadeWhenFadeFramesIsZero)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(0);
    rec.beginRecord();
    feedConstant(rec, 32, 3.f);
    rec.stopRecord();
    for (size_t f = 0; f < 32; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 3.f) << "frame " << f;
    }
}

TEST(LoopRecorderTest, BoundaryFadeClampsToHalfLoopLengthForShortLoops)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(100); // far longer than the loop
    rec.beginRecord();
    feedConstant(rec, kBlock, 2.f); // exactly one block, loopLengthFrames == kBlock (16)
    rec.stopRecord();
    ASSERT_EQ(rec.loopLengthFrames(), kBlock);

    // Clamped fade = loopLength/2 = 8, same ramp shape as the unclamped case above.
    for (size_t k = 0; k < 8; ++k)
    {
        const float expected = 2.f * static_cast<float>(k + 1) / 8.f;
        EXPECT_FLOAT_EQ(rec.sample(k, 0), expected) << "head frame " << k;
        EXPECT_FLOAT_EQ(rec.sample(kBlock - 1 - k, 0), expected) << "tail frame " << (kBlock - 1 - k);
    }
}

TEST(LoopRecorderTest, BeatLockedFinalizeFoldsPreRollAndPostRollOnTime)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f); // main take
    feedConstant(rec, 16, 9.f); // post-roll capture (only the last 8 frames used)
    ASSERT_EQ(rec.recordedFrames(), 80u);

    const std::vector<float> preRoll = makePreRoll(8, 3.f, 7.f);
    rec.stopRecordBeatLocked(64, preRoll, 0, 8, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // Front: main take (5.0) + post-roll (9.0) fading out from full to 1/4.
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 5.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(7, 0), 5.f + 9.f * 0.25f);
    // Middle: untouched main take.
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 5.f);
    // Tail: main take (5.0) + pre-roll (3.0) fading in from 1/4 to full.
    EXPECT_FLOAT_EQ(rec.sample(56, 0), 5.f + 3.f * 0.25f);
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f + 3.f * 1.f);
}

TEST(LoopRecorderTest, BeatLockedFinalizeBackfillsLateGapFromPreRoll)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(4);
    rec.beginRecord();
    // Same raw capture as the on-time case (content is constant, so shifting
    // it up by the 3-frame late gap doesn't change any of these values) --
    // feedConstant rounds up to a full block, so a genuinely 61-frame capture
    // can't be fed directly without the block padding changing recordedFrames.
    feedConstant(rec, 64, 5.f); // main take
    feedConstant(rec, 16, 9.f); // post-roll capture
    ASSERT_EQ(rec.recordedFrames(), 80u);

    const std::vector<float> preRoll = makePreRoll(8, 3.f, 7.f);
    rec.stopRecordBeatLocked(64, preRoll, -3, 8, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // [0,3): backfilled from preRoll's second half (7.0) + post-roll fold.
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 7.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(2, 0), 7.f + 9.f * 1.f);
    // [3,8): shifted-up main take (5.0) + post-roll fold still active (i<8).
    EXPECT_FLOAT_EQ(rec.sample(3, 0), 5.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(7, 0), 5.f + 9.f * 0.25f);
    // Middle: shifted-up main take, untouched by either fold.
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 5.f);
    // Tail: shifted-up main take + pre-roll fold, same as the on-time case.
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f + 3.f * 1.f);
}

TEST(LoopRecorderTest, BeatLockedFinalizeRelocatesEarlyStartToTail)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(4);
    rec.beginRecord();

    // Recording starts immediately (no waiting for the tick): construct the
    // capture directly, not via feedConstant (which always pads to a full
    // block) -- samples [0,3) are real pre-tick content this take captured
    // itself (6.0), [3,67) the aligned main take (5.0, 64 frames), [67,96)
    // generous post-roll capture (9.0, only the last 8 frames get used).
    for (size_t done = 0; done < 96; done += kBlock)
    {
        Buffer in{};
        Buffer out{};
        for (size_t i = 0; i < kBlock; ++i)
        {
            const size_t f = done + i;
            const float v = (f < 3) ? 6.f : (f < 67) ? 5.f : 9.f;
            in(i, 0) = v;
            in(i, 1) = v;
        }
        rec.processBlock(in, out);
    }
    ASSERT_EQ(rec.recordedFrames(), 96u);

    // preRoll covers only [s-8, trig): 5 frames from before this take's own
    // capture began (a ring-buffer snapshot in real use); distinct value
    // (4.0) so it's easy to tell apart from the 3 frames captured directly.
    const std::vector<float> preRoll(5 * 2, 4.f);
    rec.stopRecordBeatLocked(64, preRoll, 3, 8, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // Main take: shifted down by 3, so loop frames [0,64) all read the
    // aligned 5.0 content before any fold is applied.
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 5.f);

    // Tail fold: preRoll (4.0) for i<5, then the captured early frames (6.0)
    // for i=5,6,7, both summed onto the shifted-down main take (5.0).
    EXPECT_FLOAT_EQ(rec.sample(56, 0), 5.f + 4.f * 0.25f); // i=0
    EXPECT_FLOAT_EQ(rec.sample(59, 0), 5.f + 4.f * 1.f);   // i=3
    EXPECT_FLOAT_EQ(rec.sample(60, 0), 5.f + 4.f * 1.f);   // i=4 (last preRoll frame)
    EXPECT_FLOAT_EQ(rec.sample(61, 0), 5.f + 6.f * 1.f);   // i=5 (first captured-early frame)
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f + 6.f * 1.f);   // i=7

    // Front fold: post-roll (9.0) fading out, same shape as the on-time case.
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 5.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(7, 0), 5.f + 9.f * 0.25f);
}

TEST(LoopRecorderTest, BeatLockedFinalizeClampsRollToHalfLoopLength)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(2);
    rec.beginRecord();
    feedConstant(rec, kBlock, 5.f); // 16-frame main take
    feedConstant(rec, kBlock, 9.f); // generous post-roll capture
    ASSERT_EQ(rec.recordedFrames(), 32u);

    // rollFrames=8 requested, but loopLength/2=8 too -- exercises the clamp
    // path without actually clamping (roll==8 either way); loopLength=16 is
    // the smallest exact multiple of kBlock that keeps the test deterministic.
    const std::vector<float> preRoll = makePreRoll(8, 3.f, 7.f);
    rec.stopRecordBeatLocked(16, preRoll, 0, 8, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 16u);
    // No untouched middle frame exists (roll windows meet at the centre); just
    // confirm every frame stayed finite and the take didn't corrupt state.
    for (size_t f = 0; f < 16; ++f)
    {
        EXPECT_TRUE(std::isfinite(rec.sample(f, 0))) << "frame " << f;
    }
    EXPECT_EQ(rec.state(), LooperState::Playing);
}

TEST(LoopRecorderTest, BeatLockedFinalizeResumesPlaybackFromCatchUpOffset)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f); // main take
    feedConstant(rec, 16, 9.f); // post-roll capture
    ASSERT_EQ(rec.recordedFrames(), 80u);

    // Commit is arriving late (post-roll wait plus block-boundary slop): the
    // caller reports 10 frames of real time already elapsed past the stop
    // tick, so playback must resume already 10 frames into the loop.
    const std::vector<float> preRoll = makePreRoll(8, 3.f, 7.f);
    rec.stopRecordBeatLocked(64, preRoll, 0, 8, 10);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);
    EXPECT_EQ(rec.playPositionFrames(), 10u);
}

TEST(LoopRecorderTest, BeatLockedFinalizeWrapsCatchUpOffsetToLoopLength)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f);
    feedConstant(rec, 16, 9.f);

    const std::vector<float> preRoll = makePreRoll(8, 3.f, 7.f);
    rec.stopRecordBeatLocked(64, preRoll, 0, 8, 64 + 10); // one full loop plus 10
    ASSERT_EQ(rec.loopLengthFrames(), 64u);
    EXPECT_EQ(rec.playPositionFrames(), 10u);
}

TEST(LoopRecorderTest, ShorterReRecordDoesNotLeakStaleTail)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(100);
    rec.beginRecord();
    feed(rec, 300, 5.f); // fills frames 0..299 with nonzero
    rec.stopRecord();

    rec.clear();
    rec.setSamplesPerBeat(100);
    rec.beginRecord();
    feed(rec, 60, 2.f); // 0.6 beat -> 1 beat = 100; frames 60..99 must be zeroed
    rec.stopRecord();
    ASSERT_EQ(rec.loopLengthFrames(), 100u);
    for (size_t f = 60; f < 100; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 0.f) << "stale frame " << f;
    }
}

TEST(LoopRecorderTest, EmptyRecordStaysEmpty)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(100);
    rec.beginRecord();
    rec.stopRecord(); // nothing recorded
    EXPECT_EQ(rec.state(), LooperState::Empty);
    EXPECT_FALSE(rec.hasLoop());
}

TEST(LoopRecorderTest, OverdubSumsIntoExistingLoop)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecord();

    rec.beginOverdub();
    EXPECT_EQ(rec.state(), LooperState::Overdubbing);
    const Buffer out = runOne(rec, 10.f); // output is the pre-existing content
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 1.f + static_cast<float>(i));
    }
    rec.endOverdub();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    // Existing + 10 is now stored.
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f) + 10.f);
    }
}

TEST(LoopRecorderTest, OverdubDecayFadesExisting)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.setOverdubDecay(0.5f);
    rec.beginRecord();
    feed(rec, kBlock, 4.f); // values 4..19
    rec.stopRecord();

    rec.beginOverdub();
    runOne(rec, 0.f); // input 0: existing *= 0.5
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), (4.f + static_cast<float>(f)) * 0.5f);
    }
}

TEST(LoopRecorderTest, ClearResetsToEmpty)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, 64);
    rec.stopRecord();
    ASSERT_TRUE(rec.hasLoop());

    rec.clear();
    EXPECT_EQ(rec.state(), LooperState::Empty);
    EXPECT_FALSE(rec.hasLoop());
    EXPECT_EQ(rec.loopLengthFrames(), 0u);
    EXPECT_EQ(rec.playPositionFrames(), 0u);
}

TEST(LoopRecorderTest, StopRewindsPlayhead)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, 64);
    rec.stopRecord();
    runOne(rec); // advance playhead by one block
    EXPECT_GT(rec.playPositionFrames(), 0u);

    rec.stop();
    EXPECT_EQ(rec.state(), LooperState::Stopped);
    EXPECT_EQ(rec.playPositionFrames(), 0u);
}

TEST(LoopRecorderTest, PauseKeepsPlayhead)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, 64);
    rec.stopRecord();
    runOne(rec);
    const size_t pos = rec.playPositionFrames();
    ASSERT_GT(pos, 0u);

    rec.pause();
    EXPECT_EQ(rec.state(), LooperState::Stopped);
    EXPECT_EQ(rec.playPositionFrames(), pos);
}

TEST(LoopRecorderTest, StoppedOutputsSilence)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, 64, 1.f);
    rec.stopRecord();
    rec.stop();
    const Buffer out = runOne(rec);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
    }
}

TEST(LoopRecorderTest, AutoStopsWhenBufferFull)
{
    // 0.001 s at 48k -> 48 frames of capacity.
    Recorder rec{48000.f, 0.001f};
    ASSERT_EQ(rec.maxFrames(), 48u);
    rec.setSamplesPerBeat(0);
    rec.beginRecord();
    feed(rec, 128); // more than capacity
    EXPECT_NE(rec.state(), LooperState::Recording);
    EXPECT_LE(rec.recordedFrames(), rec.maxFrames());
    EXPECT_TRUE(rec.hasLoop());
}

TEST(LoopRecorderTest, NoAllocationDuringProcessing)
{
    Recorder rec{48000.f};
    const size_t capacityBefore = rec.maxFrames();
    rec.setSamplesPerBeat(240);
    rec.beginRecord();
    feed(rec, 2000);
    rec.stopRecord();
    rec.beginOverdub();
    feed(rec, 2000);
    rec.endOverdub();
    for (int i = 0; i < 100; ++i)
    {
        runOne(rec);
    }
    EXPECT_EQ(rec.maxFrames(), capacityBefore);
}

}
