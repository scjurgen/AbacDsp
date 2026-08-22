#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "impl/LooperImpl.h"

// 3x3 matrix: start timing x stop timing (before/on/after the bar tick).
// Verifies loopLength and the doublet's loop-frame position stay fixed.
namespace
{
constexpr size_t kBlock = 16;
// Chosen so every derived quantity is itself a multiple of kBlock.
constexpr float kSampleRate = 5120.f;
constexpr float kBpm = 120.f;
constexpr size_t kSamplesPerBeat = 2560;                          // kSampleRate*60/kBpm
constexpr size_t kBeatsPerBar = 4;                                // matches LooperImpl::kBeatsPerBar
constexpr size_t kSamplesPerBar = kSamplesPerBeat * kBeatsPerBar; // 10240
constexpr size_t kBarsPerTake = 1;
constexpr size_t kExpectedLoopLength = kSamplesPerBar * kBarsPerTake; // 10240

// Block-aligned, comfortably inside half a bar (5120), on one side of the tick.
constexpr size_t kStartOffset = 144; // 9*16
constexpr size_t kStopOffset = 96;   // 6*16

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
    // No fade: an early start's doublet can sit right at the tail fold's edge.
    looper.setFadeMs(0.f);

    looper.setRecord(true); // arm
    runSilence(looper, kBlock);

    constexpr size_t sTarget = kSamplesPerBar; // a bar tick, one bar in
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

    constexpr size_t eTarget = sTarget + kExpectedLoopLength; // the next bar tick, one bar later
    const long stopShift = (stopTiming == Timing::Before)  ? -static_cast<long>(kStopOffset)
                           : (stopTiming == Timing::After) ? static_cast<long>(kStopOffset)
                                                           : 0;
    const auto stopRequestTarget = static_cast<size_t>(static_cast<long>(eTarget) + stopShift);

    const size_t pos = trigTarget + kBlock;
    ASSERT_GT(stopRequestTarget, pos) << "test offsets left no room for the main take";
    runSilence(looper, stopRequestTarget - pos);

    looper.setRecord(true); // request stop
    runSilence(looper, kBlock);

    // Poll until finalized: no fixed margin to precompute, bar-lock always locks.
    for (int guard = 0; guard < 1000 && !looper.isPlaying(); ++guard)
    {
        runSilence(looper, kBlock);
    }
    ASSERT_TRUE(looper.isPlaying()) << "bar-locked take should have finalized by now";

    // Invariant under test: loop length depends only on the bar grid (one
    // bar from sTarget to eTarget), never on stop-request timing.
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
constexpr size_t kRealSamplesPerBar = kRealSamplesPerBeat * kBeatsPerBar;
constexpr size_t kRealStartOffset = 800; // 50*16, comfortably inside half a bar, block-aligned
constexpr size_t kRealStopOffset = 1600; // 100*16, comfortably inside half a bar, block-aligned

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
    const size_t bars = std::get<0>(info.param);
    const Timing startTiming = std::get<1>(info.param);
    const Timing stopTiming = std::get<2>(info.param);
    return std::string("Bars") + std::to_string(bars) + "_Start" + timingName(startTiming) + "_Stop" +
           timingName(stopTiming);
}
}

class OutputTimingTest : public ::testing::TestWithParam<std::tuple<size_t, Timing, Timing>>
{
};

TEST_P(OutputTimingTest, SignalReappearsExactlyOneLoopLengthApartEveryRepeat)
{
    const auto [bars, startTiming, stopTiming] = GetParam();
    const size_t expectedLoopLength = kRealSamplesPerBar * bars;

    Looper looper(kRealSampleRate);
    looper.setBpm(kRealBpm);
    looper.setThreshRec(true);
    looper.setRecThreshold(-24.f);
    // No fade: an early start's doublet can sit right at the tail fold's edge.
    looper.setFadeMs(0.f);

    looper.setRecord(true); // arm
    runSilence(looper, kBlock);

    constexpr size_t sTarget = kRealSamplesPerBar; // a bar tick, one bar in
    const long startShift = (startTiming == Timing::Before)  ? -static_cast<long>(kRealStartOffset)
                            : (startTiming == Timing::After) ? static_cast<long>(kRealStartOffset)
                                                             : 0;
    const auto trigTarget = static_cast<size_t>(static_cast<long>(sTarget) + startShift);

    ASSERT_GT(trigTarget, kBlock);
    runSilence(looper, trigTarget - kBlock);
    runDoubletBlock(looper); // the "signal": crosses threshold, recording starts immediately
    ASSERT_TRUE(looper.isRecording());

    // Stop-request timing never changes what gets captured (BeatLockMatrixTest).
    const size_t eTarget = sTarget + expectedLoopLength;
    const long stopShift = (stopTiming == Timing::Before)  ? -static_cast<long>(kRealStopOffset)
                           : (stopTiming == Timing::After) ? static_cast<long>(kRealStopOffset)
                                                           : 0;
    const auto stopRequestTarget = static_cast<size_t>(static_cast<long>(eTarget) + stopShift);

    const size_t pos = trigTarget + kBlock;
    ASSERT_GT(stopRequestTarget, pos) << "not enough bars for the offset to fit";
    runSilence(looper, stopRequestTarget - pos);

    looper.setRecord(true); // request stop
    runSilence(looper, kBlock);

    // Zero-slop wait: capture starts exactly on the first Playing-state block,
    // whatever block that turns out to be for this stop-timing sub-case.
    runUntilPlaying(looper);
    ASSERT_TRUE(looper.isPlaying());
    ASSERT_EQ(looper.rawLoopLengthFrames(), expectedLoopLength) << "bars=" << bars;

    // Capture four full loop repeats of pure silence at the input: whatever
    // comes out is entirely the looper's own played-back signal.
    std::vector<float> stream;
    captureOutput(looper, expectedLoopLength * 4, stream);

    const std::vector<size_t> hits = findAllDoublets(stream);
    ASSERT_EQ(hits.size(), 4u) << "expected exactly one doublet per loop repeat, bars=" << bars
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

    // On-time/early commits one block past the tick; a late request commits
    // immediately, already kRealStopOffset past it.
    const size_t catchUp = (stopTiming == Timing::After) ? (kRealStopOffset + kBlock) : kBlock;
    const size_t expectedFirst = (loopFrame + expectedLoopLength - catchUp) % expectedLoopLength;
    EXPECT_EQ(hits[0], expectedFirst) << "bars=" << bars << " start=" << timingName(startTiming)
                                      << " stop=" << timingName(stopTiming);

    // The real point of this test: every repeat lands exactly one loop length
    // later than the previous one -- with zero drift across repeats.
    for (size_t k = 1; k < hits.size(); ++k)
    {
        EXPECT_EQ(hits[k] - hits[k - 1], expectedLoopLength)
            << "repeat " << k << " drifted, bars=" << bars << " start=" << timingName(startTiming)
            << " stop=" << timingName(stopTiming);
    }
}

