#include <gtest/gtest.h>
#include <tuple>
#include <vector>

#include "Generators/BeatSequencer.h"

namespace AbacDsp::test
{

namespace
{
constexpr float kSampleRate{48000.f};

struct BeatMark
{
    size_t sampleIndex{0};
    size_t beatIndexInBar{0};
    bool barWrapped{false};
};

// Runs the sequencer for n samples, collecting every beat-start.
[[nodiscard]] std::vector<BeatMark> collectBeats(BeatSequencer& seq, const size_t samples)
{
    std::vector<BeatMark> beats;
    for (size_t i = 0; i < samples; ++i)
    {
        const auto event = seq.advance();
        if (event.beatStart)
        {
            beats.push_back({i, event.beatIndexInBar, event.barWrapped});
        }
    }
    return beats;
}
}

TEST(BeatSequencerTest, SamplesPerBeatMatchesBpm)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    // 120 BPM -> 0.5 s per beat -> 24000 samples at 48k.
    EXPECT_EQ(seq.samplesPerBeat(), 24000u);

    seq.setBpm(60.f);
    EXPECT_EQ(seq.samplesPerBeat(), 48000u);
}

TEST(BeatSequencerTest, SamplesPerBeatRoundsInsteadOfTruncating)
{
    BeatSequencer seq{kSampleRate};
    // 95 BPM -> 30315.789... samples/beat; truncation gave 30315, nearest is 30316.
    seq.setBpm(95.f);
    EXPECT_EQ(seq.samplesPerBeat(), 30316u);
}

// Regression: a fixed rounded samples-per-beat compounds its own rounding error over many
// beats (24 * round(36923.0769) = 886152, two samples short of the 886154 a 6-bar/4-4 take
// at 78 BPM should actually land on). The accumulator in nextBeatLength() must not drift.
TEST(BeatSequencerTest, BeatBoundariesStayWithinOneSampleOfContinuousTempoOverManyBeats)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(78.f);
    constexpr size_t kBeats = 24; // 6 bars of 4/4, the originally reported scenario
    constexpr double kExactSamplesPerBeat = static_cast<double>(kSampleRate) * 60.0 / 78.0;
    const auto idealTotal = static_cast<size_t>(kExactSamplesPerBeat * static_cast<double>(kBeats) + 0.5);

    size_t completedBeats = 0;
    size_t sample = 0;
    size_t totalFrames = 0;
    while (completedBeats < kBeats)
    {
        const auto event = seq.advance();
        if (event.beatStart && sample > 0)
        {
            ++completedBeats;
            if (completedBeats == kBeats)
            {
                totalFrames = sample;
            }
        }
        ++sample;
    }
    EXPECT_EQ(totalFrames, idealTotal);
}

TEST(BeatSequencerTest, BeatStartsAreEvenlySpaced)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto beats = collectBeats(seq, seq.samplesPerBeat() * 4);
    ASSERT_EQ(beats.size(), 4u);
    for (size_t i = 0; i < beats.size(); ++i)
    {
        EXPECT_EQ(beats[i].sampleIndex, i * seq.samplesPerBeat());
    }
}

TEST(BeatSequencerTest, BeatIndexCyclesAndBarWraps)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto beats = collectBeats(seq, seq.samplesPerBeat() * 8);
    ASSERT_EQ(beats.size(), 8u);
    const std::vector<size_t> expectedIndices{0, 1, 2, 3, 0, 1, 2, 3};
    for (size_t i = 0; i < beats.size(); ++i)
    {
        EXPECT_EQ(beats[i].beatIndexInBar, expectedIndices[i]) << "beat " << i;
    }
    // The bar wraps on the sample that completes beat 3 -> the next beat 0 sees no wrap flag,
    // but the wrap flag rides the sample that advances past the last beat.
    EXPECT_TRUE(beats[4].beatIndexInBar == 0u);
}

TEST(BeatSequencerTest, BarWrapFlagFiresOncePerBar)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(3);

    size_t wraps = 0;
    const size_t total = seq.samplesPerBeat() * 3 * 4; // four bars
    for (size_t i = 0; i < total; ++i)
    {
        if (seq.advance().barWrapped)
        {
            ++wraps;
        }
    }
    EXPECT_EQ(wraps, 4u);
}

TEST(BeatSequencerTest, BarIndexIncrementsOnceBarBoundaryIsCrossed)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(3);

    const size_t samplesPerBar = seq.samplesPerBeat() * 3;
    EXPECT_EQ(seq.barIndex(), 0u);
    for (size_t i = 0; i < samplesPerBar; ++i)
    {
        std::ignore = seq.advance();
    }
    EXPECT_EQ(seq.barIndex(), 1u);
    for (size_t i = 0; i < samplesPerBar * 2; ++i)
    {
        std::ignore = seq.advance();
    }
    EXPECT_EQ(seq.barIndex(), 3u);
}

TEST(BeatSequencerTest, ResetZeroesBarIndex)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(2);

    const size_t samplesPerBar = seq.samplesPerBeat() * 2;
    for (size_t i = 0; i < samplesPerBar * 3; ++i)
    {
        std::ignore = seq.advance();
    }
    ASSERT_GT(seq.barIndex(), 0u);
    seq.reset();
    EXPECT_EQ(seq.barIndex(), 0u);
}

TEST(BeatSequencerTest, EighthSubdivisionAtBeatMidpoint)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setSubdivType(SubdivType::Eighth);

    ASSERT_EQ(seq.subPositions().size(), 1u);
    EXPECT_EQ(seq.subPositions()[0], seq.samplesPerBeat() / 2);
}

