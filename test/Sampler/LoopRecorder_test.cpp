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

// A flat-valued preRoll snapshot for stopRecordBarLocked's late-start gap
// backfill: exactly `frames` interleaved stereo frames, all `value`.
std::vector<float> makeFlatPreRoll(const size_t frames, const float value)
{
    return std::vector<float>(frames * 2, value);
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
    rec.beginRecord();
    EXPECT_EQ(rec.state(), LooperState::Recording);
    feed(rec, 64);
    rec.stopRecordFree();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_EQ(rec.recordedFrames(), 64u);
    EXPECT_EQ(rec.loopLengthFrames(), 64u);
}

TEST(LoopRecorderTest, CapturedContentMatchesInput)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, 32, 1.f);
    rec.stopRecordFree();
    for (size_t f = 0; f < 32; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f));
        EXPECT_FLOAT_EQ(rec.sample(f, 1), -(1.f + static_cast<float>(f)));
    }
}

TEST(LoopRecorderTest, PlaybackLoopsRecordedContent)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f); // exactly one block, values 1..16
    rec.stopRecordFree();
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

TEST(LoopRecorderTest, FreeRecordStopUsesRawRecordedLength)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, 3 * kBlock, 1.f); // 48 frames, not any particular beat/bar multiple
    rec.stopRecordFree();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_EQ(rec.loopLengthFrames(), 3 * kBlock);
    for (size_t f = 0; f < 3 * kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f));
    }
}

TEST(LoopRecorderTest, BoundaryFadeRampsHeadAndTail)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 2.f);
    rec.stopRecordFree();
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
    rec.setFadeFrames(0);
    rec.beginRecord();
    feedConstant(rec, 32, 3.f);
    rec.stopRecordFree();
    for (size_t f = 0; f < 32; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 3.f) << "frame " << f;
    }
}

TEST(LoopRecorderTest, BoundaryFadeClampsToHalfLoopLengthForShortLoops)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(100); // far longer than the loop
    rec.beginRecord();
    feedConstant(rec, kBlock, 2.f); // exactly one block, loopLengthFrames == kBlock (16)
    rec.stopRecordFree();
    ASSERT_EQ(rec.loopLengthFrames(), kBlock);

    // Clamped fade = loopLength/2 = 8, same ramp shape as the unclamped case above.
    for (size_t k = 0; k < 8; ++k)
    {
        const float expected = 2.f * static_cast<float>(k + 1) / 8.f;
        EXPECT_FLOAT_EQ(rec.sample(k, 0), expected) << "head frame " << k;
        EXPECT_FLOAT_EQ(rec.sample(kBlock - 1 - k, 0), expected) << "tail frame " << (kBlock - 1 - k);
    }
}

TEST(LoopRecorderTest, BarLockedFinalizeOnTimeStartAndStopIsPlainCut)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feedConstant(rec, 64, 5.f);
    ASSERT_EQ(rec.recordedFrames(), 64u);

    rec.stopRecordBarLocked(64, {}, 0, 0);
    EXPECT_EQ(rec.state(), LooperState::Playing);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 5.f);
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f);
}

TEST(LoopRecorderTest, BarLockedFinalizeFoldsOvershootOntoFront)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f); // main take
    feedConstant(rec, 16, 9.f); // stop-side overshoot (a late press past the tick)
    ASSERT_EQ(rec.recordedFrames(), 80u);

    rec.stopRecordBarLocked(64, {}, 0, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // Front: main take (5.0) + overshoot (9.0), full strength near the seam,
    // fading out toward the far end of the 16-frame overshoot (fade=4).
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 5.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(12, 0), 5.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(13, 0), 5.f + 9.f * 0.75f);
    EXPECT_FLOAT_EQ(rec.sample(15, 0), 5.f + 9.f * 0.25f);
    // Middle and tail: untouched main take (no early start, so no tail fold).
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 5.f);
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f);
}

