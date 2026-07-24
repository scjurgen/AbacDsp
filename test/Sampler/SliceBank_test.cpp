#include <gtest/gtest.h>
#include <vector>

#include "Analysis/Slicer.h"
#include "Sampler/SliceBank.h"

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

TEST(SliceBankTest, ExtractGridPartitionCopiesContiguously)
{
    const auto loop = makeRampLoop(100);
    const auto slices = Slicer::gridSlices(100, 4);
    SliceBank bank(100);
    bank.extract(loop, slices);

    EXPECT_EQ(bank.sliceCount(), 4u);
    EXPECT_EQ(bank.usedFrames(), 100u);

    // Bank offsets are back-to-back starting at 0.
    size_t expectedOffset = 0;
    for (size_t i = 0; i < bank.sliceCount(); ++i)
    {
        EXPECT_EQ(bank.bankSlices()[i].startFrame, expectedOffset);
        expectedOffset += bank.bankSlices()[i].lengthFrames;
    }
    EXPECT_EQ(expectedOffset, 100u);
}

TEST(SliceBankTest, ExtractedAudioMatchesSourceRegions)
{
    const auto loop = makeRampLoop(100);
    const auto slices = Slicer::gridSlices(100, 4);
    SliceBank bank(100);
    bank.extract(loop, slices);

    for (size_t i = 0; i < bank.sliceCount(); ++i)
    {
        const size_t srcStart = slices[i].startFrame;
        for (size_t f = 0; f < bank.bankSlices()[i].lengthFrames; ++f)
        {
            EXPECT_FLOAT_EQ(bank.sample(i, f, 0), static_cast<float>(srcStart + f));
            EXPECT_FLOAT_EQ(bank.sample(i, f, 1), -static_cast<float>(srcStart + f));
        }
    }
}

TEST(SliceBankTest, PoolViewIsContiguousAcrossSlices)
{
    const auto loop = makeRampLoop(60);
    const auto slices = Slicer::gridSlices(60, 3);
    SliceBank bank(60);
    bank.extract(loop, slices);

    // pool() addressed via each slice's bank offset reproduces the source.
    const std::span<const float> pool = bank.pool();
    ASSERT_EQ(pool.size(), 60u * 2u);
    for (size_t i = 0; i < bank.sliceCount(); ++i)
    {
        const Slice& b = bank.bankSlices()[i];
        const size_t srcStart = slices[i].startFrame;
        for (size_t f = 0; f < b.lengthFrames; ++f)
        {
            EXPECT_FLOAT_EQ(pool[(b.startFrame + f) * 2], static_cast<float>(srcStart + f));
        }
    }
}

TEST(SliceBankTest, NonContiguousSlicesPackTightlyButKeepContent)
{
    const auto loop = makeRampLoop(100);
    // Two disjoint slices with a gap between them (transient-style).
    const std::vector<Slice> slices{{10, 20}, {70, 15}};
    SliceBank bank(100);
    bank.extract(loop, slices);

    ASSERT_EQ(bank.sliceCount(), 2u);
    EXPECT_EQ(bank.usedFrames(), 35u);
    EXPECT_EQ(bank.bankSlices()[0].startFrame, 0u);
    EXPECT_EQ(bank.bankSlices()[1].startFrame, 20u); // packed right after the first

    EXPECT_FLOAT_EQ(bank.sample(0, 0, 0), 10.f);
    EXPECT_FLOAT_EQ(bank.sample(0, 19, 0), 29.f);
    EXPECT_FLOAT_EQ(bank.sample(1, 0, 0), 70.f);
    EXPECT_FLOAT_EQ(bank.sample(1, 14, 0), 84.f);
}

TEST(SliceBankTest, SliceLengthClampedToLoopEnd)
{
    const auto loop = makeRampLoop(50);
    const std::vector<Slice> slices{{40, 100}}; // runs past the loop end
    SliceBank bank(50);
    bank.extract(loop, slices);

    ASSERT_EQ(bank.sliceCount(), 1u);
    EXPECT_EQ(bank.bankSlices()[0].lengthFrames, 10u); // 50 - 40
}

TEST(SliceBankTest, PoolOverflowStopsExtraction)
{
    const auto loop = makeRampLoop(100);
    const auto slices = Slicer::gridSlices(100, 4); // 25 frames each
    SliceBank bank(60);                             // room for 2 slices + partial
    bank.extract(loop, slices);

    // Third slice (offset 50, len 25) would need 75 > 60, so extraction stops at 2.
    EXPECT_EQ(bank.sliceCount(), 2u);
    EXPECT_EQ(bank.usedFrames(), 50u);
}

TEST(SliceBankTest, ClearResets)
{
    const auto loop = makeRampLoop(40);
    SliceBank bank(40);
    bank.extract(loop, Slicer::gridSlices(40, 2));
    ASSERT_EQ(bank.sliceCount(), 2u);

    bank.clear();
    EXPECT_EQ(bank.sliceCount(), 0u);
    EXPECT_EQ(bank.usedFrames(), 0u);
    EXPECT_TRUE(bank.pool().empty());
}

TEST(SliceBankTest, EmptyInputsProduceEmptyBank)
{
    SliceBank bank(100);
    bank.extract({}, {});
    EXPECT_EQ(bank.sliceCount(), 0u);
    EXPECT_EQ(bank.usedFrames(), 0u);

    const auto loop = makeRampLoop(50);
    bank.extract(loop, {}); // audio but no slices
    EXPECT_EQ(bank.sliceCount(), 0u);
}

TEST(SliceBankTest, SkipsSlicesStartingBeyondLoop)
{
    const auto loop = makeRampLoop(30);
    const std::vector<Slice> slices{{10, 5}, {40, 5}, {20, 5}};
    SliceBank bank(30);
    bank.extract(loop, slices);

    // The middle slice starts past the loop end and is skipped; the other two remain.
    ASSERT_EQ(bank.sliceCount(), 2u);
    EXPECT_FLOAT_EQ(bank.sample(0, 0, 0), 10.f);
    EXPECT_FLOAT_EQ(bank.sample(1, 0, 0), 20.f);
}

}
