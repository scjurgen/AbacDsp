#include <gtest/gtest.h>
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

}
