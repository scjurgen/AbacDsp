#include <gtest/gtest.h>
#include <vector>

#include "Analysis/Slicer.h"
#include "Sampler/SliceLibrary.h"

namespace AbacDsp::test
{

namespace
{
// Interleaved stereo loop where L = frame index, R = -(frame index), so every
// frame and channel is uniquely identifiable after extraction.
[[nodiscard]] std::vector<float> makeRampLoop(const size_t frames)
{
    std::vector<float> loop(frames * 2, 0.f);
    for (size_t f = 0; f < frames; ++f)
    {
        loop[f * 2] = static_cast<float>(f);
        loop[f * 2 + 1] = -static_cast<float>(f);
    }
    return loop;
}
}

TEST(SliceLibraryTest, ExtractGridPartitionCopiesContiguously)
{
    const auto loop = makeRampLoop(100);
    const auto slices = Slicer::gridSlices(100, 4);
    SliceLibrary library(100);
    const size_t track = library.extractTrack(loop, slices);

    EXPECT_EQ(track, 0u);
    EXPECT_EQ(library.sliceCountInTrack(0), 4u);
    EXPECT_EQ(library.usedFrames(), 100u);

    size_t expectedOffset = 0;
    for (size_t i = 0; i < library.sliceCountInTrack(0); ++i)
    {
        EXPECT_EQ(library.sliceInfo(0, i).startFrame, expectedOffset);
        expectedOffset += library.sliceInfo(0, i).lengthFrames;
    }
    EXPECT_EQ(expectedOffset, 100u);
}

TEST(SliceLibraryTest, ExtractedAudioMatchesSourceRegions)
{
    const auto loop = makeRampLoop(100);
    const auto slices = Slicer::gridSlices(100, 4);
    SliceLibrary library(100);
    library.extractTrack(loop, slices);

    for (size_t i = 0; i < library.sliceCountInTrack(0); ++i)
    {
        const size_t srcStart = slices[i].startFrame;
        for (size_t f = 0; f < library.sliceInfo(0, i).lengthFrames; ++f)
        {
            EXPECT_FLOAT_EQ(library.sample(0, i, f, 0), static_cast<float>(srcStart + f));
            EXPECT_FLOAT_EQ(library.sample(0, i, f, 1), -static_cast<float>(srcStart + f));
        }
    }
}

TEST(SliceLibraryTest, PoolViewIsContiguousAcrossSlices)
{
    const auto loop = makeRampLoop(60);
    const auto slices = Slicer::gridSlices(60, 3);
    SliceLibrary library(60);
    library.extractTrack(loop, slices);

    const std::span<const float> pool = library.pool();
    ASSERT_EQ(pool.size(), 60u * 2u);
    for (size_t i = 0; i < library.sliceCountInTrack(0); ++i)
    {
        const auto& s = library.sliceInfo(0, i);
        const size_t srcStart = slices[i].startFrame;
        for (size_t f = 0; f < s.lengthFrames; ++f)
        {
            EXPECT_FLOAT_EQ(pool[(s.startFrame + f) * 2], static_cast<float>(srcStart + f));
        }
    }
}

TEST(SliceLibraryTest, NonContiguousSlicesPackTightlyButKeepContent)
{
    const auto loop = makeRampLoop(100);
    // Two disjoint slices with a gap between them (transient-style).
    const std::vector<Slice> slices{{10, 20}, {70, 15}};
    SliceLibrary library(100);
    library.extractTrack(loop, slices);

    ASSERT_EQ(library.sliceCountInTrack(0), 2u);
    EXPECT_EQ(library.usedFrames(), 35u);
    EXPECT_EQ(library.sliceInfo(0, 0).startFrame, 0u);
    EXPECT_EQ(library.sliceInfo(0, 1).startFrame, 20u); // packed right after the first

    EXPECT_FLOAT_EQ(library.sample(0, 0, 0, 0), 10.f);
    EXPECT_FLOAT_EQ(library.sample(0, 0, 19, 0), 29.f);
    EXPECT_FLOAT_EQ(library.sample(0, 1, 0, 0), 70.f);
    EXPECT_FLOAT_EQ(library.sample(0, 1, 14, 0), 84.f);
}

TEST(SliceLibraryTest, SliceLengthClampedToLoopEnd)
{
    const auto loop = makeRampLoop(50);
    const std::vector<Slice> slices{{40, 100}}; // runs past the loop end
    SliceLibrary library(50);
    library.extractTrack(loop, slices);

    ASSERT_EQ(library.sliceCountInTrack(0), 1u);
    EXPECT_EQ(library.sliceInfo(0, 0).lengthFrames, 10u); // 50 - 40
}

TEST(SliceLibraryTest, PoolOverflowStopsExtractionForThatTrack)
{
    const auto loop = makeRampLoop(100);
    const auto slices = Slicer::gridSlices(100, 4); // 25 frames each
    SliceLibrary library(60);                       // room for 2 slices + partial
    library.extractTrack(loop, slices);

    // Third slice (offset 50, len 25) would need 75 > 60, so extraction stops at 2.
    EXPECT_EQ(library.sliceCountInTrack(0), 2u);
    EXPECT_EQ(library.usedFrames(), 50u);
}

TEST(SliceLibraryTest, ClearResetsAllTracks)
{
    const auto loop = makeRampLoop(40);
    SliceLibrary library(40);
    library.extractTrack(loop, Slicer::gridSlices(40, 2));
    ASSERT_EQ(library.sliceCount(), 2u);

    library.clear();
    EXPECT_EQ(library.sliceCount(), 0u);
    EXPECT_EQ(library.trackCount(), 0u);
    EXPECT_EQ(library.usedFrames(), 0u);
    EXPECT_TRUE(library.pool().empty());
}

TEST(SliceLibraryTest, EmptyInputsProduceEmptyTrack)
{
    SliceLibrary library(100);
    const size_t track = library.extractTrack({}, {});
    EXPECT_EQ(track, 0u);
    EXPECT_EQ(library.trackCount(), 1u);
    EXPECT_EQ(library.sliceCountInTrack(0), 0u);
    EXPECT_EQ(library.usedFrames(), 0u);

    const auto loop = makeRampLoop(50);
    library.extractTrack(loop, {}); // audio but no slices
    EXPECT_EQ(library.trackCount(), 2u);
    EXPECT_EQ(library.sliceCountInTrack(1), 0u);
}

TEST(SliceLibraryTest, SkipsSlicesStartingBeyondLoop)
{
    const auto loop = makeRampLoop(30);
    const std::vector<Slice> slices{{10, 5}, {40, 5}, {20, 5}};
    SliceLibrary library(30);
    library.extractTrack(loop, slices);

    // The middle slice starts past the loop end and is skipped; the other two remain.
    ASSERT_EQ(library.sliceCountInTrack(0), 2u);
    EXPECT_FLOAT_EQ(library.sample(0, 0, 0, 0), 10.f);
    EXPECT_FLOAT_EQ(library.sample(0, 1, 0, 0), 20.f);
}

TEST(SliceLibraryTest, MultipleFreezesAppendSeparateTracks)
{
    SliceLibrary library(200);
    const auto loopA = makeRampLoop(40);
    const auto loopB = makeRampLoop(30);

    const size_t trackA = library.extractTrack(loopA, Slicer::gridSlices(40, 2));
    const size_t trackB = library.extractTrack(loopB, Slicer::gridSlices(30, 3));

    EXPECT_EQ(trackA, 0u);
    EXPECT_EQ(trackB, 1u);
    EXPECT_EQ(library.trackCount(), 2u);
    EXPECT_EQ(library.sliceCountInTrack(0), 2u);
    EXPECT_EQ(library.sliceCountInTrack(1), 3u);
    EXPECT_EQ(library.sliceCount(), 5u);
    EXPECT_EQ(library.usedFrames(), 70u);

    // Track B's pool offsets continue right after track A's, not overlapping.
    EXPECT_EQ(library.sliceInfo(1, 0).startFrame, 40u);

    // Content for each track still traces back to its own source loop.
    EXPECT_FLOAT_EQ(library.sample(0, 0, 0, 0), 0.f);
    EXPECT_FLOAT_EQ(library.sample(1, 0, 0, 0), 0.f);
    EXPECT_FLOAT_EQ(library.sample(1, 0, 1, 0), 1.f);
}

TEST(SliceLibraryTest, EarlierTracksSurviveWhenALaterOneOverflows)
{
    SliceLibrary library(50);
    const auto loopA = makeRampLoop(40);
    const auto loopB = makeRampLoop(40);

    library.extractTrack(loopA, Slicer::gridSlices(40, 2)); // fills 40 of 50 frames
    library.extractTrack(loopB, Slicer::gridSlices(40, 4)); // 10-frame slices; only 1 fits before overflow

    EXPECT_EQ(library.sliceCountInTrack(0), 2u);
    EXPECT_EQ(library.sliceCountInTrack(1), 1u);
    EXPECT_EQ(library.usedFrames(), 50u);
    // Track A content untouched by track B's overflow.
    EXPECT_FLOAT_EQ(library.sample(0, 0, 0, 0), 0.f);
    EXPECT_FLOAT_EQ(library.sample(0, 1, 0, 0), 20.f);
}

TEST(SliceLibraryTest, PeakAndRmsComputedPerSlice)
{
    // Single slice: L channel is a ramp 0..9, R channel is its negation.
    std::vector<float> loop(20, 0.f);
    for (size_t f = 0; f < 10; ++f)
    {
        loop[f * 2] = static_cast<float>(f);
        loop[f * 2 + 1] = -static_cast<float>(f);
    }
    SliceLibrary library(10);
    library.extractTrack(loop, std::vector<Slice>{{0, 10}});

    const auto& info = library.sliceInfo(0, 0);
    EXPECT_FLOAT_EQ(info.peak, 9.f);
    EXPECT_GT(info.rms, 0.f);
    EXPECT_LT(info.rms, info.peak);
}

}