TEST(BeatSequencerTest, SixteenthSubdivisionsAtQuarters)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setSubdivType(SubdivType::Sixteenth);

    const auto spb = seq.samplesPerBeat();
    ASSERT_EQ(seq.subPositions().size(), 3u);
    EXPECT_EQ(seq.subPositions()[0], spb / 4);
    EXPECT_EQ(seq.subPositions()[1], spb / 2);
    EXPECT_EQ(seq.subPositions()[2], 3 * spb / 4);
}

TEST(BeatSequencerTest, ShuffleSubdivisionShiftsWithSwing)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setSubdivType(SubdivType::Shuffle);

    seq.setSwingRatio(1.f); // straight -> midpoint
    ASSERT_EQ(seq.subPositions().size(), 1u);
    const auto straight = seq.subPositions()[0];
    EXPECT_EQ(straight, seq.samplesPerBeat() / 2);

    seq.setSwingRatio(2.f); // heavy swing -> long part is 2/3
    const auto swung = seq.subPositions()[0];
    EXPECT_GT(swung, straight);
    EXPECT_EQ(swung, 2 * seq.samplesPerBeat() / 3);
}

TEST(BeatSequencerTest, SubdivisionEventsFireAtSubPositions)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);
    seq.setSubdivType(SubdivType::Eighth);

    const auto expected = seq.samplesPerBeat() / 2;
    bool sawSub = false;
    for (size_t i = 0; i < seq.samplesPerBeat(); ++i)
    {
        const auto event = seq.advance();
        if (event.subdivision)
        {
            sawSub = true;
            EXPECT_EQ(event.beatSamplePos, expected);
        }
    }
    EXPECT_TRUE(sawSub);
}

TEST(BeatSequencerTest, SyncToPpqSetsBeatAndPhase)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    // ppq 2.5 -> beat 2 of the bar, halfway through the beat.
    seq.syncToPpq(2.5);
    EXPECT_EQ(seq.beatIndexInBar(), 2u);
    EXPECT_EQ(seq.beatSamplePos(), seq.samplesPerBeat() / 2);
}

TEST(BeatSequencerTest, SyncToPpqWrapsAcrossBars)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    // ppq 9.0 -> bar 2, beat 1 (9 mod 4 = 1).
    seq.syncToPpq(9.0);
    EXPECT_EQ(seq.beatIndexInBar(), 1u);
    EXPECT_EQ(seq.beatSamplePos(), 0u);
}

TEST(BeatSequencerTest, SyncToPpqHandlesNegativePosition)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    // ppq -1.0 -> should fold to beat 3 of the bar.
    seq.syncToPpq(-1.0);
    EXPECT_EQ(seq.beatIndexInBar(), 3u);
}

TEST(BeatSequencerTest, BarPhaseProgressesMonotonicallyWithinBar)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    float previous = -1.f;
    const size_t samplesInBar = seq.samplesPerBeat() * 4;
    for (size_t i = 0; i < samplesInBar; ++i)
    {
        const float phase = seq.barPhase();
        EXPECT_GE(phase, previous);
        EXPECT_LT(phase, 1.f);
        previous = phase;
        static_cast<void>(seq.advance());
    }
}

TEST(BeatSequencerTest, SamplesToNearestBeatAtBoundaryIsZero)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000
    EXPECT_EQ(seq.samplesToNearestBeat(), 0);
}

TEST(BeatSequencerTest, SamplesToNearestBeatEarlyIsNegativeDistanceToPreviousBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000
    for (int i = 0; i < 1000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    // 1000 samples into the beat: nearer to the previous boundary (behind us).
    EXPECT_EQ(seq.samplesToNearestBeat(), -1000);
}

TEST(BeatSequencerTest, SamplesToNearestBeatLateIsPositiveDistanceToNextBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000
    for (int i = 0; i < 23000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    // 1000 samples before the next boundary: nearer to it (ahead of us).
    EXPECT_EQ(seq.samplesToNearestBeat(), 1000);
}

TEST(BeatSequencerTest, SamplesToNearestBeatTieResolvesToPreviousBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000, half = 12000
    for (int i = 0; i < 12000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    EXPECT_EQ(seq.samplesToNearestBeat(), -12000);
}

TEST(BeatSequencerTest, SamplesToNearestBarAtBoundaryIsZero)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000, samplesPerBar = 96000
    EXPECT_EQ(seq.samplesToNearestBar(), 0);
}

TEST(BeatSequencerTest, SamplesToNearestBarEarlyInBarIsNegativeDistanceToPreviousBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000, samplesPerBar = 96000
    for (int i = 0; i < 1000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    EXPECT_EQ(seq.samplesToNearestBar(), -1000);
}

TEST(BeatSequencerTest, SamplesToNearestBarSpansMultipleBeatsTowardNextBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000, samplesPerBar = 96000
    // 3 full beats plus 1000 samples: beatIndexInBar=3, position-in-bar=73000,
    // nearer to the next bar boundary (96000) than the previous one (0).
    for (int i = 0; i < 73000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    EXPECT_EQ(seq.samplesToNearestBar(), 23000);
}

TEST(BeatSequencerTest, SamplesToNearestBarLateIsPositiveDistanceToNextBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000, samplesPerBar = 96000
    for (int i = 0; i < 95000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    EXPECT_EQ(seq.samplesToNearestBar(), 1000);
}

TEST(BeatSequencerTest, SamplesToNearestBarTieResolvesToPreviousBoundary)
{
    BeatSequencer seq{kSampleRate};
    seq.setBpm(120.f); // samplesPerBeat = 24000, samplesPerBar = 96000, half = 48000
    for (int i = 0; i < 48000; ++i)
    {
        static_cast<void>(seq.advance());
    }
    EXPECT_EQ(seq.samplesToNearestBar(), -48000);
}

}
