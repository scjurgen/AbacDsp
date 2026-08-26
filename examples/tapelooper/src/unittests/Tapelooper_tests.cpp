#include <cmath>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"

#include "Audio/AudioBuffer.h"
#include "impl/TapeLooperImpl.h"

// Deliberately does not depend on real (gitignored, user-supplied) sample/MIDI
// content, same reasoning as Groover_tests.cpp: the default groove path is
// content-independent for buffering mechanics (silence in, silence out).
namespace
{
constexpr size_t kBlock = 64;
constexpr float kSampleRate = 48000.f;

using TapeLooper = TapeLooperImpl<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;

constexpr size_t kTestLoopFrames = TapeLooperDetail::framesForLoop(4.f, 100.f);

Buffer sineBlock(const float amplitude, const float frequency, size_t& phaseSampleIndex)
{
    Buffer buf{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        const float phase =
            2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(phaseSampleIndex) / kSampleRate;
        const float sample = amplitude * std::sin(phase);
        buf(i, 0) = sample;
        buf(i, 1) = sample;
        ++phaseSampleIndex;
    }
    return buf;
}

float outputRms(TapeLooper& sut, const Buffer& in, const size_t numBlocks)
{
    double sumSquares = 0.0;
    size_t sampleCount = 0;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer out{};
        sut.processBlock(in, out);
        for (size_t i = 0; i < kBlock; ++i)
        {
            sumSquares += static_cast<double>(out(i, 0)) * out(i, 0);
            ++sampleCount;
        }
    }
    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(sampleCount)));
}

// One RMS per numBlocksPerChunk-sized chunk, so a dip in a single later
// repeat (rather than a uniform level change) shows up as one low entry.
std::vector<float> outputRmsPerChunk(TapeLooper& sut, const Buffer& in, const size_t numBlocksPerChunk,
                                     const size_t numChunks)
{
    std::vector<float> result;
    result.reserve(numChunks);
    for (size_t chunk = 0; chunk < numChunks; ++chunk)
    {
        result.push_back(outputRms(sut, in, numBlocksPerChunk));
    }
    return result;
}

// Feeds identical silence into both and reports whether any sample pair ever diverges -
// used to prove an effect actually reaches playback without asserting its direction/size.
bool outputsDifferAudibly(TapeLooper& a, TapeLooper& b, const size_t numBlocks)
{
    const Buffer decoyIn{};
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer outA{};
        Buffer outB{};
        a.processBlock(decoyIn, outA);
        b.processBlock(decoyIn, outB);
        for (size_t i = 0; i < kBlock; ++i)
        {
            if (std::abs(outA(i, 0) - outB(i, 0)) > 1e-6f)
            {
                return true;
            }
        }
    }
    return false;
}
}

TEST(TapeLooperTest, StoppedRecordSustainsPreviouslyRecordedLevel)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);

    const size_t numBlocks = kTestLoopFrames / kBlock;
    size_t phase = 0;
    const float recordAmplitude = 0.7f;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const auto in = sineBlock(recordAmplitude, 220.f, phase);
        Buffer out{};
        sut.processBlock(in, out);
    }
    const float recordedRms = recordAmplitude / std::numbers::sqrt2_v<float>;

    // Stop recording (still playing): whatever "live" input is fed now must be
    // ignored, and the loop just recorded must keep sounding at its own level.
    sut.setRecordA(false);
    const Buffer decoyIn{}; // silence - if this leaked through, output RMS would collapse
    const float sustainedRms = outputRms(sut, decoyIn, numBlocks);

    EXPECT_GT(sustainedRms, recordedRms * 0.5f);
    EXPECT_LT(sustainedRms, recordedRms * 1.5f);
}

TEST(TapeLooperTest, RecordingASecondPassOverdubsOntoTheFirst)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);

    const size_t numBlocks = kTestLoopFrames / kBlock;
    const float amplitude = 0.4f;
    size_t phase = 0;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const auto in = sineBlock(amplitude, 220.f, phase);
        Buffer out{};
        sut.processBlock(in, out);
    }
    phase = 0;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const auto in = sineBlock(amplitude, 880.f, phase); // second pass, different tone
        Buffer out{};
        sut.processBlock(in, out);
    }

    sut.setRecordA(false);
    const Buffer decoyIn{};
    const float overdubbedRms = outputRms(sut, decoyIn, numBlocks);

    // Two uncorrelated equal-amplitude tones combine to ~sqrt(2) times either
    // one's RMS alone, distinguishing overdub from a plain replace.
    const float singleToneRms = amplitude / std::numbers::sqrt2_v<float>;
    EXPECT_GT(overdubbedRms, singleToneRms * 1.2f);
}

