#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "impl/LooperImpl.h"

// 3x3 matrix test for the Phase 8b beat-lock: start timing (before/on/after
// the tick) x stop timing (before/on/after the tick). Each case injects a
// single "doublet" (-1.0 then +1.0) as the threshold-crossing signal at a
// precisely controlled offset from the start tick, and verifies:
//   1. loopLength always comes out as the same exact beat-multiple, and
//   2. the doublet lands at the expected loop frame for its start category,
// regardless of which stop-timing sub-case ran (stop timing, within
// tolerance, must not affect what's captured -- that's the invariant this
// test exists to check). See examples/looper/PLAN.md Phase 8b.
namespace
{
constexpr size_t kBlock = 16;
// Chosen so every derived quantity (including kRoll = spb/8) is itself a
// multiple of kBlock -- keeps every silence run in this test exactly
// block-aligned, not just the doublet placements.
constexpr float kSampleRate = 5120.f;
constexpr float kBpm = 120.f;
constexpr size_t kSamplesPerBeat = 2560;      // kSampleRate*60/kBpm
constexpr size_t kRoll = kSamplesPerBeat / 8; // 320
constexpr size_t kBeatsPerTake = 2;
constexpr size_t kExpectedLoopLength = kSamplesPerBeat * kBeatsPerTake; // 5120

// Block-aligned offsets (multiples of kBlock) so "trig" (block-start
// granularity) exactly matches the doublet's true position -- keeps this
// test isolated from the separate, already-accepted block-granularity
// limitation.
constexpr size_t kStartOffset = 144; // 9*16, < kRoll
constexpr size_t kStopOffset = 96;   // 6*16, < kRoll

using Looper = LooperImpl<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;

enum class Timing
{
    Before,
    On,
    After
};

const char* timingName(const Timing t)
{
    switch (t)
    {
        case Timing::Before:
            return "Before";
        case Timing::On:
            return "On";
        case Timing::After:
            return "After";
    }
    return "?";
}

void runSilence(Looper& looper, const size_t frames)
{
    ASSERT_EQ(frames % kBlock, 0u) << "test offsets must be block-aligned";
    Buffer in{};
    Buffer out{};
    for (size_t done = 0; done < frames; done += kBlock)
    {
        looper.processBlock(in, out);
    }
}

// One block: sample 0 = -1.0 (crosses the threshold, defining "trig" at this
// block's start), sample 1 = +1.0, the rest silent.
void runDoubletBlock(Looper& looper)
{
    Buffer in{};
    Buffer out{};
    in(0, 0) = -1.f;
    in(0, 1) = -1.f;
    in(1, 0) = 1.f;
    in(1, 1) = 1.f;
    looper.processBlock(in, out);
}

// Scans [0, loopLength) for the exact two-sample doublet pattern. Returns the
// index of the -1.0 sample, or std::nullopt if not found (or found more than
// once, which would itself indicate a bug -- treated as not-found so the
// caller's EXPECT_EQ against a concrete index fails with a clear diagnostic).
std::optional<size_t> findDoublet(const Looper& looper, const size_t loopLength)
{
    std::optional<size_t> found;
    for (size_t f = 0; f + 1 < loopLength; ++f)
    {
        const bool isDoublet = looper.rawLoopSample(f, 0) < -0.9f && looper.rawLoopSample(f + 1, 0) > 0.9f;
        if (isDoublet)
        {
            if (found.has_value())
            {
                return std::nullopt; // more than one match: ambiguous, fail loudly
            }
            found = f;
        }
    }
    return found;
}

// Named (not inline-lambda) so the comma in a structured binding never lands
// inside the INSTANTIATE_TEST_SUITE_P macro's argument list -- the C
// preprocessor doesn't track [] nesting, so `[a, b] = ...` written directly
// inside the macro call splits into extra macro arguments.
std::string beatLockTestName(const ::testing::TestParamInfo<std::tuple<Timing, Timing>>& info)
{
    const Timing startTiming = std::get<0>(info.param);
    const Timing stopTiming = std::get<1>(info.param);
    return std::string("Start") + timingName(startTiming) + "_Stop" + timingName(stopTiming);
}

}