TEST(LoopRecorderTest, BarLockedFinalizeBackfillsLateStartGapFromPreRoll)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    // feedConstant rounds up to a full block; the relocate below only reads
    // capture frames [0,61), so the last 3 padding zeros are never read.
    feedConstant(rec, 61, 5.f);
    ASSERT_EQ(rec.recordedFrames(), 64u);

    const std::vector<float> preRoll = makeFlatPreRoll(3, 7.f);
    rec.stopRecordBarLocked(64, preRoll, -3, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // [0,3): backfilled directly from preRoll -- placement, not a fold.
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 7.f);
    EXPECT_FLOAT_EQ(rec.sample(2, 0), 7.f);
    // [3,64): shifted-up main take, untouched (no overshoot in this take).
    EXPECT_FLOAT_EQ(rec.sample(3, 0), 5.f);
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f);
}

TEST(LoopRecorderTest, BarLockedFinalizeFoldsEarlyStartPickupOntoTail)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(4);
    rec.beginRecord();
    // [0,3): pre-tick pickup. [3,80): aligned main take, which also covers
    // the stop-side overshoot an early start's relocate requires.
    for (size_t done = 0; done < 80; done += kBlock)
    {
        Buffer in{};
        Buffer out{};
        for (size_t i = 0; i < kBlock; ++i)
        {
            const size_t f = done + i;
            const float v = (f < 3) ? 6.f : 5.f;
            in(i, 0) = v;
            in(i, 1) = v;
        }
        rec.processBlock(in, out);
    }
    ASSERT_EQ(rec.recordedFrames(), 80u);

    rec.stopRecordBarLocked(64, {}, 3, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // Middle: shifted-down main take, untouched by either fold.
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 5.f);

    // Tail fold: the 3 pre-tick frames (6.0), fading in from 1/4 to full.
    EXPECT_FLOAT_EQ(rec.sample(61, 0), 5.f + 6.f * 0.25f);
    EXPECT_FLOAT_EQ(rec.sample(62, 0), 5.f + 6.f * 0.5f);
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f + 6.f * 0.75f);

    // Front fold: the 16-frame overshoot (5.0, from the main take's own
    // tail), full strength until the last 3 frames, then fading out.
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 5.f + 5.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(12, 0), 5.f + 5.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(13, 0), 5.f + 5.f * 0.75f);
    EXPECT_FLOAT_EQ(rec.sample(14, 0), 5.f + 5.f * 0.5f);
    EXPECT_FLOAT_EQ(rec.sample(15, 0), 5.f + 5.f * 0.25f);
    EXPECT_FLOAT_EQ(rec.sample(16, 0), 5.f); // beyond the fold window
}

TEST(LoopRecorderTest, BarLockedFinalizeCombinesLateStartAndOvershoot)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f); // main take (relocate only reads the first 62)
    feedConstant(rec, 16, 9.f); // stop-side overshoot
    ASSERT_EQ(rec.recordedFrames(), 80u);

    const std::vector<float> preRoll = makeFlatPreRoll(2, 7.f);
    rec.stopRecordBarLocked(64, preRoll, -2, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);

    // [0,2): late-start backfill (7.0) + front fold (9.0, full strength).
    EXPECT_FLOAT_EQ(rec.sample(0, 0), 7.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(1, 0), 7.f + 9.f * 1.f);
    // [2,13): shifted-up main take (5.0) + front fold still at full strength.
    EXPECT_FLOAT_EQ(rec.sample(2, 0), 5.f + 9.f * 1.f);
    EXPECT_FLOAT_EQ(rec.sample(12, 0), 5.f + 9.f * 1.f);
    // Front fold's own taper, at the far end of the 16-frame overshoot.
    EXPECT_FLOAT_EQ(rec.sample(13, 0), 5.f + 9.f * 0.75f);
    EXPECT_FLOAT_EQ(rec.sample(14, 0), 5.f + 9.f * 0.5f);
    EXPECT_FLOAT_EQ(rec.sample(15, 0), 5.f + 9.f * 0.25f);
    // Beyond the fold window: shifted-up main take, untouched.
    EXPECT_FLOAT_EQ(rec.sample(16, 0), 5.f);
    EXPECT_FLOAT_EQ(rec.sample(32, 0), 5.f);
    EXPECT_FLOAT_EQ(rec.sample(63, 0), 5.f);
}