// Reproduction attempt for a reported "gap" heard some repeats into sustained
// playback: checks every individual repeat's level, not just an average over
// several, so a dip in one specific later cycle would show up here.
TEST(TapeLooperTest, SustainedLoopLevelStaysConsistentAcrossManyRepeats)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);

    const size_t numBlocks = kTestLoopFrames / kBlock;
    size_t phase = 0;
    const float recordAmplitude = 0.7f;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const auto in = sineBlock(recordAmplitude, 220.f, phase);
        Buffer out{};
        sut.processBlock(in, out);
    }
    const float recordedRms = recordAmplitude / std::numbers::sqrt2_v<float>;

    sut.setRecordA(false);
    const Buffer decoyIn{};
    constexpr size_t kRepeats = 4;
    const auto perRepeatRms = outputRmsPerChunk(sut, decoyIn, numBlocks, kRepeats);

    for (size_t repeat = 0; repeat < perRepeatRms.size(); ++repeat)
    {
        EXPECT_GT(perRepeatRms[repeat], recordedRms * 0.5f) << "repeat " << repeat;
        EXPECT_LT(perRepeatRms[repeat], recordedRms * 1.5f) << "repeat " << repeat;
    }
}

TEST(TapeLooperTest, TrackNotPlayingContributesNothingBeforeAnyLoopWraparound)
{
    TapeLooper sut(kSampleRate);
    sut.setRecordA(true);
    sut.setPlayA(false); // recording but not audible
    sut.setRecordB(false);
    sut.setPlayB(true); // audible but nothing recorded yet
    sut.setRecordC(false);
    sut.setPlayC(false);

    Buffer in{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.5f;
        in(i, 1) = 0.5f;
    }

    // Far fewer samples than any preset's loop distance, so nothing has had
    // time to wrap back to the read heads regardless of play/record state;
    // the input itself still always reaches the output (live monitoring).
    for (size_t block = 0; block < 100; ++block)
    {
        Buffer out{};
        sut.processBlock(in, out);
        for (size_t i = 0; i < kBlock; ++i)
        {
            EXPECT_FLOAT_EQ(out(i, 0), 0.5f);
            EXPECT_FLOAT_EQ(out(i, 1), 0.5f);
        }
    }
}

// Proves the script's wow/flutter commands reach VariSpeedTapeDelay, not that
// pitch is more stable (out of scope here).
TEST(TapeLooperTest, WowFlutterDepthAudiblyChangesPlayback)
{
    const auto record = [](TapeLooper& sut)
    {
        sut.setBars(4.f);
        sut.setBpm(100.f);
        sut.setTapeSpeed(1.f);
        sut.setRecordA(true);
        sut.setPlayA(true);
        size_t phase = 0;
        for (size_t block = 0; block < kTestLoopFrames / kBlock; ++block)
        {
            const auto in = sineBlock(0.7f, 220.f, phase);
            Buffer out{};
            sut.processBlock(in, out);
        }
        sut.setRecordA(false);
    };

    TapeLooper modulated(kSampleRate);
    record(modulated);

    TapeLooper flat(kSampleRate);
    ASSERT_TRUE(flat.setScript("SetTrackWow(0, 0, 0, 0)\nSetTrackFlutter(0, 0, 0)\n"));
    record(flat);

    EXPECT_TRUE(outputsDifferAudibly(modulated, flat, kTestLoopFrames / kBlock));
}

// Lua-only Phase 7 effects: each is proven by comparing a scripted instance against an
// untouched (all-default, i.e. neutral) one, the same audible-difference approach as
// WowFlutterDepthAudiblyChangesPlayback above.
namespace
{
void recordATone(TapeLooper& sut)
{
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);
    size_t phase = 0;
    for (size_t block = 0; block < kTestLoopFrames / kBlock; ++block)
    {
        const auto in = sineBlock(0.7f, 220.f, phase);
        Buffer out{};
        sut.processBlock(in, out);
    }
    sut.setRecordA(false);
}
}

TEST(TapeLooperTest, ScriptDriveAudiblyChangesPlayback)
{
    TapeLooper clean(kSampleRate);
    recordATone(clean);

    TapeLooper driven(kSampleRate);
    ASSERT_TRUE(driven.setScript("SetTrackDrive(0, 1)"));
    recordATone(driven);

    EXPECT_TRUE(outputsDifferAudibly(clean, driven, kTestLoopFrames / kBlock));
}