namespace
{
class BeatLockMatrixTest : public ::testing::TestWithParam<std::tuple<Timing, Timing>>
{
};
}

TEST_P(BeatLockMatrixTest, DoubletLandsAtExpectedFrame)
{
    const auto [startTiming, stopTiming] = GetParam();
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(true);
    looper.setRecThreshold(-24.f);
    looper.setFadeMs(1.f);

    looper.setRecord(true); // arm
    runSilence(looper, kBlock);

    constexpr size_t sTarget = kSamplesPerBeat;
    const long startShift = (startTiming == Timing::Before)  ? -static_cast<long>(kStartOffset)
                            : (startTiming == Timing::After) ? static_cast<long>(kStartOffset)
                                                             : 0;
    const auto trigTarget = static_cast<size_t>(static_cast<long>(sTarget) + startShift);

    // Already at absolute position kBlock (the arm block above); advance to
    // trigTarget, not by trigTarget more.
    ASSERT_GT(trigTarget, kBlock);
    runSilence(looper, trigTarget - kBlock);
    runDoubletBlock(looper);
    ASSERT_TRUE(looper.isRecording()) << "recording must start immediately on threshold crossing";

    constexpr size_t eTarget = sTarget + kExpectedLoopLength;
    const long stopShift = (stopTiming == Timing::Before)  ? -static_cast<long>(kStopOffset)
                           : (stopTiming == Timing::After) ? static_cast<long>(kStopOffset)
                                                           : 0;
    const auto stopRequestTarget = static_cast<size_t>(static_cast<long>(eTarget) + stopShift);

    const size_t pos = trigTarget + kBlock;
    ASSERT_GT(stopRequestTarget, pos) << "test offsets left no room for the main take";
    runSilence(looper, stopRequestTarget - pos);

    looper.setRecord(true); // request stop
    runSilence(looper, kBlock);

    const size_t nowAbs = stopRequestTarget + kBlock;
    constexpr size_t finalizeAbs = eTarget + kRoll + kBlock;
    ASSERT_GT(finalizeAbs, nowAbs);
    runSilence(looper, finalizeAbs - nowAbs);

    ASSERT_TRUE(looper.isPlaying()) << "beat-locked take should have finalized by now";

    // Invariant under test: loop length depends only on the beat grid
    // (kBeatsPerTake beats from sTarget to eTarget), never on stop-request
    // timing within tolerance.
    EXPECT_EQ(looper.rawLoopLengthFrames(), kExpectedLoopLength)
        << "start=" << timingName(startTiming) << " stop=" << timingName(stopTiming);

    const auto doublet = findDoublet(looper, kExpectedLoopLength);
    ASSERT_TRUE(doublet.has_value()) << "doublet not found in [0, " << kExpectedLoopLength
                                     << ") for start=" << timingName(startTiming) << " stop=" << timingName(stopTiming);

    size_t expected = 0;
    if (startTiming == Timing::Before)
    {
        expected = kExpectedLoopLength - kStartOffset;
    }
    else if (startTiming == Timing::After)
    {
        expected = kStartOffset;
    }
    EXPECT_EQ(*doublet, expected) << "start=" << timingName(startTiming) << " stop=" << timingName(stopTiming);
}

INSTANTIATE_TEST_SUITE_P(StartXStop, BeatLockMatrixTest,
                         ::testing::Combine(::testing::Values(Timing::Before, Timing::On, Timing::After),
                                            ::testing::Values(Timing::Before, Timing::On, Timing::After)),
                         beatLockTestName);

