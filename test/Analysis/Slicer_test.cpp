#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "Analysis/Slicer.h"

namespace AbacDsp::test
{

namespace
{
// Places unit impulses at the given frames in an otherwise-silent mono buffer.
[[nodiscard]] std::vector<float> impulseTrain(const size_t length, const std::vector<size_t>& positions)
{
    std::vector<float> buffer(length, 0.f);
    for (const size_t p : positions)
    {
        if (p < length)
        {
            buffer[p] = 1.f;
        }
    }
    return buffer;
}
}

TEST(SlicerTest, GridSlicesEvenDivision)
{
    const auto slices = Slicer::gridSlices(1000, 4);
    ASSERT_EQ(slices.size(), 4u);
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(slices[i].startFrame, i * 250u);
        EXPECT_EQ(slices[i].lengthFrames, 250u);
    }
}

TEST(SlicerTest, GridSlicesDistributeRemainder)
{
    const auto slices = Slicer::gridSlices(1003, 4);
    ASSERT_EQ(slices.size(), 4u);
    // Slices tile the whole loop with no gap or overlap.
    size_t covered = 0;
    size_t expectedStart = 0;
    for (const auto& s : slices)
    {
        EXPECT_EQ(s.startFrame, expectedStart);
        expectedStart += s.lengthFrames;
        covered += s.lengthFrames;
    }
    EXPECT_EQ(covered, 1003u);
    EXPECT_EQ(expectedStart, 1003u);
}

TEST(SlicerTest, GridSlicesEmptyForZeroInputs)
{
    EXPECT_TRUE(Slicer::gridSlices(0, 4).empty());
    EXPECT_TRUE(Slicer::gridSlices(1000, 0).empty());
}

TEST(SlicerTest, GridBoundaries)
{
    const auto boundaries = Slicer::gridBoundaries(1000, 250);
    ASSERT_EQ(boundaries.size(), 4u);
    EXPECT_EQ(boundaries[0], 0u);
    EXPECT_EQ(boundaries[1], 250u);
    EXPECT_EQ(boundaries[2], 500u);
    EXPECT_EQ(boundaries[3], 750u);
}

TEST(SlicerTest, SlicesFromBoundariesImpliesLeadingZeroAndTrailingEnd)
{
    const std::vector<size_t> boundaries{250, 500, 750};
    const auto slices = Slicer::slicesFromBoundaries(boundaries, 1000);
    ASSERT_EQ(slices.size(), 4u);
    EXPECT_EQ(slices[0].startFrame, 0u);
    EXPECT_EQ(slices[0].lengthFrames, 250u);
    EXPECT_EQ(slices[3].startFrame, 750u);
    EXPECT_EQ(slices[3].lengthFrames, 250u);
}

TEST(SlicerTest, SlicesFromBoundariesDedupesAndSorts)
{
    const std::vector<size_t> boundaries{500, 250, 500, 0, 1000, 1200};
    const auto slices = Slicer::slicesFromBoundaries(boundaries, 1000);
    // Unique in-range starts: 0, 250, 500 -> 3 slices.
    ASSERT_EQ(slices.size(), 3u);
    EXPECT_EQ(slices[0].startFrame, 0u);
    EXPECT_EQ(slices[1].startFrame, 250u);
    EXPECT_EQ(slices[2].startFrame, 500u);
    EXPECT_EQ(slices[2].lengthFrames, 500u);
}

TEST(SlicerTest, DetectsOnsetsAtImpulses)
{
    const std::vector<size_t> positions{0, 200, 400, 600, 800};
    const auto mono = impulseTrain(1000, positions);
    Slicer::TransientParams params{};
    params.relativeThreshold = 0.5f;
    params.minGapFrames = 50;
    params.envelopeWindow = 1;

    const auto onsets = Slicer::detectOnsets(mono, 1000, params);
    ASSERT_EQ(onsets.size(), positions.size());
    for (size_t i = 0; i < positions.size(); ++i)
    {
        EXPECT_EQ(onsets[i], positions[i]);
    }
}

TEST(SlicerTest, MinGapRejectsCloseOnsets)
{
    const auto mono = impulseTrain(1000, {100, 110, 300});
    Slicer::TransientParams params{};
    params.relativeThreshold = 0.5f;
    params.minGapFrames = 50;
    params.envelopeWindow = 1;

    const auto onsets = Slicer::detectOnsets(mono, 1000, params);
    ASSERT_EQ(onsets.size(), 2u);
    EXPECT_EQ(onsets[0], 100u);
    EXPECT_EQ(onsets[1], 300u); // 110 rejected as within the gap
}

