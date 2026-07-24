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
    rec.setSamplesPerBar(0); // no quantization
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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

TEST(LoopRecorderTest, BarQuantizationRoundsToNearestBar)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBar(100);

    rec.beginRecord();
    feed(rec, 240); // 2.4 bars -> rounds down to 2 bars = 200
    rec.stopRecord();
    EXPECT_EQ(rec.loopLengthFrames(), 200u);

    rec.clear();
    rec.setSamplesPerBar(100);
    rec.beginRecord();
    feed(rec, 260); // 2.6 bars -> rounds up to 3 bars = 300
    rec.stopRecord();
    EXPECT_EQ(rec.loopLengthFrames(), 300u);
}

TEST(LoopRecorderTest, QuantizedTailIsSilent)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBar(100);
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

TEST(LoopRecorderTest, ShorterReRecordDoesNotLeakStaleTail)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBar(100);
    rec.beginRecord();
    feed(rec, 300, 5.f); // fills frames 0..299 with nonzero
    rec.stopRecord();

    rec.clear();
    rec.setSamplesPerBar(100);
    rec.beginRecord();
    feed(rec, 60, 2.f); // 0.6 bar -> 1 bar = 100; frames 60..99 must be zeroed
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
    rec.setSamplesPerBar(100);
    rec.beginRecord();
    rec.stopRecord(); // nothing recorded
    EXPECT_EQ(rec.state(), LooperState::Empty);
    EXPECT_FALSE(rec.hasLoop());
}

TEST(LoopRecorderTest, OverdubSumsIntoExistingLoop)
{
    Recorder rec{48000.f};
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(0);
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
    rec.setSamplesPerBar(240);
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