// Output-stream timing test, still fully isolated (LooperImpl only, no JUCE,
// no host/plugin layer). Real 48 kHz, 60 BPM -- 1 beat = 1 second exactly --
// so a loop of N beats must play the recorded signal back at exactly N
// seconds after playback starts, on *every* repeat (no drift), regardless of
// where within the beat the signal was actually played relative to the tick.
// This measures the actual output audio stream over several loop cycles,
// not just the stored buffer, unlike BeatLockMatrixTest above.
namespace
{
constexpr float kRealSampleRate = 48000.f;
constexpr float kRealBpm = 60.f;
constexpr size_t kRealSamplesPerBeat = 48000; // kRealSampleRate*60/kRealBpm
constexpr size_t kRealRoll = kRealSamplesPerBeat / 8;
constexpr size_t kRealStartOffset = 800; // 50*16, < kRealRoll, block-aligned
constexpr size_t kRealStopOffset = 1600; // 100*16, < kRealRoll, block-aligned

// Runs `frames` samples of silence, appending the left-channel output to `out`.
void captureOutput(Looper& looper, const size_t frames, std::vector<float>& out)
{
    ASSERT_EQ(frames % kBlock, 0u) << "test offsets must be block-aligned";
    Buffer in{};
    Buffer buf{};
    for (size_t done = 0; done < frames; done += kBlock)
    {
        looper.processBlock(in, buf);
        for (size_t i = 0; i < kBlock; ++i)
        {
            out.push_back(buf(i, 0));
        }
    }
}

// Runs blocks (discarding output) until the take finalizes. The commit block
// itself still outputs silence (the recorder was in Recording state for its
// own whole processing); capturing starts on the next block so the first
// captured sample is genuinely the first Playing-state one, with zero slop.
void runUntilPlaying(Looper& looper)
{
    Buffer in{};
    Buffer out{};
    while (!looper.isPlaying())
    {
        looper.processBlock(in, out);
    }
}

// All indices of the -1.0,+1.0 doublet pattern in `stream`, in order.
// A rising-step detector (not an absolute-value threshold): robust to a small
// click contribution summed onto an on-beat pulse (by design, some pulses in
// ClickAlignmentTest below land exactly where a click also fires). The click
// itself is a slowly-varying damped sine -- its own sample-to-sample delta is
// tiny -- while the doublet's is a clean +2.0 step, so a coarse threshold
// well under 2.0 still cleanly separates the two.
std::vector<size_t> findAllDoublets(const std::vector<float>& stream)
{
    std::vector<size_t> found;
    for (size_t i = 0; i + 1 < stream.size(); ++i)
    {
        if (stream[i + 1] - stream[i] > 1.5f)
        {
            found.push_back(i);
        }
    }
    return found;
}

// Named (not inline-lambda), same reason as beatLockTestName above.
std::string outputTimingTestName(const ::testing::TestParamInfo<std::tuple<size_t, Timing, Timing>>& info)
{
    const size_t beats = std::get<0>(info.param);
    const Timing startTiming = std::get<1>(info.param);
    const Timing stopTiming = std::get<2>(info.param);
    return std::string("Beats") + std::to_string(beats) + "_Start" + timingName(startTiming) + "_Stop" +
           timingName(stopTiming);
}
}

class OutputTimingTest : public ::testing::TestWithParam<std::tuple<size_t, Timing, Timing>>
{
};