INSTANTIATE_TEST_SUITE_P(BarsXStartXStop, OutputTimingTest,
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

    const size_t emptyRun{wrp.BeatWidth() * 4}; // a bar tick, so the trigger lands exactly on it
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

    const size_t emptyRun{wrp.BeatWidth() * 4}; // a bar tick, so the trigger lands exactly on it
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
    ASSERT_EQ(loopLength, recordRun);

    // m_recordedRight is the wet output stream (see LooperWrapper::feed).
    const std::vector<float>& output = wrp.m_recordedRight;

    const std::vector<size_t> impulses = findOnsets(output, 0.3f, wrp.BeatWidth() / 2);

    // The looper always passes dry input through, so onsets 0 and 1 are the
    // direct (during-recording) hits, not playback repeats.
    ASSERT_GE(impulses.size(), 2u);
    EXPECT_EQ(impulses[0], emptyRun);
    EXPECT_EQ(impulses[1], emptyRun + 6004);

    // From here on, every other onset is exactly loopLength apart.
    const std::vector<size_t> playback(impulses.begin() + 2, impulses.end());
    ASSERT_GE(playback.size(), 4u) << "not enough playback onsets to verify repeat spacing";
    for (size_t k = 0; k + 2 < playback.size(); ++k)
    {
        EXPECT_EQ(playback[k + 2] - playback[k], loopLength)
            << "onset #" << k << " and #" << (k + 2) << " should be exactly one loop apart";
    }
    for (size_t k = 0; k + 1 < playback.size(); ++k)
    {
        const size_t gap = playback[k + 1] - playback[k];
        EXPECT_TRUE(gap == 6004 || gap == loopLength - 6004)
            << "onset #" << k << " -> #" << (k + 1) << " gap " << gap << " doesn't match either impulse separation";
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

void expectSameOverdubContent(Looper& a, Looper& b, const size_t length)
{
    for (size_t f = 0; f < length; ++f)
    {
        EXPECT_NEAR(a.rawOverdubSample(f, 0), b.rawOverdubSample(f, 0), 1e-3f) << "frame " << f;
        EXPECT_NEAR(a.rawOverdubSample(f, 1), b.rawOverdubSample(f, 1), 1e-3f) << "frame " << f;
    }
}

// Requires the looper to already be Playing. Overdub decay defaults to 1
// and the layer starts at zero, so one pass leaves the overdub buffer at
// exactly `value` for every frame -- a known, easily verified constant.
void addKnownOverdub(Looper& looper, const float value)
{
    Buffer silence{};
    Buffer out{};
    looper.setOverdub(true);
    looper.processBlock(silence, out); // begins overdub

    Buffer in{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = value;
        in(i, 1) = -value;
    }
    looper.processBlock(in, out);

    looper.setOverdub(true);
    looper.processBlock(silence, out); // ends overdub
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

    // The sidecar JSON should spell out bars/beats, not just bpm, so a human
    // reading the file doesn't have to compute them from bpm + file length.
    std::ifstream jsonIn(std::filesystem::path(dir.path()) / "myloop.json");
    ASSERT_TRUE(jsonIn);
    nlohmann::json savedJson;
    jsonIn >> savedJson;
    const float expectedBeats = static_cast<float>(originalLength) / (kLoopFileSampleRate * 60.f / kLoopFileBpm);
    const float expectedBars = expectedBeats / static_cast<float>(kBeatsPerBar);
    EXPECT_NEAR(savedJson.at("beats").get<float>(), expectedBeats, 1e-2f);
    EXPECT_NEAR(savedJson.at("bars").get<float>(), expectedBars, 1e-2f);

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

// Regression: loading only ever installed into the active part, leaving any
// other part's stale content (and the selection) untouched - a loaded loop
// is a fresh single-part session, so every other part must reset to empty.
TEST(LooperLoopFile, LoadingALoopResetsAllOtherParts)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    writer.requestSaveLoopAs("myloop");
    waitUntilLoopSaveDone(writer);

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.setThreshRec(false);
    reader.setFadeMs(0.f);

    // Give part B stale content before loading (active part 0 is empty, so
    // this commits immediately, no crossfade needed).
    reader.setSelectedPart(1);
    Buffer out{};
    reader.processBlock(Buffer{}, out);
    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 9.f;
        inB(i, 1) = 9.f;
    }
    reader.setRecord(true);
    reader.processBlock(inB, out);
    reader.setRecord(true);
    reader.processBlock(inB, out);
    ASSERT_NE(reader.partStatusLabel(1), "Part B: free");

    reader.requestLoadLoop("myloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    EXPECT_EQ(reader.currentSelectedPartIndex(), 0);
    EXPECT_EQ(reader.partStatusLabel(1), "Part B: free");
    EXPECT_EQ(reader.partStatusLabel(2), "Part C: free");
    EXPECT_EQ(reader.partStatusLabel(3), "Part D: free");
    EXPECT_TRUE(reader.isPlaying());
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

TEST(LooperLoopFile, SaveThenLoadRoundTripsFrozenTracksAndPattern)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    ASSERT_TRUE(writer.isPlaying());

    writer.setFreeze(true);
    for (int guard = 0; guard < 2000 && (guard == 0 || writer.isFreezePending()); ++guard)
    {
        pump(writer, 1);
    }
    ASSERT_FALSE(writer.isFreezePending());
    ASSERT_GT(writer.getFrozenTrackCount(), 0u);

    const size_t originalTrackCount = writer.getFrozenTrackCount();
    const size_t originalSliceCount = writer.getFrozenSliceCount();
    const auto originalBoundaries = writer.getSequencerSliceBoundaries();
    ASSERT_FALSE(originalBoundaries.empty());

    writer.requestSaveLoopAs("frozenloop");
    waitUntilLoopSaveDone(writer);

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("frozenloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    EXPECT_EQ(reader.getFrozenTrackCount(), originalTrackCount);
    EXPECT_EQ(reader.getFrozenSliceCount(), originalSliceCount);
    const auto reloadedBoundaries = reader.getSequencerSliceBoundaries();
    ASSERT_EQ(reloadedBoundaries.size(), originalBoundaries.size());
    for (size_t i = 0; i < originalBoundaries.size(); ++i)
    {
        EXPECT_NEAR(reloadedBoundaries[i], originalBoundaries[i], 1e-4f);
    }
}

// Regression test, same root cause as SaveThenLoadRoundTripsOverdubLayer-
// InSubfolder: a frozen track's JSON file reference lost its subfolder too.
TEST(LooperLoopFile, SaveThenLoadRoundTripsFrozenTracksInSubfolder)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    ASSERT_TRUE(writer.isPlaying());

    writer.setFreeze(true);
    for (int guard = 0; guard < 2000 && (guard == 0 || writer.isFreezePending()); ++guard)
    {
        pump(writer, 1);
    }
    ASSERT_FALSE(writer.isFreezePending());
    ASSERT_GT(writer.getFrozenTrackCount(), 0u);

    const size_t originalTrackCount = writer.getFrozenTrackCount();
    const size_t originalSliceCount = writer.getFrozenSliceCount();
    const auto originalBoundaries = writer.getSequencerSliceBoundaries();
    ASSERT_FALSE(originalBoundaries.empty());

    writer.requestSaveLoopAs("MyFolder/frozenloop");
    waitUntilLoopSaveDone(writer);

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("MyFolder/frozenloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    EXPECT_EQ(reader.getFrozenTrackCount(), originalTrackCount);
    EXPECT_EQ(reader.getFrozenSliceCount(), originalSliceCount);
    const auto reloadedBoundaries = reader.getSequencerSliceBoundaries();
    ASSERT_EQ(reloadedBoundaries.size(), originalBoundaries.size());
    for (size_t i = 0; i < originalBoundaries.size(); ++i)
    {
        EXPECT_NEAR(reloadedBoundaries[i], originalBoundaries[i], 1e-4f);
    }
}

// Reuses kSampleRate/kBpm/kSamplesPerBeat (5120 Hz, 120 BPM) from
// BeatLockMatrixTest above, so a mid-take meter change lands on clean block
// boundaries; mirrors TimeSignatureChange.PlaybackReplaysRecordedMeterTimeline-
// AcrossLoopRepeats but round-trips the take through a .mid sidecar save/load
// instead of checking the in-memory instance's own playback.
TEST(LooperLoopFile, SaveThenLoadRoundTripsMeterTimeline)
{
    const TempLoopsDir dir;

    Looper writer(kSampleRate);
    writer.setLoopsDirectory(dir.path());
    writer.setBpm(kBpm);
    writer.setThreshRec(false);
    writer.setAutoStop(true);
    writer.setRecordBars(2);
    writer.setTimeSignature(2); // 4/4

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }

    writer.setRecord(true);
    writer.processBlock(in, out);
    ASSERT_TRUE(writer.isRecording());
    ASSERT_EQ(writer.getBarBeats(), 4);

    writer.setTimeSignature(1); // 3/4, queued to apply at bar 2
    constexpr size_t kBlocksPerBar4_4 = kSamplesPerBar / kBlock;
    // One call already consumed above; kBlocksPerBar4_4-1 more stay within bar 1.
    for (size_t call = 1; call < kBlocksPerBar4_4 - 1; ++call)
    {
        writer.processBlock(in, out);
    }
    writer.processBlock(in, out); // crosses into bar 2
    ASSERT_EQ(writer.getBarBeats(), 3);

    constexpr size_t kSamplesPerBar3_4 = kSamplesPerBeat * 3;
    constexpr size_t kBlocksPerBar3_4 = kSamplesPerBar3_4 / kBlock;
    size_t safety = 0;
    while (writer.isRecording())
    {
        writer.processBlock(in, out);
        ASSERT_LE(++safety, kBlocksPerBar3_4 + 10) << "recording never auto-stopped";
    }
    ASSERT_TRUE(writer.isPlaying());
    const size_t originalLength = writer.rawLoopLengthFrames();

    writer.requestSaveLoopAs("meterloop");
    waitUntilLoopSaveDone(writer);

    Looper reader(kSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("meterloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    ASSERT_EQ(reader.rawLoopLengthFrames(), originalLength);

    // Playback restarts at bar 0's own recorded meter, reconstructed from the
    // .mid sidecar rather than the writer's own live in-memory timeline.
    EXPECT_EQ(reader.getBarBeats(), 4);

    for (size_t call = 0; call < kBlocksPerBar4_4; ++call)
    {
        reader.processBlock(in, out);
    }
    EXPECT_EQ(reader.getBarBeats(), 3) << "bar 2 of playback should replay the recorded meter change";

    for (size_t call = 0; call < kBlocksPerBar3_4; ++call)
    {
        reader.processBlock(in, out);
    }
    EXPECT_EQ(reader.getBarBeats(), 4) << "loop repeat should wrap back to bar 0's meter";
}

TEST(LooperLoopFile, SaveThenLoadRoundTripsOverdubLayer)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    ASSERT_TRUE(writer.isPlaying());
    const size_t originalLength = writer.rawLoopLengthFrames();

    addKnownOverdub(writer, 0.2f);
    ASSERT_TRUE(writer.hasOverdub());

    writer.requestSaveLoopAs("overdubloop");
    waitUntilLoopSaveDone(writer);

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("overdubloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    ASSERT_EQ(reader.rawLoopLengthFrames(), originalLength);
    ASSERT_TRUE(reader.hasOverdub());
    expectSameLoopContent(writer, reader, originalLength);
    expectSameOverdubContent(writer, reader, originalLength);
}

// Regression test: a subfoldered loop name's overdub file reference lost
// its subfolder on save (LoopStorageService::runSaveLoopAs()'s "overdub"
// field), so load rejoined it against the flat loops directory.
TEST(LooperLoopFile, SaveThenLoadRoundTripsOverdubLayerInSubfolder)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    recordKnownLoop(writer);
    ASSERT_TRUE(writer.isPlaying());
    const size_t originalLength = writer.rawLoopLengthFrames();

    addKnownOverdub(writer, 0.2f);
    ASSERT_TRUE(writer.hasOverdub());

    writer.requestSaveLoopAs("MyFolder/overdubloop");
    waitUntilLoopSaveDone(writer);

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("MyFolder/overdubloop");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    ASSERT_EQ(reader.rawLoopLengthFrames(), originalLength);
    ASSERT_TRUE(reader.hasOverdub());
    expectSameLoopContent(writer, reader, originalLength);
    expectSameOverdubContent(writer, reader, originalLength);
}

TEST(LooperLoopFile, CaptureExtraStateThenRestoreRoundTripsOverdubLayer)
{
    Looper writer(kLoopFileSampleRate);
    recordKnownLoop(writer);
    ASSERT_TRUE(writer.isPlaying());
    const size_t originalLength = writer.rawLoopLengthFrames();

    addKnownOverdub(writer, 0.15f);
    ASSERT_TRUE(writer.hasOverdub());

    const auto blob = writer.captureExtraState();
    ASSERT_FALSE(blob.empty());

    Looper reader(kLoopFileSampleRate);
    reader.restoreExtraState(blob);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    ASSERT_EQ(reader.rawLoopLengthFrames(), originalLength);
    ASSERT_TRUE(reader.hasOverdub());
    expectSameLoopContent(writer, reader, originalLength);
    expectSameOverdubContent(writer, reader, originalLength);
}

// Regression: LoopStorageService used to save/load only the active part
// and one bpm shared by all of them; each part's own audio and own bpm
// now round-trip independently (Part B via <name>_partB.wav/.json).
TEST(LooperLoopFile, SaveThenLoadRoundTripsAllPopulatedPartsAndTheirOwnBpm)
{
    const TempLoopsDir dir;

    Looper writer(kLoopFileSampleRate);
    writer.setLoopsDirectory(dir.path());
    writer.setThreshRec(false);
    writer.setFadeMs(0.f);
    writer.setFreeRecord(true);
    writer.setBpm(90.f);
    Buffer out{};

    Buffer inA{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 0.3f;
        inA(i, 1) = 0.3f;
    }
    writer.setRecord(true);
    writer.processBlock(inA, out);
    for (int b = 0; b < 3; ++b)
    {
        writer.processBlock(inA, out);
    }
    writer.setRecord(true);
    writer.processBlock(inA, out);
    ASSERT_TRUE(writer.isPlaying());
    const size_t lenA = writer.rawLoopLengthFrames();
    writer.setPlay(true); // -> Stopped, so the part-B redirect below is immediate

    writer.setSelectedPart(1);
    writer.processBlock(Buffer{}, out);
    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 0.6f;
        inB(i, 1) = 0.6f;
    }
    writer.setRecord(true);
    writer.processBlock(inB, out);
    ASSERT_TRUE(writer.isRecording());
    writer.setBpm(140.f);
    for (int b = 0; b < 3; ++b)
    {
        writer.processBlock(inB, out);
    }
    writer.setRecord(true);
    writer.processBlock(inB, out);
    ASSERT_TRUE(writer.isPlaying());

    writer.requestSaveLoopAs("multipart");
    waitUntilLoopSaveDone(writer);

    Looper reader(kLoopFileSampleRate);
    reader.setLoopsDirectory(dir.path());
    reader.requestLoadLoop("multipart");
    const auto outcome = waitForLoopLoadOutcome(reader);
    ASSERT_TRUE(outcome.success);
    waitUntilLoopLoadInstalled(reader);

    ASSERT_TRUE(reader.isPlaying());
    EXPECT_EQ(reader.currentSelectedPartIndex(), 0);
    EXPECT_EQ(reader.rawLoopLengthFrames(), lenA);
    EXPECT_NEAR(reader.rawLoopSample(kBlock / 2, 0), 0.3f, 1e-3f);
    EXPECT_NE(reader.partStatusLabel(1), "Part B: free");
    EXPECT_NEAR(reader.currentAppliedBpm(), 90.f, 1e-3f);

    reader.setSelectedPart(1);
    for (int i = 0; i < 200; ++i)
    {
        reader.processBlock(Buffer{}, out);
    }
    EXPECT_TRUE(reader.isPlaying());
    EXPECT_NEAR(reader.rawLoopSample(kBlock / 2, 0), 0.6f, 1e-3f);
    EXPECT_NEAR(reader.currentAppliedBpm(), 140.f, 1e-3f);
}

// Reuses kSampleRate/kBpm/kSamplesPerBar (5120 Hz, 120 BPM, 10240
// samples/bar) and the Looper/Buffer aliases from BeatLockMatrixTest above.
TEST(RecordingModes, CountInPlaysClickThenStartsRecordingAfterNBars)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setCountInBars(2);

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 1.f; // fed throughout; must never be captured during count-in
        in(i, 1) = 1.f;
    }

    looper.setRecord(true);
    looper.processBlock(in, out); // consumes the pulse; begins count-in
    ASSERT_TRUE(looper.isCountingIn());
    EXPECT_FALSE(looper.isRecording());

    // Recording only ever starts in the same call that ends count-in, so an
    // exact match below also proves it didn't start early.
    constexpr size_t kExpectedCountInFrames = 2 * kSamplesPerBar;
    size_t elapsed = kBlock;
    while (looper.isCountingIn())
    {
        looper.processBlock(in, out);
        elapsed += kBlock;
        ASSERT_LE(elapsed, kExpectedCountInFrames + kBlock) << "count-in ran longer than expected";
    }
    EXPECT_EQ(elapsed, kExpectedCountInFrames + kBlock)
        << "count-in should complete on the block that crosses exactly 2 bars";
    EXPECT_TRUE(looper.isRecording());
}