TEST(SlicerTest, TransientSlicesFromImpulses)
{
    const std::vector<size_t> positions{0, 250, 500, 750};
    const auto mono = impulseTrain(1000, positions);
    Slicer::TransientParams params{};
    params.relativeThreshold = 0.5f;
    params.minGapFrames = 50;
    params.envelopeWindow = 1;

    const auto slices = Slicer::transientSlices(mono, 1000, params);
    ASSERT_EQ(slices.size(), 4u);
    EXPECT_EQ(slices[0].startFrame, 0u);
    EXPECT_EQ(slices[1].startFrame, 250u);
    EXPECT_EQ(slices[2].startFrame, 500u);
    EXPECT_EQ(slices[3].startFrame, 750u);
}

TEST(SlicerTest, TransientOnsetsSnapToGrid)
{
    // Impulses slightly off the grid; snapping pulls them onto exact grid lines.
    const auto mono = impulseTrain(1000, {3, 248, 505, 752});
    Slicer::TransientParams params{};
    params.relativeThreshold = 0.5f;
    params.minGapFrames = 50;
    params.envelopeWindow = 1;
    params.snapMaxDistance = 20;

    const auto grid = Slicer::gridBoundaries(1000, 250); // 0,250,500,750
    const auto slices = Slicer::transientSlices(mono, 1000, params, grid);
    ASSERT_EQ(slices.size(), 4u);
    EXPECT_EQ(slices[0].startFrame, 0u);
    EXPECT_EQ(slices[1].startFrame, 250u);
    EXPECT_EQ(slices[2].startFrame, 500u);
    EXPECT_EQ(slices[3].startFrame, 750u);
}

TEST(SlicerTest, SnapMaxDistanceLeavesFarOnsetsAlone)
{
    const std::vector<size_t> boundaries{0, 250, 500, 750};
    // 380 is 120 from the nearest boundary (500); beyond maxDistance 50.
    EXPECT_EQ(Slicer::snapToNearest(380, boundaries, 50), 380u);
    // 260 is 10 from 250; within maxDistance.
    EXPECT_EQ(Slicer::snapToNearest(260, boundaries, 50), 250u);
}

TEST(SlicerTest, ZeroCrossingSnapMovesToSignChange)
{
    // A sine crosses zero at the midpoint; a boundary placed near a peak should
    // move toward the nearest zero crossing.
    const size_t length = 480;
    std::vector<float> mono(length, 0.f);
    const float freq = 100.f;
    const float sr = 48000.f;
    for (size_t i = 0; i < length; ++i)
    {
        mono[i] = std::sin(2.f * std::numbers::pi_v<float> * freq * static_cast<float>(i) / sr);
    }
    // One period is 480 samples; zero crossings near 0, 240. Peak near 120.
    const size_t snapped = Slicer::snapToZeroCrossing(mono, 120, 200, length);
    // Nearest sign change to 120 is around 240 or 0; both ~120 away. Accept either.
    const bool nearCrossing = (snapped <= 5) || (std::abs(static_cast<int>(snapped) - 240) <= 5);
    EXPECT_TRUE(nearCrossing) << "snapped=" << snapped;
    EXPECT_LT(std::abs(mono[snapped]), 0.05f);
}

TEST(SlicerTest, DownmixToMonoSumsChannels)
{
    std::vector<float> interleaved{1.f, 2.f, 3.f, 4.f, 5.f, 6.f}; // 3 frames
    std::vector<float> mono;
    Slicer::downmixToMono(interleaved, 3, mono);
    ASSERT_EQ(mono.size(), 3u);
    EXPECT_FLOAT_EQ(mono[0], 3.f);
    EXPECT_FLOAT_EQ(mono[1], 7.f);
    EXPECT_FLOAT_EQ(mono[2], 11.f);
}

TEST(SlicerTest, SilentLoopYieldsNoOnsets)
{
    const std::vector<float> mono(1000, 0.f);
    Slicer::TransientParams params{};
    params.relativeThreshold = 0.5f;
    const auto onsets = Slicer::detectOnsets(mono, 1000, params);
    EXPECT_TRUE(onsets.empty());
    // With no onsets, transient slicing still returns a single whole-loop slice.
    const auto slices = Slicer::transientSlices(mono, 1000, params);
    ASSERT_EQ(slices.size(), 1u);
    EXPECT_EQ(slices[0].startFrame, 0u);
    EXPECT_EQ(slices[0].lengthFrames, 1000u);
}

}