TEST_P(OutputTimingTest, SignalReappearsExactlyOneLoopLengthApartEveryRepeat)
{
    const auto [beats, startTiming, stopTiming] = GetParam();
    const size_t expectedLoopLength = kRealSamplesPerBeat * beats;

    Looper looper(kRealSampleRate);
    looper.setBpm(kRealBpm);
    looper.setThreshRec(true);
    looper.setRecThreshold(-24.f);
    looper.setFadeMs(1.f);

    looper.setRecord(true); // arm
    runSilence(looper, kBlock);

    constexpr size_t sTarget = kRealSamplesPerBeat;
    const long startShift = (startTiming == Timing::Before)  ? -static_cast<long>(kRealStartOffset)
                            : (startTiming == Timing::After) ? static_cast<long>(kRealStartOffset)
                                                             : 0;
    const auto trigTarget = static_cast<size_t>(static_cast<long>(sTarget) + startShift);

    ASSERT_GT(trigTarget, kBlock);
    runSilence(looper, trigTarget - kBlock);
    runDoubletBlock(looper); // the "signal": crosses threshold, recording starts immediately
    ASSERT_TRUE(looper.isRecording());

    // Stop-request timing (before/on/after the tick, within tolerance) never
    // changes the outcome -- it still beat-locks to the same tick, per
    // BeatLockMatrixTest's invariant -- so this is pure robustness coverage.
    const size_t eTarget = sTarget + expectedLoopLength;
    const long stopShift = (stopTiming == Timing::Before)  ? -static_cast<long>(kRealStopOffset)
                           : (stopTiming == Timing::After) ? static_cast<long>(kRealStopOffset)
                                                           : 0;
    const auto stopRequestTarget = static_cast<size_t>(static_cast<long>(eTarget) + stopShift);

    const size_t pos = trigTarget + kBlock;
    ASSERT_GT(stopRequestTarget, pos) << "not enough beats for the offset to fit";
    runSilence(looper, stopRequestTarget - pos);

    looper.setRecord(true); // request stop
    runSilence(looper, kBlock);

    // Zero-slop wait: capture starts exactly on the first Playing-state block,
    // whatever block that turns out to be for this stop-timing sub-case.
    runUntilPlaying(looper);
    ASSERT_TRUE(looper.isPlaying());
    ASSERT_EQ(looper.rawLoopLengthFrames(), expectedLoopLength) << "beats=" << beats;

    // Capture four full loop repeats of pure silence at the input: whatever
    // comes out is entirely the looper's own played-back signal.
    std::vector<float> stream;
    captureOutput(looper, expectedLoopLength * 4, stream);

    const std::vector<size_t> hits = findAllDoublets(stream);
    ASSERT_EQ(hits.size(), 4u) << "expected exactly one doublet per loop repeat, beats=" << beats
                               << " start=" << timingName(startTiming) << " stop=" << timingName(stopTiming);

    // The doublet's position within the loop buffer: purely a function of
    // start timing (see BeatLockMatrixTest), unaffected by the catch-up below.
    size_t loopFrame = 0;
    if (startTiming == Timing::Before)
    {
        loopFrame = expectedLoopLength - kRealStartOffset;
    }
    else if (startTiming == Timing::After)
    {
        loopFrame = kRealStartOffset;
    }

    // Playback resumes catchUp frames into the loop, not at frame 0 (see
    // LoopRecorder::finalizeBeatLocked): the commit only fires one block after
    // the tick+roll target. Fixed here regardless of beats/timing, since both
    // the tick and rollFrames are exact multiples of the block size.
    constexpr size_t catchUp = (kRealRoll + kBlock) % kRealSamplesPerBeat;
    const size_t expectedFirst = (loopFrame + expectedLoopLength - catchUp) % expectedLoopLength;
    EXPECT_EQ(hits[0], expectedFirst) << "beats=" << beats << " start=" << timingName(startTiming)
                                      << " stop=" << timingName(stopTiming);

    // The real point of this test: every repeat lands exactly one loop length
    // later than the previous one -- in seconds, exactly `beats` seconds later
    // (60 BPM == 1 beat/second) -- with zero drift across repeats.
    for (size_t k = 1; k < hits.size(); ++k)
    {
        EXPECT_EQ(hits[k] - hits[k - 1], expectedLoopLength)
            << "repeat " << k << " drifted, beats=" << beats << " start=" << timingName(startTiming)
            << " stop=" << timingName(stopTiming);
    }
}

INSTANTIATE_TEST_SUITE_P(BeatsXStartXStop, OutputTimingTest,
                         ::testing::Combine(::testing::Values(1u, 2u, 3u, 4u),
                                            ::testing::Values(Timing::Before, Timing::On, Timing::After),
                                            ::testing::Values(Timing::Before, Timing::On, Timing::After)),
                         outputTimingTestName);

