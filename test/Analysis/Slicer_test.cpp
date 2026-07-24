#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <utility>
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

namespace
{
// Deterministic broadband noise bursts over the given [start,end) frame ranges,
// silence elsewhere, so spectral flux spikes cleanly at each burst start.
[[nodiscard]] std::vector<float> noiseBursts(const size_t length, const std::vector<std::pair<size_t, size_t>>& ranges)
{
    std::vector<float> buf(length, 0.f);
    uint32_t state = 12345u;
    const auto next = [&state]() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 9) / 4194304.f - 1.f; // ~[-1, 1)
    };
    for (const auto& [a, b] : ranges)
    {
        for (size_t i = a; i < std::min(b, length); ++i)
        {
            buf[i] = 0.6f * next();
        }
    }
    return buf;
}

[[nodiscard]] size_t nearestDistance(const std::vector<size_t>& values, const size_t target)
{
    size_t best = std::numeric_limits<size_t>::max();
    for (const size_t v : values)
    {
        const size_t d = (v > target) ? v - target : target - v;
        best = std::min(best, d);
    }
    return best;
}
}

TEST(SlicerTest, SpectralFluxDetectsBurstOnsets)
{
    const auto mono = noiseBursts(8000, {{2000, 3000}, {5000, 6000}});
    Slicer::SpectralParams sp{};
    sp.fftSize = 1024;
    sp.hopSize = 256;
    sp.relativeThreshold = 0.3f;
    sp.minGapFrames = 1000;

    const auto onsets = Slicer::spectralFluxOnsets(mono, 8000, sp);

    ASSERT_FALSE(onsets.empty());
    EXPECT_LE(onsets.size(), 4u); // the two real onsets, not a spray of peaks
    // Detected onset is window-centred, so it should land near each burst start.
    EXPECT_LE(nearestDistance(onsets, 2000), 700u);
    EXPECT_LE(nearestDistance(onsets, 5000), 700u);
}

TEST(SlicerTest, SpectralFluxSilenceAndTooShort)
{
    const std::vector<float> silent(8000, 0.f);
    Slicer::SpectralParams sp{};
    EXPECT_TRUE(Slicer::spectralFluxOnsets(silent, 8000, sp).empty());

    const std::vector<float> tooShort(500, 0.5f); // shorter than fftSize
    EXPECT_TRUE(Slicer::spectralFluxOnsets(tooShort, 500, sp).empty());
}

TEST(SlicerTest, SpectralTransientSlicesCutAtOnsets)
{
    const auto mono = noiseBursts(8000, {{2000, 3000}, {5000, 6000}});
    Slicer::SpectralParams sp{};
    sp.minGapFrames = 1000;
    Slicer::TransientParams snap{}; // no grid / zero-cross snapping

    const auto slices = Slicer::spectralTransientSlices(mono, 8000, sp, snap);

    // Two onsets partition the loop into three slices (leading 0 implied).
    ASSERT_GE(slices.size(), 3u);
    EXPECT_EQ(slices.front().startFrame, 0u);
    // Slices tile the whole loop with no gaps.
    size_t covered = 0;
    for (const auto& s : slices)
    {
        EXPECT_EQ(s.startFrame, covered);
        covered += s.lengthFrames;
    }
    EXPECT_EQ(covered, 8000u);
}

namespace
{
// Broadband noise bursts with a per-range amplitude, so one burst can be made
// much quieter than another (a "ghost" note) while both spike the spectral flux.
[[nodiscard]] std::vector<float> scaledNoiseBursts(const size_t length,
                                                   const std::vector<std::tuple<size_t, size_t, float>>& ranges)
{
    std::vector<float> buf(length, 0.f);
    uint32_t state = 12345u;
    const auto next = [&state]() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 9) / 4194304.f - 1.f;
    };
    for (const auto& [a, b, amp] : ranges)
    {
        for (size_t i = a; i < std::min(b, length); ++i)
        {
            buf[i] = amp * next();
        }
    }
    return buf;
}
}

TEST(SlicerTest, AdaptiveFluxDetectsBurstOnsets)
{
    const auto mono = noiseBursts(8000, {{2000, 3000}, {5000, 6000}});
    Slicer::AdaptiveParams ap{};
    ap.minGapFrames = 1000;

    const auto onsets = Slicer::adaptiveFluxOnsets(mono, 8000, ap);

    ASSERT_FALSE(onsets.empty());
    EXPECT_LE(nearestDistance(onsets, 2000), 700u);
    EXPECT_LE(nearestDistance(onsets, 5000), 700u);
}