TEST(LoopRecorderTest, BarLockedFinalizeClampsShiftToHalfLoopLength)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(2);
    rec.beginRecord();
    feedConstant(rec, kBlock, 5.f); // 16-frame main take
    feedConstant(rec, kBlock, 9.f); // generous overshoot capture
    ASSERT_EQ(rec.recordedFrames(), 32u);

    // startOffset=20 requested but loopLength/2=8 -- exercises the clamp
    // path without a crash or non-finite output.
    rec.stopRecordBarLocked(16, {}, 20, 0);
    ASSERT_EQ(rec.loopLengthFrames(), 16u);
    for (size_t f = 0; f < 16; ++f)
    {
        EXPECT_TRUE(std::isfinite(rec.sample(f, 0))) << "frame " << f;
    }
    EXPECT_EQ(rec.state(), LooperState::Playing);
}

TEST(LoopRecorderTest, BarLockedFinalizeResumesPlaybackFromCatchUpOffset)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f);
    feedConstant(rec, 16, 9.f);
    ASSERT_EQ(rec.recordedFrames(), 80u);

    // Commit is arriving late (block-boundary slop): the caller reports 10
    // frames of real time already elapsed past the stop tick, so playback
    // must resume already 10 frames into the loop.
    rec.stopRecordBarLocked(64, {}, 0, 10);
    ASSERT_EQ(rec.loopLengthFrames(), 64u);
    EXPECT_EQ(rec.playPositionFrames(), 10u);
}

TEST(LoopRecorderTest, BarLockedFinalizeWrapsCatchUpOffsetToLoopLength)
{
    Recorder rec{48000.f};
    rec.setFadeFrames(4);
    rec.beginRecord();
    feedConstant(rec, 64, 5.f);
    feedConstant(rec, 16, 9.f);

    rec.stopRecordBarLocked(64, {}, 0, 64 + 10); // one full loop plus 10
    ASSERT_EQ(rec.loopLengthFrames(), 64u);
    EXPECT_EQ(rec.playPositionFrames(), 10u);
}

TEST(LoopRecorderTest, EmptyRecordStaysEmpty)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    rec.stopRecordFree(); // nothing recorded
    EXPECT_EQ(rec.state(), LooperState::Empty);
    EXPECT_FALSE(rec.hasLoop());
}

TEST(LoopRecorderTest, OverdubOutputsMainPlusOverdubWithoutTouchingMain)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    EXPECT_EQ(rec.state(), LooperState::Overdubbing);
    const Buffer out = runOne(rec, 10.f);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 1.f + static_cast<float>(i));
    }
    rec.endOverdub();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_TRUE(rec.hasOverdub());
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f));
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 10.f);
    }
}

TEST(LoopRecorderTest, PlaybackIncludesPendingOverdubLayer)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 10.f);
    rec.endOverdub();

    const Buffer out = runOne(rec);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 1.f + static_cast<float>(i) + 10.f);
    }
}

TEST(LoopRecorderTest, OverdubDecayFadesOverdubLayerOnly)
{
    Recorder rec{48000.f};
    rec.setOverdubDecay(0.5f);
    rec.beginRecord();
    feed(rec, kBlock, 4.f); // values 4..19
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 2.f);
    runOne(rec, 2.f); // second pass: overdub = overdub*0.5 + 2 = 3
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 4.f + static_cast<float>(f));
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 3.f);
    }
}

TEST(LoopRecorderTest, UndoOverdubDiscardsLayerAndRestoresMain)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 10.f);
    rec.endOverdub();
    ASSERT_TRUE(rec.hasOverdub());

    rec.undoOverdub();
    EXPECT_FALSE(rec.hasOverdub());
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f));
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 0.f);
    }
}

TEST(LoopRecorderTest, UndoOverdubMidTakeEndsTakeAndDiscardsLayer)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 10.f);
    rec.undoOverdub();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_FALSE(rec.hasOverdub());
}

TEST(LoopRecorderTest, MixDownOverdubMergesLayerIntoMain)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 10.f);
    rec.endOverdub();

    rec.mixDownOverdub();
    EXPECT_FALSE(rec.hasOverdub());
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f) + 10.f);
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 0.f);
    }
}