// Independent cross-check: compares our fed signal against the CLICK's own
// output, not against internally-computed "expected" positions -- the click
// is generated by a completely separate path (ClickGenerator, triggered off
// BeatSequencer's beatStart) from the fold logic under test, so agreement
// between the two is real evidence, not circular reasoning.
//
// Procedure (matches the user's spec exactly): arm, feed the trigger pulse
// 1/8 beat before beat 1 (this both crosses the threshold and is pulse #1),
// then feed one pulse exactly on beat 1, 2, 3, 4, then stop on-time at beat 5.
// Real 60 BPM/48 kHz (1 beat = 1 second).
//
// Phase A (during recording): the loop is silent while capturing (LoopRecorder
// outputs 0 while Recording), so out = in + click exactly -- click[i] =
// out[i] - in[i] isolates the click with zero ambiguity. Verifies the click
// itself fires exactly on beat 1..4, independent of anything under test.
//
// Phase B (during playback): dry input is silent, so away from our pulses,
// out = click exactly (loop content is silent everywhere except the 5
// recorded pulses). Verifies every played-back pulse coincides with a click,
// on every loop repeat.
namespace
{
constexpr float kClickTestBpm = 60.f;
constexpr size_t kClickTestSpb = 48000; // kRealSampleRate*60/kClickTestBpm
constexpr size_t kClickTestEighth = kClickTestSpb / 8;
// The pickup pulse sits safely *inside* the roll window (a sixteenth before
// the beat, not exactly a full eighth): landing exactly at the tolerance
// edge (== roll) puts it at the roll window's outermost sample, which is
// where the boundary fade is strongest by design (fades in approaching the
// beat) -- that's correct behavior, but it makes the pulse itself nearly
// silent and a poor thing to pattern-match on for this test.
constexpr size_t kClickTestPickupOffset = 2880; // 180*16, comfortably inside the roll window

// Runs `frames` samples of silence, recording both channels' input and the
// left-channel output in lockstep (for the out-in click isolation).
void stepSilenceRecordBoth(Looper& looper, const size_t frames, std::vector<float>& inLog, std::vector<float>& outLog)
{
    ASSERT_EQ(frames % kBlock, 0u);
    Buffer in{};
    Buffer out{};
    for (size_t done = 0; done < frames; done += kBlock)
    {
        looper.processBlock(in, out);
        for (size_t i = 0; i < kBlock; ++i)
        {
            inLog.push_back(in(i, 0));
            outLog.push_back(out(i, 0));
        }
    }
}


// One doublet block (the "signal"), same in/out logging.
void stepDoubletRecordBoth(Looper& looper, std::vector<float>& inLog, std::vector<float>& outLog)
{
    Buffer in{};
    Buffer out{};
    in(0, 0) = -1.f;
    in(0, 1) = -1.f;
    in(1, 0) = 1.f;
    in(1, 1) = 1.f;
    looper.processBlock(in, out);
    for (size_t i = 0; i < kBlock; ++i)
    {
        inLog.push_back(in(i, 0));
        outLog.push_back(out(i, 0));
    }
}

// Rising-edge onset detector: the first sample of each contiguous region
// where |signal| exceeds `threshold`, skipping ahead past each hit's decay
// tail so a single click's ringing isn't counted twice.
std::vector<size_t> findOnsets(const std::vector<float>& signal, const float threshold, const size_t minGap)
{
    std::vector<size_t> onsets;
    size_t i = 0;
    while (i < signal.size())
    {
        if (std::abs(signal[i]) > threshold)
        {
            onsets.push_back(i);
            i += minGap;
        }
        else
        {
            ++i;
        }
    }
    return onsets;
}

// Predictive check: the click at beat k is expected exactly at k*beatWidth.
// Sums |signal| over a window there -- long enough for one full downbeat wave
// (400 Hz) or two full beat waves (800 Hz) at 48 kHz -- ignoring any sample
// that reaches into the impulse's own amplitude, so an overlapping impulse
// can't be mistaken for (or hide) the click.
float clickWindowEnergy(const std::vector<float>& signal, const size_t pos, const size_t windowLen,
                        const float ignoreAbove)
{
    float energy = 0.f;
    for (size_t i = 0; i < windowLen && pos + i < signal.size(); ++i)
    {
        const float v = std::abs(signal[pos + i]);
        if (v < ignoreAbove)
        {
            energy += v;
        }
    }
    return energy;
}
}


class LooperWrapper
{
  public:
    explicit LooperWrapper(Looper& looper)
        : m_looper(looper)
    {
        looper.setBpm(m_bpm);
        looper.setThreshRec(true);
        looper.setRecThreshold(-48.f);
        looper.setClickVolume(-24.f); // audible, comfortably below the 1.0 test-pulse amplitude
        looper.setFadeMs(.1f);
    }
    size_t m_bpm{240};