TEST(TapeLooperTest, ScriptChorusAudiblyChangesPlayback)
{
    TapeLooper dry(kSampleRate);
    recordATone(dry);

    TapeLooper chorused(kSampleRate);
    ASSERT_TRUE(chorused.setScript("SetTrackChorus(0, 1, 2)"));
    recordATone(chorused);

    EXPECT_TRUE(outputsDifferAudibly(dry, chorused, kTestLoopFrames / kBlock));
}

TEST(TapeLooperTest, ScriptEchoAudiblyChangesPlayback)
{
    TapeLooper dry(kSampleRate);
    recordATone(dry);

    TapeLooper echoed(kSampleRate);
    ASSERT_TRUE(echoed.setScript("SetTrackEcho(0, 4, 0.5)"));
    recordATone(echoed);

    EXPECT_TRUE(outputsDifferAudibly(dry, echoed, kTestLoopFrames / kBlock));
}

TEST(TapeLooperTest, ScriptCompressorReducesLoudPlaybackLevel)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);
    size_t phase = 0;
    const size_t numBlocks = kTestLoopFrames / kBlock;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer out{};
        sut.processBlock(sineBlock(0.9f, 220.f, phase), out);
    }
    sut.setRecordA(false);

    const Buffer decoyIn{};
    const float uncompressedRms = outputRms(sut, decoyIn, numBlocks);

    ASSERT_TRUE(sut.setScript("SetTrackCompressor(0, -40, 8, 5, 50)"));
    outputRms(sut, decoyIn, 20); // let the envelope follower settle
    const float compressedRms = outputRms(sut, decoyIn, numBlocks);

    EXPECT_LT(compressedRms, uncompressedRms * 0.8f);
}

TEST(TapeLooperTest, ScriptRingModAudiblyChangesPlayback)
{
    TapeLooper dry(kSampleRate);
    recordATone(dry);

    TapeLooper ringModded(kSampleRate);
    ASSERT_TRUE(ringModded.setScript("SetTrackRingMod(0, 300, 1)"));
    recordATone(ringModded);

    EXPECT_TRUE(outputsDifferAudibly(dry, ringModded, kTestLoopFrames / kBlock));
}

TEST(TapeLooperTest, ScriptTremoloAudiblyChangesPlayback)
{
    TapeLooper dry(kSampleRate);
    recordATone(dry);

    TapeLooper tremoloed(kSampleRate);
    ASSERT_TRUE(tremoloed.setScript("SetTrackTremolo(0, 5, 1, 1)"));
    recordATone(tremoloed);

    EXPECT_TRUE(outputsDifferAudibly(dry, tremoloed, kTestLoopFrames / kBlock));
}

// Distortion before vs. after the filter is a genuinely different signal (a resonant
// filter shapes what gets clipped, vs. clipping shaping what gets filtered) - proves
// SetTrackChain's reorder actually changes processing order, not just node enablement.
TEST(TapeLooperTest, ScriptChainReorderAudiblyChangesProcessing)
{
    TapeLooper defaultOrder(kSampleRate);
    ASSERT_TRUE(defaultOrder.setScript("SetTrackDrive(0, 1)\nSetTrackFilter(0, 400, 0.8)"));
    recordATone(defaultOrder);

    TapeLooper reordered(kSampleRate);
    ASSERT_TRUE(reordered.setScript("SetTrackDrive(0, 1)\nSetTrackFilter(0, 400, 0.8)\n"
                                    "SetTrackChain(0, {\"distortion\", \"filter\", \"chorus\", \"echo\", "
                                    "\"compressor\", \"ringmod\", \"tremolo\"})"));
    recordATone(reordered);

    EXPECT_TRUE(outputsDifferAudibly(defaultOrder, reordered, kTestLoopFrames / kBlock));
}

// Compares one instance against itself, before vs. after the bad call - not two separate
// instances, since nothing in this file establishes that two independently constructed
// instances stay bit-identical (every other multi-instance test here asserts a difference).
TEST(TapeLooperTest, ScriptChainRejectsInvalidNodeNameKeepingPreviousChain)
{
    TapeLooper sut(kSampleRate);
    ASSERT_TRUE(sut.setScript("SetTrackDrive(0, 1)")); // gives the chain something to preserve
    recordATone(sut);

    const Buffer decoyIn{};
    const size_t numBlocks = kTestLoopFrames / kBlock;
    const float rmsBeforeBadCall = outputRms(sut, decoyIn, numBlocks);

    ASSERT_TRUE(sut.setScript("SetTrackChain(0, {\"not-a-real-node\"})"));
    const float rmsAfterBadCall = outputRms(sut, decoyIn, numBlocks);

    EXPECT_NEAR(rmsAfterBadCall, rmsBeforeBadCall, rmsBeforeBadCall * 0.1f + 1e-6f);
}