TEST(RecordingModes, PresetBarsAutoStopWorksAfterCountIn)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setCountInBars(2);
    looper.setAutoStop(true);
    looper.setRecordBars(2);

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }

    looper.setRecord(true);
    looper.processBlock(in, out); // begins count-in
    ASSERT_TRUE(looper.isCountingIn());

    while (looper.isCountingIn())
    {
        looper.processBlock(in, out);
    }
    ASSERT_TRUE(looper.isRecording());

    constexpr size_t kExpectedLength = 2 * kSamplesPerBar;
    size_t elapsed = 0;
    while (looper.isRecording())
    {
        looper.processBlock(in, out);
        elapsed += kBlock;
        ASSERT_LE(elapsed, kExpectedLength + 4 * kBlock) << "recording never auto-stopped";
    }
    EXPECT_TRUE(looper.isPlaying());
    EXPECT_EQ(looper.rawLoopLengthFrames(), kExpectedLength);
}

// Sweeps realistic BPM values at a real sample rate/block size (48kHz, 512),
// exactly reproducing a reported scenario: 2 bars count-in, Auto Stop with
// Record Bars=4, no threshold. The 5120Hz/kBlock=16 fixture above divides
// bar length by block size exactly (640 blocks/bar); this checks whether a
// less convenient real-world block/bar alignment exposes a miscount that the
// clean fixture masks.
TEST(RecordingModes, PresetBarsAutoStopAfterCountInStaysExactAcrossRealisticBpms)
{
    using RealLooper = LooperImpl<512>;
    using RealBuffer = AbacDsp::AudioBuffer<2, 512>;
    constexpr float kRealSampleRate = 48000.f;

    for (const float bpm : {90.f, 100.f, 110.f, 120.f, 128.f, 130.f, 135.f, 140.f, 150.f, 160.f, 174.f})
    {
        RealLooper looper(kRealSampleRate);
        looper.setBpm(bpm);
        looper.setThreshRec(false);
        looper.setCountInBars(2);
        looper.setAutoStop(true);
        looper.setRecordBars(4);

        RealBuffer in{};
        RealBuffer out{};
        for (size_t i = 0; i < 512; ++i)
        {
            in(i, 0) = 0.3f;
            in(i, 1) = -0.3f;
        }

        looper.setRecord(true);
        looper.processBlock(in, out); // begins count-in
        ASSERT_TRUE(looper.isCountingIn()) << "bpm=" << bpm;

        int guard = 0;
        while (looper.isCountingIn())
        {
            looper.processBlock(in, out);
            ASSERT_LE(++guard, 10000) << "count-in never finished, bpm=" << bpm;
        }
        ASSERT_TRUE(looper.isRecording()) << "bpm=" << bpm;

        guard = 0;
        while (looper.isRecording())
        {
            looper.processBlock(in, out);
            ASSERT_LE(++guard, 10000) << "recording never auto-stopped, bpm=" << bpm;
        }
        ASSERT_TRUE(looper.isPlaying()) << "bpm=" << bpm;
        EXPECT_EQ(looper.getOuterRingBars(), 4) << "bpm=" << bpm << " (ring bar count after finalize)";

        const size_t spb = looper.getSamplesPerBar();
        const size_t expectedLength = 4 * spb;
        EXPECT_NEAR(static_cast<double>(looper.rawLoopLengthFrames()), static_cast<double>(expectedLength),
                    static_cast<double>(spb) * 0.5)
            << "bpm=" << bpm;
    }
}