    size_t BeatWidth()
    {
        return 48000 * 60 / m_bpm;
    }

    ~LooperWrapper()
    {
        save();
    }

    // Flushes any pending recording under the previous filename, then starts a
    // fresh take: subsequent feed() output is captured under the new filename.
    void Reset(const std::string_view& filename)
    {
        save();
        m_filename = filename;
        m_recordedLeft.clear();
        m_recordedRight.clear();
        m_pos = 0;
        m_fedSamples = 0;
    }

    void save()
    {
        if (m_filename.empty() || m_recordedLeft.empty())
        {
            return;
        }
        AudioUtility::SaveWav::saveStereoAs(m_filename, m_recordedLeft, m_recordedRight, m_looper.sampleRate());
    }

    std::pair<float, float> feed(const std::pair<float, float>& values)
    {
        const std::pair<float, float> result{m_out(m_pos, 0), m_out(m_pos, 1)};
        m_in(m_pos, 0) = values.first;
        m_in(m_pos, 1) = values.second;
        ++m_pos;
        ++m_fedSamples;
        if (m_pos == kBlock)
        {
            m_looper.processBlock(m_in, m_out);
            for (size_t i = 0; i < kBlock; ++i)
            {
                m_recordedLeft.push_back(m_in(i, 0));
                m_recordedRight.push_back(m_out(i, 0));
            }
            m_pos = 0;
        }
        return result;
    }

    void feedImpulse(const float gain, size_t halfWidth)
    {
        for (size_t i = 0; i < halfWidth; ++i)
        {
            feed({gain, gain});
        }
        for (size_t i = 0; i < halfWidth; ++i)
        {
            feed({-gain, -gain});
        }
    }

    void stopRecording()
    {
        m_looper.setRecord(true);
    }

    void startRecording()
    {
        m_looper.setRecord(true);
    }

    Looper& m_looper;
    size_t m_fedSamples{0};
    size_t m_pos{0};
    std::string m_filename;
    std::vector<float> m_recordedLeft;
    std::vector<float> m_recordedRight;
    AbacDsp::AudioBuffer<2, kBlock> m_in;
    AbacDsp::AudioBuffer<2, kBlock> m_out;
};

TEST(ClickAlignmentTest, RecordScenario)
{
    Looper looper(kRealSampleRate);
    LooperWrapper wrp(looper);
    wrp.Reset("/tmp/run.wav");
    wrp.startRecording();

    const size_t emptyRun{wrp.BeatWidth()};
    const size_t recordRun{wrp.BeatWidth() * 4};
    for (size_t i = 0; i < emptyRun; ++i)
    {
        wrp.feed({0, 0});
    }
    wrp.feedImpulse(0.5, 2);
    for (size_t i = 0; i < 6000; ++i)
    {
        wrp.feed({0, 0});
    }
    wrp.feedImpulse(0.5, 2);
    for (size_t i = 0; i < recordRun - 1000 - 6000; ++i)
    {
        wrp.feed({0, 0});
    }
    wrp.stopRecording();
    for (size_t i = 0; i < recordRun * 4; ++i)
    {
        wrp.feed({0, 0});
    }
}