// Script-driven record/play reaches the same applied state as the host setters do -
// started via script, stopped via host, proving both paths share one underlying state.
TEST(TapeLooperTest, ScriptCanDriveTrackRecordAndPlay)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    ASSERT_TRUE(sut.setScript("SetTrackRecord(0, true)\nSetTrackPlay(0, true)\n"));

    const size_t numBlocks = kTestLoopFrames / kBlock;
    size_t phase = 0;
    const float recordAmplitude = 0.7f;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const auto in = sineBlock(recordAmplitude, 220.f, phase);
        Buffer out{};
        sut.processBlock(in, out);
    }
    const float recordedRms = recordAmplitude / std::numbers::sqrt2_v<float>;

    sut.setRecordA(false);
    const Buffer decoyIn{};
    const float sustainedRms = outputRms(sut, decoyIn, numBlocks);

    EXPECT_GT(sustainedRms, recordedRms * 0.5f);
    EXPECT_LT(sustainedRms, recordedRms * 1.5f);
}

// End-to-end with the engine's own default stub script: recording on track A fires
// OnRecordStateChanged, which the script answers with SetGrooveSource("click").
TEST(TapeLooperTest, DefaultScriptSwitchesToClickWhileRecording)
{
    TapeLooper sut(kSampleRate);
    sut.setBpm(120.f);
    sut.setGroovePlay(true);

    const Buffer in{};
    const size_t numBlocks = 50;
    const float silentGrooveRms = outputRms(sut, in, numBlocks);
    EXPECT_LT(silentGrooveRms, 1e-6f);

    sut.setRecordA(true);
    Buffer out{};
    sut.processBlock(in, out); // applies the record edge and drains the resulting script command
    // The groove tape's fixed ~4800-sample read-behind-write delay (VariSpeedTapeDelay's
    // own default read head) must be fed past once before playback catches up to it.
    const float clickRms = outputRms(sut, in, 200);
    EXPECT_GT(clickRms, 1e-4f);
}

// The linear-ramp smoother reaches its target within one block, so a fade set via script
// is fully in effect by the very next block processed after it.
TEST(TapeLooperTest, ScriptTrackGainFadesOutAndBackIn)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);

    const size_t numBlocks = kTestLoopFrames / kBlock;
    size_t phase = 0;
    const float recordAmplitude = 0.7f;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        const auto in = sineBlock(recordAmplitude, 220.f, phase);
        Buffer out{};
        sut.processBlock(in, out);
    }
    sut.setRecordA(false);
    const float recordedRms = recordAmplitude / std::numbers::sqrt2_v<float>;

    const Buffer decoyIn{};
    const float baselineRms = outputRms(sut, decoyIn, numBlocks);
    EXPECT_GT(baselineRms, recordedRms * 0.5f);

    ASSERT_TRUE(sut.setScript("SetTrackGain(0, 0)"));
    Buffer rampOut{};
    sut.processBlock(decoyIn, rampOut);
    const float fadedRms = outputRms(sut, decoyIn, numBlocks);
    EXPECT_LT(fadedRms, recordedRms * 0.05f);

    ASSERT_TRUE(sut.setScript("SetTrackGain(0, 1)"));
    sut.processBlock(decoyIn, rampOut);
    const float restoredRms = outputRms(sut, decoyIn, numBlocks);
    EXPECT_GT(restoredRms, recordedRms * 0.5f);
}

// A script-driven low-pass on track A audibly attenuates a high tone recorded there,
// while track B (never filtered) stays unaffected - proves both the effect and its
// per-track independence.
TEST(TapeLooperTest, ScriptTrackFilterAttenuatesHighTonePerTrackOnly)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    const size_t numBlocks = kTestLoopFrames / kBlock;

    sut.setRecordA(true);
    size_t phaseA = 0;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer out{};
        sut.processBlock(sineBlock(0.7f, 8000.f, phaseA), out);
    }
    sut.setRecordA(false);

    sut.setRecordB(true);
    size_t phaseB = 0;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer out{};
        sut.processBlock(sineBlock(0.7f, 8000.f, phaseB), out);
    }
    sut.setRecordB(false);
    sut.setPlayA(true);
    sut.setPlayB(true);

    const Buffer decoyIn{};
    sut.setPlayB(false);
    const float aBeforeRms = outputRms(sut, decoyIn, numBlocks);
    sut.setPlayA(false);
    sut.setPlayB(true);
    const float bBeforeRms = outputRms(sut, decoyIn, numBlocks);

    ASSERT_TRUE(sut.setScript("SetTrackFilter(0, 200, 0, \"LP4\")"));
    sut.setPlayA(true);
    outputRms(sut, decoyIn, 20); // let the filter's internal smoothing converge

    sut.setPlayB(false);
    const float aAfterRms = outputRms(sut, decoyIn, numBlocks);
    sut.setPlayA(false);
    sut.setPlayB(true);
    const float bAfterRms = outputRms(sut, decoyIn, numBlocks);

    EXPECT_LT(aAfterRms, aBeforeRms * 0.2f);
    EXPECT_GT(bAfterRms, bBeforeRms * 0.7f);
    EXPECT_LT(bAfterRms, bBeforeRms * 1.3f);
}