TEST(RecordingModes, CountInCanBeCancelledByPressingRecordAgain)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setCountInBars(2);

    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isCountingIn());

    looper.setRecord(true);
    looper.processBlock(in, out);
    EXPECT_FALSE(looper.isCountingIn());
    EXPECT_FALSE(looper.isRecording());
}

TEST(RecordingModes, PresetBarsAutoStopWorksWithThresholdArmedStart)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(true);
    looper.setRecThreshold(-24.f);
    looper.setAutoStop(true);
    looper.setRecordBars(2);
    looper.setFadeMs(0.f);

    looper.setRecord(true); // arm
    runSilence(looper, kBlock);

    // Trigger the threshold crossing partway into a bar (not block-0-aligned),
    // mimicking a realistic performer start rather than an exact tick.
    runSilence(looper, 3 * kBlock);
    runDoubletBlock(looper);
    ASSERT_TRUE(looper.isRecording());

    constexpr size_t kExpectedLength = 2 * kSamplesPerBar;
    Buffer in{};
    Buffer out{};
    size_t elapsed = 0;
    while (looper.isRecording())
    {
        looper.processBlock(in, out);
        elapsed += kBlock;
        ASSERT_LE(elapsed, kExpectedLength + 4 * kBlock) << "recording never auto-stopped";
    }
    EXPECT_TRUE(looper.isPlaying());
    EXPECT_EQ(looper.rawLoopLengthFrames(), kExpectedLength);
}

TEST(RecordingModes, PresetBarsAutoStopsAfterExactlyNBars)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setAutoStop(true);
    looper.setRecordBars(2);

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }

    looper.setRecord(true);
    looper.processBlock(in, out); // immediate button start (bar-locked, no thresh/count-in)
    ASSERT_TRUE(looper.isRecording());

    constexpr size_t kExpectedLength = 2 * kSamplesPerBar;
    size_t elapsed = kBlock;
    while (looper.isRecording())
    {
        looper.processBlock(in, out);
        elapsed += kBlock;
        ASSERT_LE(elapsed, kExpectedLength + 4 * kBlock) << "recording never auto-stopped";
    }
    EXPECT_TRUE(looper.isPlaying());
    EXPECT_EQ(looper.rawLoopLengthFrames(), kExpectedLength);
}