// Same scenario as RecordScenario above, analyzed in-memory instead of via WAV.
TEST(ClickAlignmentTest, RecordScenarioTimingAnalysis)
{
    Looper looper(kRealSampleRate);
    LooperWrapper wrp(looper);
    wrp.startRecording();

    const size_t emptyRun{wrp.BeatWidth()};
    const size_t recordRun{wrp.BeatWidth() * 4};
    for (size_t i = 0; i < emptyRun; ++i)
    {
        wrp.feed({0, 0});
    }
    wrp.feedImpulse(0.5f, 2);
    for (size_t i = 0; i < 6000; ++i)
    {
        wrp.feed({0, 0});
    }
    wrp.feedImpulse(0.5f, 2);
    for (size_t i = 0; i < recordRun - 1000 - 6000; ++i)
    {
        wrp.feed({0, 0});
    }
    wrp.stopRecording();
    for (size_t i = 0; i < recordRun * 4; ++i)
    {
        wrp.feed({0, 0});
    }

    ASSERT_TRUE(looper.isPlaying());
    const size_t loopLength = looper.rawLoopLengthFrames();
    ASSERT_GT(loopLength, 0u);

    // m_recordedRight is the wet output stream (see LooperWrapper::feed).
    const std::vector<float>& output = wrp.m_recordedRight;

    const std::vector<size_t> impulses = findOnsets(output, 0.3f, wrp.BeatWidth() / 2);

    // Impulse 1 sits near loop-frame 0: playback resumes already partway
    // through the loop (the catch-up offset), so it only reaches frame 0
    // again -- and impulse 1 reappears -- a full loop length after the stop
    // tick, not a full loop length after its first (dry, direct) occurrence.
    // Impulse 2 sits mid-loop (loop-frame ~6004), well clear of the catch-up
    // region, so it reappears on every one of the 4 playback repeats.
    const std::vector<size_t> expected = {12000, 18004, 66004, 108000, 114004, 156000, 162004, 204000, 210004};
    ASSERT_EQ(impulses.size(), expected.size()) << "expected 4 occurrences of impulse 1 + 5 of impulse 2";
    for (size_t k = 0; k < impulses.size(); ++k)
    {
        EXPECT_EQ(impulses[k], expected[k]) << "impulse onset #" << k << " is at the wrong position";
    }

    // Predictive: click k is expected exactly at k*BeatWidth
    constexpr size_t kClickWindow = 120; // 400Hz and 800 Hz click
    constexpr float kImpulseAmplitude = 0.49f;
    constexpr float kMinClickEnergy = 0.5f;
    constexpr size_t kNumBeats = 21;
    for (size_t k = 0; k < kNumBeats; ++k)
    {
        const size_t pos = k * wrp.BeatWidth();
        ASSERT_LE(pos + kClickWindow, output.size());
        const float energy = clickWindowEnergy(output, pos, kClickWindow, kImpulseAmplitude);
        EXPECT_GT(energy, kMinClickEnergy) << "no click energy found at beat " << k;
    }
}

// Named loop save/load ("Loops" menu): round-trips through an actual WAV +
// JSON pair on disk, and checks that a BPM disagreement between the WAV's
// embedded metadata and the sidecar JSON is reported rather than silently
// resolved (see LooperImpl::requestLoadLoop()/consumeLoopLoadOutcome()).
namespace
{
constexpr float kLoopFileSampleRate = 48000.f;
constexpr float kLoopFileBpm = 120.f;

class TempLoopsDir
{
  public:
    TempLoopsDir()
        : m_path(std::filesystem::temp_directory_path() / "abacdsp_looper_loopfile_test")
    {
        std::filesystem::create_directories(m_path);
    }

    ~TempLoopsDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] std::string path() const
    {
        return m_path.string();
    }

  private:
    std::filesystem::path m_path;
};

// Immediate (non-threshold-armed) record/stop: simplest path to a known,
// fade-free loop, since the beat-lock corner cases are covered elsewhere.
void recordKnownLoop(Looper& looper)
{
    looper.setBpm(kLoopFileBpm);
    looper.setThreshRec(false);
    looper.setFadeMs(0.f);
    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out); // begins recording

    for (int b = 0; b < 4; ++b)
    {
        for (size_t i = 0; i < kBlock; ++i)
        {
            const float v = 0.01f * static_cast<float>(b * static_cast<int>(kBlock) + static_cast<int>(i) + 1);
            in(i, 0) = v;
            in(i, 1) = -v;
        }
        looper.processBlock(in, out);
    }

    looper.setRecord(true);
    looper.processBlock(in, out); // requests stop; finalizes immediately (not beat-locked)

    Buffer silence{};
    for (int i = 0; i < 4; ++i)
    {
        looper.processBlock(silence, out);
    }
}

void pump(Looper& looper, const int blocks)
{
    Buffer in{};
    Buffer out{};
    for (int i = 0; i < blocks; ++i)
    {
        looper.processBlock(in, out);
    }
}