TEST(SlicerTest, AdaptiveFluxSilenceAndTooShort)
{
    const std::vector<float> silent(8000, 0.f);
    Slicer::AdaptiveParams ap{};
    EXPECT_TRUE(Slicer::adaptiveFluxOnsets(silent, 8000, ap).empty());

    const std::vector<float> tooShort(500, 0.5f); // shorter than fftSize
    EXPECT_TRUE(Slicer::adaptiveFluxOnsets(tooShort, 500, ap).empty());
}

// The point of the adaptive detector: a quiet onset below the global-threshold
// bar is still found because it stands out from its own silent neighbourhood.
TEST(SlicerTest, AdaptiveRecoversQuietOnsetThatGlobalThresholdMisses)
{
    const auto mono = scaledNoiseBursts(16000, {{2000, 3000, 0.6f}, {12000, 13000, 0.08f}});

    Slicer::SpectralParams sp{};
    sp.relativeThreshold = 0.3f;
    sp.minGapFrames = 1000;
    const auto global = Slicer::spectralFluxOnsets(mono, 16000, sp);
    EXPECT_GT(nearestDistance(global, 12000), 700u); // quiet burst missed globally

    Slicer::AdaptiveParams ap{};
    ap.minGapFrames = 1000;
    const auto adaptive = Slicer::adaptiveFluxOnsets(mono, 16000, ap);
    EXPECT_LE(nearestDistance(adaptive, 2000), 700u);
    EXPECT_LE(nearestDistance(adaptive, 12000), 700u); // recovered
}

TEST(SlicerTest, AdaptiveTransientSlicesTileTheLoop)
{
    const auto mono = noiseBursts(8000, {{2000, 3000}, {5000, 6000}});
    Slicer::AdaptiveParams ap{};
    ap.minGapFrames = 1000;
    Slicer::TransientParams snap{}; // no grid / zero-cross snapping

    const auto slices = Slicer::adaptiveTransientSlices(mono, 8000, ap, snap);

    ASSERT_GE(slices.size(), 2u);
    EXPECT_EQ(slices.front().startFrame, 0u);
    size_t covered = 0;
    for (const auto& s : slices)
    {
        EXPECT_EQ(s.startFrame, covered);
        covered += s.lengthFrames;
    }
    EXPECT_EQ(covered, 8000u);
}

// A coarse onset that sits late, inside the sustain, is pulled back onto the
// attack edge (the silence-to-burst transition at frame 4000).
TEST(SlicerTest, RefineSnapsLateOnsetTowardAttack)
{
    const auto mono = noiseBursts(8000, {{4000, 8000}});
    const std::vector<size_t> coarse = {4400}; // 400 samples late
    Slicer::RefineParams rp{};                 // tuned defaults

    const auto refined = Slicer::refineSliceStarts(coarse, mono, 8000, rp);

    ASSERT_EQ(refined.size(), 1u);
    EXPECT_LE(nearestDistance(refined, 4000), 600u);
    // improved on the coarse position and did not overshoot past it
    EXPECT_LT(refined[0], 4400u);
}

TEST(SlicerTest, RefineKeepsOnsetsSortedAndBounded)
{
    const auto mono = noiseBursts(12000, {{2000, 4000}, {6000, 8000}, {10000, 12000}});
    const std::vector<size_t> coarse = {2400, 6400, 10400};
    Slicer::RefineParams rp{};

    const auto refined = Slicer::refineSliceStarts(coarse, mono, 12000, rp);

    ASSERT_EQ(refined.size(), coarse.size());
    for (size_t i = 0; i < refined.size(); ++i)
    {
        if (i > 0)
        {
            EXPECT_LT(refined[i - 1], refined[i]); // sorted, de-duplicated
        }
        // stays within its own search window around the coarse onset
        EXPECT_GE(refined[i] + rp.prerollSamples, coarse[i]);
        EXPECT_LE(refined[i], coarse[i] + rp.postrollSamples);
    }
}

TEST(SlicerTest, RefineEmptyOnsetsYieldsEmpty)
{
    const std::vector<float> mono(4000, 0.5f);
    const std::vector<size_t> none;
    Slicer::RefineParams rp{};
    EXPECT_TRUE(Slicer::refineSliceStarts(none, mono, 4000, rp).empty());
}

}