TEST(RecordingModes, UndoRestoresPreviousLoopAfterReRecord)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true); // sidesteps bar-locked stop timing, not the point of this test
    looper.setFadeMs(0.f);      // isolates frame 0 from the boundary fade-in

    Buffer inA{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 1.f;
        inA(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(inA, out); // starts take 1
    ASSERT_TRUE(looper.isRecording());
    looper.processBlock(inA, out);
    looper.setRecord(true);
    looper.processBlock(inA, out); // stops take 1 (free record stops immediately)
    ASSERT_FALSE(looper.isRecording());
    const size_t firstLength = looper.rawLoopLengthFrames();
    ASSERT_GT(firstLength, 0u);
    ASSERT_NEAR(looper.rawLoopSample(0, 0), 1.f, 1e-3f);

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 5.f;
        inB(i, 1) = 5.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // starts take 2, over-recording take 1
    ASSERT_TRUE(looper.isRecording());
    looper.setRecord(true);
    looper.processBlock(inB, out); // stops take 2
    ASSERT_FALSE(looper.isRecording());
    ASSERT_NEAR(looper.rawLoopSample(0, 0), 5.f, 1e-3f);

    looper.setUndo(true);
    looper.processBlock(inB, out); // consumes the undo pulse
    EXPECT_EQ(looper.rawLoopLengthFrames(), firstLength);
    EXPECT_NEAR(looper.rawLoopSample(0, 0), 1.f, 1e-3f) << "undo after a record should restore the prior take";
}

TEST(RecordingModes, UndoWhileOverdubbingStillUndoesTheOverdubNotTheRecord)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f); // isolates frame 0 from the boundary fade-in

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 1.f;
        in(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(in, out); // start
    looper.setRecord(true);
    looper.processBlock(in, out); // stop -> Playing
    ASSERT_FALSE(looper.isRecording());
    const size_t loopLength = looper.rawLoopLengthFrames();

    looper.setOverdub(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isOverdubbing());

    looper.setUndo(true);
    looper.processBlock(in, out); // undo while overdubbing: drops the overdub layer
    EXPECT_FALSE(looper.isOverdubbing());
    EXPECT_EQ(looper.rawLoopLengthFrames(), loopLength) << "the base take must be untouched";
    EXPECT_NEAR(looper.rawLoopSample(0, 0), 1.f, 1e-3f);
}

TEST(TimeSignatureChange, ChangeDuringRecordingAppliesOnlyAtNextBarBoundary)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setTimeSignature(2); // 4/4

    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());
    ASSERT_EQ(looper.getBarBeats(), 4);

    looper.setTimeSignature(0); // request 2/4; must not disturb the bar in progress
    constexpr size_t kBlocksPerBar = kSamplesPerBar / kBlock;
    // One call already consumed above; kBlocksPerBar-1 more stay within bar 1.
    for (size_t call = 1; call < kBlocksPerBar - 1; ++call)
    {
        looper.processBlock(in, out);
        ASSERT_EQ(looper.getBarBeats(), 4) << "meter changed before the bar boundary, at call " << call;
    }
    looper.processBlock(in, out); // this call crosses into bar 2
    EXPECT_EQ(looper.getBarBeats(), 2);
}

TEST(TimeSignatureChange, IgnoredWhilePlayingBack)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setTimeSignature(2); // 4/4

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }
    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());
    looper.setRecord(true); // stop: bar-locked take auto-transitions into playback
    while (looper.isRecording())
    {
        looper.processBlock(in, out);
    }
    ASSERT_TRUE(looper.isPlaying());
    ASSERT_EQ(looper.getBarBeats(), 4);

    looper.setTimeSignature(0); // 2/4: must have no effect while just playing back
    for (size_t call = 0; call < kBlock; ++call)
    {
        looper.processBlock(in, out);
        ASSERT_EQ(looper.getBarBeats(), 4) << "live control affected playback meter, at call " << call;
    }
}

TEST(TimeSignatureChange, PlaybackReplaysRecordedMeterTimelineAcrossLoopRepeats)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setAutoStop(true);
    looper.setRecordBars(2);
    looper.setTimeSignature(2); // 4/4

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }

    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());
    ASSERT_EQ(looper.getBarBeats(), 4);

    looper.setTimeSignature(1); // 3/4, queued to apply at bar 2
    constexpr size_t kBlocksPerBar4_4 = kSamplesPerBar / kBlock;
    // One call already consumed above; kBlocksPerBar4_4-1 more stay within bar 1.
    for (size_t call = 1; call < kBlocksPerBar4_4 - 1; ++call)
    {
        looper.processBlock(in, out);
    }
    looper.processBlock(in, out); // crosses into bar 2
    ASSERT_EQ(looper.getBarBeats(), 3);

    constexpr size_t kSamplesPerBar3_4 = kSamplesPerBeat * 3;
    constexpr size_t kBlocksPerBar3_4 = kSamplesPerBar3_4 / kBlock;
    size_t safety = 0;
    while (looper.isRecording())
    {
        looper.processBlock(in, out);
        ASSERT_LE(++safety, kBlocksPerBar3_4 + 10) << "recording never auto-stopped";
    }
    ASSERT_TRUE(looper.isPlaying());

    // Playback restarts at bar 0's own recorded meter, not wherever the take ended.
    EXPECT_EQ(looper.getBarBeats(), 4);

    for (size_t call = 0; call < kBlocksPerBar4_4; ++call)
    {
        looper.processBlock(in, out);
    }
    EXPECT_EQ(looper.getBarBeats(), 3) << "bar 2 of playback should replay the recorded meter change";

    for (size_t call = 0; call < kBlocksPerBar3_4; ++call)
    {
        looper.processBlock(in, out);
    }
    EXPECT_EQ(looper.getBarBeats(), 4) << "loop repeat should wrap back to bar 0's meter";
}

TEST(TimeSignatureChange, OuterRingAndBarLabelStableAcrossMeterChangeDuringPlayback)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setAutoStop(true);
    looper.setRecordBars(2);
    looper.setTimeSignature(2); // 4/4

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }

    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());

    looper.setTimeSignature(1); // 3/4, queued to apply at bar 2
    constexpr size_t kBlocksPerBar4_4 = kSamplesPerBar / kBlock;
    for (size_t call = 1; call < kBlocksPerBar4_4 - 1; ++call)
    {
        looper.processBlock(in, out);
    }
    looper.processBlock(in, out); // crosses into bar 2

    constexpr size_t kSamplesPerBar3_4 = kSamplesPerBeat * 3;
    constexpr size_t kBlocksPerBar3_4 = kSamplesPerBar3_4 / kBlock;
    size_t safety = 0;
    while (looper.isRecording())
    {
        looper.processBlock(in, out);
        ASSERT_LE(++safety, kBlocksPerBar3_4 + 10) << "recording never auto-stopped";
    }
    ASSERT_TRUE(looper.isPlaying());

    // The take-wide bar count must stay 2 regardless of which bar's meter the
    // live clock currently shows. Before the fix, getOuterRingBars() recomputed
    // loopLengthFrames()/getSamplesPerBar() from the LIVE (fluctuating) meter,
    // which for this exact loop (a 4/4 bar plus a 3/4 bar) read 1 during the
    // 4/4 bar and 2 during the 3/4 bar -- an unstable count that made the ring
    // visibly redraw wrong right at the meter change.
    EXPECT_EQ(looper.getOuterRingBars(), 2) << "bar 1 (4/4) of playback";
    EXPECT_EQ(looper.getBarFrameLengths().size(), 2u);
    EXPECT_EQ(looper.getBarBeatLabel(), "1.1");

    for (size_t call = 0; call < kBlocksPerBar4_4; ++call)
    {
        looper.processBlock(in, out);
    }
    EXPECT_EQ(looper.getOuterRingBars(), 2) << "bar 2 (3/4) of playback";
    EXPECT_EQ(looper.getBarFrameLengths().size(), 2u);
    EXPECT_EQ(looper.getBarBeatLabel(), "2.1");

    for (size_t call = 0; call < kBlocksPerBar3_4; ++call)
    {
        looper.processBlock(in, out);
    }
    EXPECT_EQ(looper.getOuterRingBars(), 2) << "loop repeat, back in bar 1 (4/4)";
    EXPECT_EQ(looper.getBarBeatLabel(), "1.1") << "loop repeat should wrap the bar label back to 1.1";
}