TEST(LoopRecorderTest, MixDownOverdubMidTakeEndsTake)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 10.f);
    rec.mixDownOverdub();
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_FALSE(rec.hasOverdub());
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f) + 10.f);
    }
}

TEST(LoopRecorderTest, MultipleOverdubPassesAccumulateInLayer)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 3.f);
    rec.endOverdub();

    rec.beginOverdub();
    runOne(rec, 4.f);
    rec.endOverdub();

    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), 1.f + static_cast<float>(f));
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 7.f);
    }
}

TEST(LoopRecorderTest, NewTakeAfterUndoStartsWithoutStaleOverdub)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    rec.beginOverdub();
    runOne(rec, 10.f);
    rec.endOverdub();

    rec.beginRecord();
    feed(rec, kBlock, 2.f);
    rec.stopRecordFree();

    EXPECT_FALSE(rec.hasOverdub());
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 0.f);
    }
}

TEST(LoopRecorderTest, LoadOverdubInstallsSavedLayer)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, kBlock, 1.f);
    rec.stopRecordFree();

    std::vector<float> left(kBlock, 5.f);
    std::vector<float> right(kBlock, -5.f);
    rec.loadOverdub(left, right);

    EXPECT_TRUE(rec.hasOverdub());
    for (size_t f = 0; f < kBlock; ++f)
    {
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 0), 5.f);
        EXPECT_FLOAT_EQ(rec.overdubSample(f, 1), -5.f);
    }
}

TEST(LoopRecorderTest, ClearResetsToEmpty)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, 64);
    rec.stopRecordFree();
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
    rec.beginRecord();
    feed(rec, 64);
    rec.stopRecordFree();
    runOne(rec); // advance playhead by one block
    EXPECT_GT(rec.playPositionFrames(), 0u);

    rec.stop();
    EXPECT_EQ(rec.state(), LooperState::Stopped);
    EXPECT_EQ(rec.playPositionFrames(), 0u);
}

TEST(LoopRecorderTest, PauseKeepsPlayhead)
{
    Recorder rec{48000.f};
    rec.beginRecord();
    feed(rec, 64);
    rec.stopRecordFree();
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
    rec.beginRecord();
    feed(rec, 64, 1.f);
    rec.stopRecordFree();
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
    rec.beginRecord();
    feed(rec, 2000);
    rec.stopRecordFree();
    rec.beginOverdub();
    feed(rec, 2000);
    rec.endOverdub();
    for (int i = 0; i < 100; ++i)
    {
        runOne(rec);
    }
    EXPECT_EQ(rec.maxFrames(), capacityBefore);
}

TEST(LoopRecorderTest, LoadLoopEntersPlayingWithGivenContent)
{
    Recorder rec{48000.f};
    std::vector<float> left(32);
    std::vector<float> right(32);
    for (size_t f = 0; f < 32; ++f)
    {
        left[f] = 1.f + static_cast<float>(f);
        right[f] = -(1.f + static_cast<float>(f));
    }
    rec.loadLoop(left, right);
    EXPECT_EQ(rec.state(), LooperState::Playing);
    EXPECT_EQ(rec.loopLengthFrames(), 32u);
    for (size_t f = 0; f < 32; ++f)
    {
        EXPECT_FLOAT_EQ(rec.sample(f, 0), left[f]);
        EXPECT_FLOAT_EQ(rec.sample(f, 1), right[f]);
    }
}

TEST(LoopRecorderTest, LoadLoopTruncatesAtMaxFrames)
{
    Recorder rec{48000.f, 0.001f}; // maxFrames() == 48
    ASSERT_EQ(rec.maxFrames(), 48u);
    const std::vector<float> left(128, 1.f);
    const std::vector<float> right(128, -1.f);
    rec.loadLoop(left, right);
    EXPECT_EQ(rec.loopLengthFrames(), 48u);
}

TEST(LoopRecorderTest, LoadLoopWithEmptySpansClears)
{
    Recorder rec{48000.f};
    rec.loadLoop({}, {});
    EXPECT_EQ(rec.state(), LooperState::Empty);
    EXPECT_FALSE(rec.hasLoop());
}

}
