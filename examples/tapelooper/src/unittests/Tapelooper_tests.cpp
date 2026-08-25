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

// Proves the new setters reach VariSpeedTapeDelay, not that pitch is more
// stable (out of scope for wiring-only Phase 2).
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
    flat.setWowDepthA(0.f);
    flat.setWowRateA(0.f);
    flat.setWowDriftA(0.f);
    flat.setFlutterDepthA(0.f);
    flat.setFlutterRateA(0.f);
    record(flat);

    const Buffer decoyIn{};
    const size_t numBlocks = kTestLoopFrames / kBlock;
    bool sawDifference = false;
    for (size_t block = 0; block < numBlocks && !sawDifference; ++block)
    {
        Buffer modulatedOut{};
        Buffer flatOut{};
        modulated.processBlock(decoyIn, modulatedOut);
        flat.processBlock(decoyIn, flatOut);
        for (size_t i = 0; i < kBlock; ++i)
        {
            if (std::abs(modulatedOut(i, 0) - flatOut(i, 0)) > 1e-6f)
            {
                sawDifference = true;
                break;
            }
        }
    }
    EXPECT_TRUE(sawDifference);
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

// Regression for a real Phase 5 bug: the Filter Mode dial's raw 0-3 selection was used
// directly as a poleMixingList index, so every curated position landed on LP1-LP4 - the
// dial's "HP4" position (curated index 1) must actually cut a low tone, not pass it like LP4.
TEST(TapeLooperTest, HostFilterModeDropResolvesToCuratedPreset)
{
    TapeLooper sut(kSampleRate);
    sut.setBars(4.f);
    sut.setBpm(100.f);
    sut.setTapeSpeed(1.f);
    sut.setRecordA(true);
    size_t phase = 0;
    const size_t numBlocks = kTestLoopFrames / kBlock;
    for (size_t block = 0; block < numBlocks; ++block)
    {
        Buffer out{};
        sut.processBlock(sineBlock(0.7f, 100.f, phase), out);
    }
    sut.setRecordA(false);
    sut.setPlayA(true);
    sut.setFilterCutoffA(4000.f);
    sut.setFilterResonanceA(0.f);

    const Buffer decoyIn{};
    sut.setFilterModeA(0); // curated LP4
    const float lowPassRms = outputRms(sut, decoyIn, numBlocks);

    sut.setFilterModeA(1); // curated HP4
    const float highPassRms = outputRms(sut, decoyIn, numBlocks);

    EXPECT_LT(highPassRms, lowPassRms * 0.5f);
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