TEST(RecordingModes, PlayStopsActiveRecording)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);

    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out); // begins recording
    ASSERT_TRUE(looper.isRecording());
    for (int i = 0; i < 10; ++i)
    {
        looper.processBlock(in, out);
    }
    ASSERT_TRUE(looper.isRecording());

    looper.setPlay(true);
    for (int guard = 0; guard < 2000 && looper.isRecording(); ++guard)
    {
        looper.processBlock(in, out);
    }
    EXPECT_FALSE(looper.isRecording());
    EXPECT_TRUE(looper.isPlaying());
}

// Regression test: default Part Count/Capacity must already match what the
// constructor built, so no resize is pending on the first block - an earlier
// version's auto-resize silently dropped this exact first Record press.
TEST(PartSettings, FreshInstanceRecordsImmediatelyNoStartupResizePending)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    ASSERT_TRUE(looper.canEditPartSettings());

    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out);
    EXPECT_TRUE(looper.isRecording());
}

TEST(PartSettings, CanEditPartSettingsIsFalseWhileRecording)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);

    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());
    EXPECT_FALSE(looper.canEditPartSettings());
}

// A part-settings change requested mid-recording must not corrupt the take
// in progress - canEditPartSettings() being false keeps it queued instead.
TEST(PartSettings, PartSettingsChangeDuringRecordingDoesNotDisturbTheTake)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);

    Buffer in{};
    Buffer out{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 1.f;
        in(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());

    looper.setPartCount(2.f); // refused while recording; must not disturb the take
    for (int i = 0; i < 4; ++i)
    {
        looper.processBlock(in, out);
    }
    looper.setRecord(true);
    looper.processBlock(in, out); // stop
    ASSERT_FALSE(looper.isRecording());
    EXPECT_NEAR(looper.rawLoopSample(0, 0), 1.f, 1e-3f);
}

// End-to-end: a live capacity change while empty actually reaches
// PartBankResizeService and completes, and recording afterward still works.
TEST(PartSettings, ChangingPartCapacityWhileEmptyResizesThenRecordingStillWorks)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    ASSERT_TRUE(looper.canEditPartSettings());

    looper.setPartCapacityBars(4.f);
    Buffer in{};
    Buffer out{};
    for (int guard = 0; guard < 200; ++guard)
    {
        looper.processBlock(in, out);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    looper.setRecord(true);
    looper.processBlock(in, out);
    EXPECT_TRUE(looper.isRecording());
}

namespace
{
// Frees part 0 into Stopped (not audible), so a later selection change
// commits/redirects immediately instead of queuing for a bar boundary.
void recordThenStopPlayback(Looper& looper, Buffer& out, const float value)
{
    Buffer in{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = value;
        in(i, 1) = value;
    }
    looper.setRecord(true);
    looper.processBlock(in, out);
    looper.setRecord(true);
    looper.processBlock(in, out); // finalizes -> Playing
    looper.setPlay(true);
    looper.processBlock(Buffer{}, out); // -> Stopped
}
}

TEST(PartSelection, SelectingEmptyPartWhileActiveIsStoppedRecordsImmediatelyIntoNewPart)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f);

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    ASSERT_EQ(looper.currentSelectedPartIndex(), 1);

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 3.f;
        inB(i, 1) = 3.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out);
    ASSERT_TRUE(looper.isRecording()) << "the Record press must redirect into part 1, not stop anything";
    looper.setRecord(true);
    looper.processBlock(inB, out);
    EXPECT_NEAR(looper.rawLoopSample(0, 0), 3.f, 1e-3f);
}

TEST(PartSelection, SelectingPartWithContentSwitchesImmediatelyWhenActiveIsNotPlaying)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f);

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    recordThenStopPlayback(looper, out, 2.f); // redirected into part 1

    looper.setSelectedPart(0);
    looper.processBlock(Buffer{}, out);
    EXPECT_EQ(looper.currentSelectedPartIndex(), 0);
    EXPECT_NEAR(looper.rawLoopSample(0, 0), 1.f, 1e-3f) << "active part should now be part 0's content";
}

TEST(PartSelection, SelectingCurrentlyActivePartIsANoOp)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    ASSERT_EQ(looper.currentSelectedPartIndex(), 0);

    looper.setSelectedPart(0);
    Buffer in{};
    Buffer out{};
    looper.processBlock(in, out);
    EXPECT_EQ(looper.currentSelectedPartIndex(), 0);
    EXPECT_FALSE(looper.isRecording());
}

TEST(PartSelection, RefusedSwitchLeavesSelectedPartIndexUnchanged)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f);

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // redirected -> recording on part 1
    ASSERT_TRUE(looper.isRecording());
    ASSERT_EQ(looper.currentSelectedPartIndex(), 1);

    looper.setSelectedPart(0); // part 0 has content, but active part 1 is Recording -> refused
    looper.processBlock(inB, out);
    EXPECT_EQ(looper.currentSelectedPartIndex(), 1) << "a refused switch must not update the selected index";
}

// Regression: a part that was the *outgoing* side of an earlier switch is
// left Stopped; re-selecting it must resume playback, checked against real
// processBlock() output rather than rawLoopSample()'s raw stored buffer.
TEST(PartSelection, SwitchingBackToAPreviouslyStoppedPartResumesItsPlayback)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};
    looper.setClickVolume(-60.f);
    recordThenStopPlayback(looper, out, 1.f); // part 0: value 1, active=0, Stopped

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    recordThenStopPlayback(looper, out, 2.f); // redirected into part 1: value 2, active=1, Stopped

    looper.setPlay(true);
    looper.processBlock(Buffer{}, out); // part 1: Stopped -> Playing (audible)

    looper.setSelectedPart(0); // part 0 has content, active(1) Playing -> queue+crossfade
    looper.processBlock(Buffer{}, out);
    ASSERT_EQ(looper.currentSelectedPartIndex(), 0);

    for (int i = 0; i < 5000; ++i)
    {
        looper.processBlock(Buffer{}, out);
    }
    EXPECT_TRUE(looper.isPlaying()) << "part 0 must resume playback, not stay Stopped";
    EXPECT_NEAR(out(kBlock - 1, 0), 1.f, 1e-3f) << "audible output must be part 0's own content";
}

// Regression: Count-In used to short-circuit toggleRecord() before the
// redirect check ran, so it always re-recorded the active part instead.
TEST(PartSelection, RedirectedRecordSkipsCountInWhenActivePartIsStopped)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f); // part 0: value 1, active=0, Stopped

    looper.setCountInBars(2); // must not apply to the redirected take
    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    ASSERT_EQ(looper.currentSelectedPartIndex(), 1);

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out);
    EXPECT_FALSE(looper.isCountingIn()) << "a redirected take must not count in again";
    EXPECT_TRUE(looper.isRecording()) << "the redirected take must start immediately";
}