// Cutting Play stops the dry signal and any new feed into the reverb bus, but not the bus's
// own already-excited state - a raised send keeps ringing briefly after; send 0 does not.
TEST(TapeLooperTest, ScriptReverbSendProducesATailAfterPlayStops)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    sut.setPlayA(true);

    const size_t numBlocks = kTestLoopFrames / kBlock;
    size_t phase = 0;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer out{};
        sut.processBlock(sineBlock(0.7f, 440.f, phase), out);
    }
    sut.setRecordA(false);

    const Buffer decoyIn{};
    sut.setPlayA(false);
    const float dryTailRms = outputRms(sut, decoyIn, 20);
    EXPECT_LT(dryTailRms, 1e-5f);

    sut.setPlayA(true);
    ASSERT_TRUE(sut.setScript("SetTrackReverbSend(0, 1)\nSetReverbDecay(800)"));
    outputRms(sut, decoyIn, 50); // feed the reverb bus for a while before cutting the source
    sut.setPlayA(false);
    const float wetTailRms = outputRms(sut, decoyIn, 20);
    EXPECT_GT(wetTailRms, 1e-4f);
    EXPECT_GT(wetTailRms, dryTailRms * 10.f);
}

TEST(TapeLooperTest, GrooveBuffersOnlyFillWhilePlaying)
{
    TapeLooper sut(kSampleRate);
    const Buffer in{};

    for (size_t block = 0; block < 20; ++block)
    {
        Buffer out{};
        sut.processBlock(in, out);
    }
    EXPECT_EQ(sut.grooveLoopBufferFramesAheadForTest(), 0u);

    sut.setGroovePlay(true);
    Buffer out{};
    sut.processBlock(in, out);
    EXPECT_GT(sut.grooveLoopBufferFramesAheadForTest(), 0u);
}

// Tape speed multiplies the effective groove/click tempo: 2x speed halves the beat clock.
TEST(TapeLooperTest, TapeSpeedScalesTheGrooveClickBeatClock)
{
    TapeLooper sut(kSampleRate);
    sut.setBpm(120.f);
    sut.setTapeSpeed(1.f);
    const Buffer in{};
    Buffer out{};
    sut.processBlock(in, out);
    const auto baseSpb = sut.samplesPerBeatForTest();

    sut.setTapeSpeed(2.f);
    sut.processBlock(in, out);
    const auto doubledSpeedSpb = sut.samplesPerBeatForTest();

    EXPECT_NEAR(static_cast<double>(doubledSpeedSpb), static_cast<double>(baseSpb) / 2.0, 1.0);
}

// No real kit ever loads in this test file (see the top-of-file note), so every instrument
// name here is necessarily untagged - this proves that stays a silent no-op, per the plan's
// own risk callout, rather than a crash, while the groove track is actively playing.
TEST(TapeLooperTest, ScriptInstrumentCommandsAreSafeNoOpsWithoutALoadedKit)
{
    TapeLooper sut(kSampleRate);
    ASSERT_TRUE(sut.setScript("SetInstrumentGain(\"kick\", 0.3)\n"
                              "MuteInstrument(\"snare\")\n"
                              "SetInstrumentReverbSend(\"hihat_closed\", 0.8)\n"
                              "SetInstrumentGain(\"not_a_real_instrument\", 0.5)\n"));
    sut.setGroovePlay(true);
    const Buffer in{};
    for (int block = 0; block < 20; ++block)
    {
        Buffer out{};
        sut.processBlock(in, out);
        for (size_t i = 0; i < kBlock; ++i)
        {
            EXPECT_FLOAT_EQ(out(i, 0), 0.f);
            EXPECT_FLOAT_EQ(out(i, 1), 0.f);
        }
    }
}