// Polls the background worker (file I/O isn't RT-safe, see runLoadLoop()) for
// its decode outcome, pumping the audio thread so completion is observed.
Looper::LoopLoadOutcome waitForLoopLoadOutcome(Looper& looper)
{
    for (int i = 0; i < 2000; ++i)
    {
        pump(looper, 1);
        if (auto outcome = looper.consumeLoopLoadOutcome(); outcome.attempted)
        {
            return outcome;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return {};
}

void waitUntilLoopSaveDone(Looper& looper)
{
    for (int i = 0; i < 2000 && looper.isLoopSavePending(); ++i)
    {
        pump(looper, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void waitUntilLoopLoadInstalled(Looper& looper)
{
    for (int i = 0; i < 2000 && looper.isLoopLoadPending(); ++i)
    {
        pump(looper, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void expectSameLoopContent(Looper& a, Looper& b, const size_t length)
{
    for (size_t f = 0; f < length; ++f)
    {
        EXPECT_NEAR(a.rawLoopSample(f, 0), b.rawLoopSample(f, 0), 1e-3f) << "frame " << f;
        EXPECT_NEAR(a.rawLoopSample(f, 1), b.rawLoopSample(f, 1), 1e-3f) << "frame " << f;
    }
}
}

TEST(LooperLoopFile, SaveThenLoadRoundTripsAudioAndBpm)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    ASSERT_TRUE(writer.isPlaying());
    const size_t originalLength = writer.rawLoopLengthFrames();
    ASSERT_GT(originalLength, 0u);

    writer.requestSaveLoopAs("myloop");
    waitUntilLoopSaveDone(writer);
    EXPECT_EQ(writer.consumeLastSavedLoopName(), "myloop");

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("myloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    EXPECT_TRUE(outcome.attempted);
    EXPECT_TRUE(outcome.success);
    EXPECT_FALSE(outcome.hasConflict);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    ASSERT_EQ(reader.rawLoopLengthFrames(), originalLength);
    expectSameLoopContent(writer, reader, originalLength);

    const auto expectedSamplesPerBeat = static_cast<size_t>(kLoopFileSampleRate * 60.f / kLoopFileBpm);
    EXPECT_EQ(reader.getSamplesPerBar(), expectedSamplesPerBeat * reader.getBarBeats());
}

TEST(LooperLoopFile, LoadReportsConflictInsteadOfPickingSilently)
{
    const TempLoopsDir dir;
    constexpr float kJsonBpm = 140.f;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    const size_t originalLength = writer.rawLoopLengthFrames();

    writer.requestSaveLoopAs("conflicted");
    waitUntilLoopSaveDone(writer);

    // Hand-edit the sidecar JSON so it disagrees with the WAV's embedded BPM.
    const auto jsonPath = std::filesystem::path(dir.path()) / "conflicted.json";
    {
        std::ofstream out(jsonPath);
        out << "{\"version\":1,\"bpm\":" << kJsonBpm << "}";
    }

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("conflicted");
    const auto outcome = waitForLoopLoadOutcome(reader);

    ASSERT_TRUE(outcome.attempted);
    ASSERT_TRUE(outcome.success);
    ASSERT_TRUE(outcome.hasConflict);
    EXPECT_NEAR(outcome.wavBpm, kLoopFileBpm, 1e-3f);
    EXPECT_NEAR(outcome.jsonBpm, kJsonBpm, 1e-3f);

    // Not installed yet: still pending resolution.
    pump(reader, 4);
    EXPECT_TRUE(reader.isLoopLoadPending());
    EXPECT_FALSE(reader.isPlaying());

    reader.resolveLoopLoadBpm(kJsonBpm);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    ASSERT_EQ(reader.rawLoopLengthFrames(), originalLength);
    expectSameLoopContent(writer, reader, originalLength);

    const auto expectedSamplesPerBeat = static_cast<size_t>(kLoopFileSampleRate * 60.f / kJsonBpm);
    EXPECT_EQ(reader.getSamplesPerBar(), expectedSamplesPerBeat * reader.getBarBeats());
}