// Same gap, matching the actually-reported scenario: the active part is
// still Playing when Record is pressed, so the redirect queues a crossfade.
TEST(PartSelection, RedirectedRecordSkipsCountInWhenActivePartIsPlaying)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};
    Buffer inA{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 1.f;
        inA(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(inA, out);
    looper.setRecord(true);
    looper.processBlock(inA, out); // part 0: value 1, finalizeFree() leaves it Playing (audible)
    ASSERT_TRUE(looper.isPlaying());

    looper.setCountInBars(2); // must not apply to the redirected take
    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // queued: A is audible, so this waits for a bar boundary
    EXPECT_FALSE(looper.isCountingIn()) << "a redirected take must not count in, queued or not";

    for (int i = 0; i < 5000 && !looper.isRecording(); ++i)
    {
        looper.processBlock(inB, out);
        ASSERT_FALSE(looper.isCountingIn()) << "a redirected take must never count in, even after the crossfade";
    }
    EXPECT_TRUE(looper.isRecording()) << "the redirected take must start once the crossfade completes";
}

// Bar-locked, neither part explicitly stopped before switching: both the
// A->B and B->A transitions go through the queued crossfade path, matching
// the default UI's actual usage pattern (not the immediate-commit shortcut).
TEST(PartSelection, BarLockedTwoQueuedCrossfadesInARowRestoresEachPartsOwnPlayback)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFadeMs(0.f);
    looper.setClickVolume(-60.f);
    Buffer out{};
    Buffer inA{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 1.f;
        inA(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(inA, out);
    size_t elapsed = kBlock;
    while (elapsed < kSamplesPerBar)
    {
        looper.processBlock(inA, out);
        elapsed += kBlock;
    }
    looper.setRecord(true); // request bar-locked stop (async, quantized)
    while (looper.isRecording())
    {
        looper.processBlock(inA, out);
    }
    ASSERT_TRUE(looper.isPlaying());

    looper.setSelectedPart(1); // part 0 still Playing/audible -> no immediate action
    looper.processBlock(Buffer{}, out);

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true); // queued: waits for a bar boundary before starting
    looper.processBlock(inB, out);
    for (int i = 0; i < 5000 && !looper.isRecording(); ++i)
    {
        looper.processBlock(inB, out);
    }
    ASSERT_TRUE(looper.isRecording()) << "queued record-switch into part 1 never committed";

    elapsed = kBlock;
    while (elapsed < kSamplesPerBar)
    {
        looper.processBlock(inB, out);
        elapsed += kBlock;
    }
    looper.setRecord(true); // request bar-locked stop for part 1
    while (looper.isRecording())
    {
        looper.processBlock(inB, out);
    }
    ASSERT_TRUE(looper.isPlaying());

    looper.setSelectedPart(0); // part 1 audible -> queues the switch back
    looper.processBlock(Buffer{}, out);
    ASSERT_EQ(looper.currentSelectedPartIndex(), 0);
    for (int i = 0; i < 5000; ++i)
    {
        looper.processBlock(Buffer{}, out);
    }
    EXPECT_TRUE(looper.isPlaying()) << "part 0 must resume playback, not stay Stopped";
    EXPECT_NEAR(out(kBlock - 1, 0), 1.f, 1e-3f) << "audible output must be part 0's own content again";
}

// Regression: a queued switch used to commit at the next global bar
// instead of when the active part's own multi-bar loop actually wraps.
// Part 0 is 8 bars; a switch requested partway through must wait it out.
TEST(PartSelection, QueuedSwitchWaitsForActiveLoopsOwnLengthNotJustOneBar)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFadeMs(0.f);
    looper.setAutoStop(true);
    looper.setRecordBars(8);
    Buffer out{};
    Buffer inA{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 1.f;
        inA(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(inA, out);
    while (looper.isRecording())
    {
        looper.processBlock(inA, out);
    }
    ASSERT_TRUE(looper.isPlaying());
    ASSERT_EQ(looper.rawLoopLengthFrames(), 8 * kSamplesPerBar);

    // Run playback 2 bars into the loop, nowhere near a wrap.
    for (size_t i = 0; i < 2 * kSamplesPerBar; i += kBlock)
    {
        looper.processBlock(Buffer{}, out);
    }

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // queued: part 0 is audible

    for (size_t i = 0; i < kSamplesPerBar; i += kBlock)
    {
        looper.processBlock(inB, out);
    }
    EXPECT_FALSE(looper.isRecording()) << "must not commit at the next bar; 5+ bars remain in part 0's own loop";

    for (size_t i = 0; i < 8 * kSamplesPerBar; i += kBlock)
    {
        looper.processBlock(inB, out);
    }
    EXPECT_TRUE(looper.isRecording()) << "must commit once part 0's own loop actually wraps";
}

TEST(PartSelection, PartStatusLabelReflectsContentAndEmptiness)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);

    EXPECT_EQ(looper.partStatusLabel(0), "Part A: free");
    EXPECT_EQ(looper.partStatusLabel(1), "Part B: free");

    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f);
    EXPECT_NE(looper.partStatusLabel(0), "Part A: free");
}

// Regression: bpm used to be one value shared by the whole session; each
// part now remembers and re-applies its own tempo across a switch.
TEST(PartSelection, SwitchingPartsAppliesEachPartsOwnBpm)
{
    Looper looper(kSampleRate);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);

    looper.setBpm(90.f);
    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f); // part 0 recorded at 90 BPM
    EXPECT_NEAR(looper.currentAppliedBpm(), 90.f, 1e-3f);

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out); // part 1 empty -> no immediate switch yet

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // redirected: part 0 was Stopped, so this commits immediately
    ASSERT_TRUE(looper.isRecording());

    looper.setBpm(140.f); // dialed in while part 1 is the active part
    looper.processBlock(inB, out);
    looper.setRecord(true);
    looper.processBlock(inB, out); // stop
    EXPECT_NEAR(looper.currentAppliedBpm(), 140.f, 1e-3f);

    looper.setSelectedPart(0); // part 0 has content, part 1 audible -> queues
    looper.processBlock(Buffer{}, out);
    for (int i = 0; i < 200; ++i)
    {
        looper.processBlock(Buffer{}, out);
    }
    EXPECT_NEAR(looper.currentAppliedBpm(), 90.f, 1e-3f) << "part 0's own tempo must come back, not part 1's";
}

// Regression: switching into an empty part used to force-reset the live
// bpm to its unset 120 default instead of carrying over the current one.
TEST(PartSelection, RedirectedRecordInheritsTheCurrentBpmNotDefault)
{
    Looper looper(kSampleRate);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);

    looper.setBpm(90.f);
    Buffer out{};
    recordThenStopPlayback(looper, out, 1.f); // part 0 recorded at 90 BPM

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out); // part 1 empty -> no immediate switch yet

    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // redirected: part 0 was Stopped, so this commits immediately
    ASSERT_TRUE(looper.isRecording());
    looper.processBlock(inB, out); // lets the inherited live bpm get adopted into part 1's own slot
    EXPECT_NEAR(looper.currentAppliedBpm(), 90.f, 1e-3f) << "must inherit the live bpm, not reset to 120";
}

// Regression: turning the dial while a recorded part is Stopped (allowed by
// canEditBpm) used to immediately rescale that part's own display away from
// its actual fixed-length audio, before any new take was ever recorded.
TEST(PartSelection, BpmDialChangeWhileStoppedDoesNotDesyncUntilReRecorded)
{
    Looper looper(kSampleRate);
    looper.setThreshRec(false);
    looper.setFadeMs(0.f);
    looper.setClickVolume(-60.f);
    Buffer out{};
    Buffer inA{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 1.f;
        inA(i, 1) = 1.f;
    }
    looper.setBpm(90.f);
    looper.setRecord(true);
    looper.processBlock(inA, out);
    const size_t samplesPerBar90 = looper.getSamplesPerBar();
    for (size_t elapsed = kBlock; elapsed < samplesPerBar90; elapsed += kBlock)
    {
        looper.processBlock(inA, out);
    }
    looper.setRecord(true);
    while (looper.isRecording())
    {
        looper.processBlock(inA, out);
    }
    ASSERT_TRUE(looper.isPlaying());

    looper.setPlay(true); // toggle: stop playback
    looper.processBlock(Buffer{}, out);
    ASSERT_FALSE(looper.isPlaying());

    looper.setBpm(140.f); // allowed by canEditBpm while Stopped, must not apply yet
    looper.processBlock(Buffer{}, out);
    EXPECT_EQ(looper.getSamplesPerBar(), samplesPerBar90) << "display must stay on the recorded part's own tempo";
    EXPECT_NEAR(looper.currentAppliedBpm(), 90.f, 1e-3f);

    looper.setRecord(true); // re-record over the same part: now 140 may apply
    while (!looper.isRecording())
    {
        looper.processBlock(inA, out);
    }
    looper.processBlock(inA, out); // lets the now-Recording state adopt the dialed-in tempo
    EXPECT_NEAR(looper.currentAppliedBpm(), 140.f, 1e-3f) << "a fresh take on this part adopts the dialed-in tempo";
}

TEST(SpectrogramWrap, IsWrappedOnlyOnceRecordingStops)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);

    Buffer in{};
    Buffer out{};
    EXPECT_TRUE(looper.isSpectrogramWrapped()); // not recording, no loop: vacuously true

    looper.setRecord(true);
    looper.processBlock(in, out); // begins recording
    ASSERT_TRUE(looper.isRecording());
    EXPECT_FALSE(looper.isSpectrogramWrapped());

    for (int i = 0; i < 10; ++i)
    {
        looper.processBlock(in, out);
    }
    looper.setRecord(true);
    looper.processBlock(in, out); // stops; finalizes immediately
    ASSERT_FALSE(looper.isRecording());
    EXPECT_TRUE(looper.isSpectrogramWrapped());
}

// Regression for the seam-gap fix: LooperImpl::primeSpectrogramWindowFromLoopTail
// feeds the loop's own tail into the FFT window before frame 0, so the first
// completed slice after regen's reset() (slot 0) should carry the tail's energy,
// not the silence that opened the recording.
TEST(SpectrogramWrap, SeamSliceReflectsLoopTailNotRecordingStart)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFadeMs(0.f);

    Buffer in{};
    Buffer out{};
    looper.setRecord(true);
    looper.processBlock(in, out); // begins recording

    constexpr int kSilentBlocks = 400;
    for (int b = 0; b < kSilentBlocks; ++b)
    {
        looper.processBlock(in, out);
    }

    constexpr int kLoudBlocks = 300; // 4800 frames, comfortably over the prime window
    for (int b = 0; b < kLoudBlocks; ++b)
    {
        for (size_t i = 0; i < kBlock; ++i)
        {
            const float v = ((i % 2) == 0) ? 0.8f : -0.8f;
            in(i, 0) = v;
            in(i, 1) = v;
        }
        looper.processBlock(in, out);
    }

    looper.setRecord(true);
    looper.processBlock(in, out); // stops; finalizes immediately
    ASSERT_FALSE(looper.isRecording());

    bool wrapped = false;
    for (int i = 0; i < 2000 && !wrapped; ++i)
    {
        looper.processBlock(in, out);
        wrapped = looper.isSpectrogramWrapped() && looper.getSpectrogramHeadFrames() > 0;
        if (!wrapped)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    ASSERT_TRUE(wrapped) << "spectrogram regen never completed";

    const auto img = looper.getSpectrogramData();
    ASSERT_NE(img.data, nullptr);
    ASSERT_GT(img.height, 0u);

    float seamEnergy = 0.f;
    for (size_t bin = 0; bin < img.height; ++bin)
    {
        seamEnergy += img.data[bin]; // slot 0: reset() zeroed currentSlice, first write lands here
    }
    EXPECT_GT(seamEnergy, 0.01f) << "seam slice should carry real energy from the loop's loud tail";
}

TEST(TransportStatusText, StoppedWhenFreshlyConstructed)
{
    Looper looper(kSampleRate);
    EXPECT_EQ(looper.transportStatusText(), "Stopped");
}

TEST(TransportStatusText, ArmedWhileWaitingForThreshold)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(true);
    looper.setRecThreshold(-24.f);

    looper.setRecord(true); // arms, nothing loud enough to trigger yet
    Buffer out{};
    looper.processBlock(Buffer{}, out);
    ASSERT_TRUE(looper.isArmed());
    EXPECT_EQ(looper.transportStatusText(), "Armed, waiting for input");
}

TEST(TransportStatusText, CountingIn)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setCountInBars(2);

    looper.setRecord(true);
    Buffer out{};
    looper.processBlock(Buffer{}, out);
    ASSERT_TRUE(looper.isCountingIn());
    EXPECT_EQ(looper.transportStatusText(), "Counting in");
}

TEST(TransportStatusText, RecordingShowsBarCountWhenAutoStopArmed)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setAutoStop(true);
    looper.setRecordBars(2);

    Buffer in{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 0.3f;
        in(i, 1) = -0.3f;
    }
    looper.setRecord(true);
    Buffer out{};
    looper.processBlock(in, out);
    ASSERT_TRUE(looper.isRecording());
    EXPECT_EQ(looper.transportStatusText(), "Recording (bar 1 of 2)");
}

TEST(TransportStatusText, PlayingOnceALoopExists)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);

    Buffer out{};
    looper.setRecord(true);
    Buffer in{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        in(i, 0) = 1.f;
        in(i, 1) = 1.f;
    }
    looper.processBlock(in, out);
    looper.setRecord(true);
    looper.processBlock(in, out); // finalizes -> auto-transitions to Playing
    EXPECT_EQ(looper.transportStatusText(), "Playing");
}

// Matches PartSelection.QueuedSwitchWaitsForActiveLoopsOwnLengthNotJustOneBar's setup:
// selecting an empty part while the active one is audible queues a record-redirect.
TEST(TransportStatusText, RecordRedirectQueuedNamesTheTargetPart)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFadeMs(0.f);
    looper.setAutoStop(true);
    looper.setRecordBars(8);

    Buffer out{};
    Buffer inA{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inA(i, 0) = 1.f;
        inA(i, 1) = 1.f;
    }
    looper.setRecord(true);
    looper.processBlock(inA, out);
    while (looper.isRecording())
    {
        looper.processBlock(inA, out);
    }
    ASSERT_TRUE(looper.isPlaying());

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // queued: part 0 is still audible
    ASSERT_FALSE(looper.isRecording());
    EXPECT_EQ(looper.transportStatusText(), "Record redirect to Part B queued");
}

TEST(TransportStatusText, SwitchToAlreadyRecordedPartQueuedNamesTheTargetPart)
{
    Looper looper(kSampleRate);
    looper.setBpm(kBpm);
    looper.setThreshRec(false);
    looper.setFreeRecord(true);
    looper.setFadeMs(0.f);
    Buffer out{};

    // Part A: recorded then explicitly stopped, so the redirect below commits immediately.
    recordThenStopPlayback(looper, out, 1.f);

    looper.setSelectedPart(1);
    looper.processBlock(Buffer{}, out);
    Buffer inB{};
    for (size_t i = 0; i < kBlock; ++i)
    {
        inB(i, 0) = 2.f;
        inB(i, 1) = 2.f;
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // part A Stopped -> redirect commits immediately
    ASSERT_TRUE(looper.isRecording());
    for (int i = 0; i < 4; ++i)
    {
        looper.processBlock(inB, out); // several blocks, so the loop doesn't wrap in a single block below
    }
    looper.setRecord(true);
    looper.processBlock(inB, out); // finalizes -> Part B Playing
    ASSERT_TRUE(looper.isPlaying());

    looper.setSelectedPart(0); // Part A has content, Part B is audible -> queues
    looper.processBlock(Buffer{}, out);
    EXPECT_EQ(looper.transportStatusText(), "Switch to Part A queued");
}
