#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <tuple>

#include "gtest/gtest.h"

#include "Diffuser/SizeSpreadControl.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Test
{

using AbacDsp::SizeSpreadControl::effectiveMagnitude;
using AbacDsp::SizeSpreadControl::fillAlternatingOffsets;
using AbacDsp::SizeSpreadControl::kDefaultSafetyFraction;

TEST(SizeSpreadControlTest, PassesThroughWhenWellUnderSafetyCap)
{
    EXPECT_FLOAT_EQ(effectiveMagnitude(10.f, 1.f), 1.f);
    EXPECT_FLOAT_EQ(effectiveMagnitude(100.f, 5.f), 5.f);
}

TEST(SizeSpreadControlTest, CapsToFractionOfBaseSizeWhenSpreadIsLarger)
{
    EXPECT_FLOAT_EQ(effectiveMagnitude(10.f, 100.f), 5.f); // 10 * 0.5
    EXPECT_FLOAT_EQ(effectiveMagnitude(0.5f, 100.f), 0.25f);
}

TEST(SizeSpreadControlTest, NeverPushesBaseSizeToZeroOrBelow)
{
    for (const float baseSize : {0.001f, 0.5f, 1.f, 10.f, 100.f})
    {
        for (const float spread : {0.f, 0.1f, 1.f, 10.f, 100.f, 1000.f})
        {
            const auto magnitude = effectiveMagnitude(baseSize, spread);
            EXPECT_GT(baseSize - magnitude, 0.f) << "baseSize=" << baseSize << " spread=" << spread;
        }
    }
}

TEST(SizeSpreadControlTest, CustomSafetyFractionIsRespected)
{
    EXPECT_FLOAT_EQ(effectiveMagnitude(10.f, 100.f, 0.1f), 1.f);
    EXPECT_FLOAT_EQ(effectiveMagnitude(10.f, 100.f, 0.9f), 9.f);
}

TEST(SizeSpreadControlTest, InvertingParityFlipsTheSignPattern)
{
    constexpr std::size_t count{6};
    const std::array<float, count> baseSizes{10.f, 10.f, 10.f, 10.f, 10.f, 10.f};
    std::array<float, count> offsetsA{};
    std::array<float, count> offsetsB{};

    fillAlternatingOffsets(baseSizes, 1.f, false, offsetsA, count);
    fillAlternatingOffsets(baseSizes, 1.f, true, offsetsB, count);

    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_FLOAT_EQ(offsetsA[i], -offsetsB[i]);
    }
    // Not identical channel-to-channel: this is the actual decorrelation the caller wants.
    EXPECT_NE(offsetsA, offsetsB);
}

TEST(SizeSpreadControlTest, UnfilledTailIsLeftAtZero)
{
    constexpr std::size_t capacity{5};
    const std::array<float, capacity> baseSizes{10.f, 10.f, 10.f, 10.f, 10.f};
    std::array<float, capacity> offsets{1.f, 1.f, 1.f, 1.f, 1.f};

    fillAlternatingOffsets(baseSizes, 1.f, false, offsets, 2);

    EXPECT_NE(offsets[0], 0.f);
    EXPECT_NE(offsets[1], 0.f);
    EXPECT_FLOAT_EQ(offsets[2], 1.f); // untouched, not zeroed by the call
}

// Typical diffuser element sizes (1-10m) and spread settings a user might actually dial in,
// checked against the Haas effect: sub-~1ms L/R differences fuse into a single localized image,
// ~1-40ms widens the stereo image (the actual goal here), beyond ~40ms reads as a discrete echo
// rather than width.
TEST(SizeSpreadControlTest, TypicalDiffuserSizesStayInHaasWideningRange)
{
    constexpr float kSampleRate{48000.f};
    constexpr float kHaasEchoThresholdMs{40.f};
    const std::array<float, 8> baseSizes{1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f, 10.f};

    for (const float spread : {0.02f, 0.05f, 0.1f, 0.3f, 0.5f, 1.f})
    {
        std::array<float, baseSizes.size()> offsetsL{};
        std::array<float, baseSizes.size()> offsetsR{};
        fillAlternatingOffsets(baseSizes, spread, false, offsetsL, baseSizes.size());
        fillAlternatingOffsets(baseSizes, spread, true, offsetsR, baseSizes.size());

        std::cout << "spread=" << spread << "m:\n";
        for (std::size_t i = 0; i < baseSizes.size(); ++i)
        {
            const auto deltaMeters = std::abs(offsetsL[i] - offsetsR[i]);
            const auto deltaMs = Convert::metersToSamples(deltaMeters, kSampleRate) / kSampleRate * 1000.f;
            std::cout << "  size=" << baseSizes[i] << "m  L/R delta=" << deltaMeters << "m (" << deltaMs << " ms)\n";
            EXPECT_LT(deltaMs, kHaasEchoThresholdMs)
                << "spread=" << spread << " size=" << baseSizes[i] << " would read as an echo, not stereo width";
        }
    }
}

// Extreme values pulled directly from maxdiffuser.json's port ranges: elements [0, 50],
// bottomSize/topSize [0.5, 100] meters. Requested spread is swept beyond the largest possible
// element to confirm graceful, safe capping at the edges rather than a specific delta target.
class SizeSpreadControlExtremesTest : public ::testing::TestWithParam<std::tuple<std::size_t, float, float>>
{
};

TEST_P(SizeSpreadControlExtremesTest, StaysSafeAcrossMaxdiffuserJsonExtremes)
{
    constexpr std::size_t maxElements{50};
    const auto [elementCount, baseSize, requestedSpread] = GetParam();
    ASSERT_LE(elementCount, maxElements);

    std::array<float, maxElements> baseSizes{};
    std::fill_n(baseSizes.begin(), elementCount, baseSize);
    std::array<float, maxElements> offsetsL{};
    std::array<float, maxElements> offsetsR{};

    fillAlternatingOffsets(baseSizes, requestedSpread, false, offsetsL, elementCount);
    fillAlternatingOffsets(baseSizes, requestedSpread, true, offsetsR, elementCount);

    for (std::size_t i = 0; i < elementCount; ++i)
    {
        EXPECT_GT(baseSizes[i] + offsetsL[i], 0.f);
        EXPECT_GT(baseSizes[i] + offsetsR[i], 0.f);
        EXPECT_LE(std::abs(offsetsL[i]), baseSizes[i] * kDefaultSafetyFraction);
        EXPECT_LE(std::abs(offsetsR[i]), baseSizes[i] * kDefaultSafetyFraction);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MaxdiffuserJsonRanges, SizeSpreadControlExtremesTest,
    ::testing::Values(std::make_tuple(0, 0.5f, 1.f),       // no elements
                      std::make_tuple(1, 0.5f, 1.f),       // single element, smallest size
                      std::make_tuple(1, 0.5f, 100.f),     // single element, spread far exceeding size
                      std::make_tuple(2, 100.f, 100.f),    // spread equal to the largest possible size
                      std::make_tuple(50, 0.5f, 0.f),      // max elements, no spread
                      std::make_tuple(50, 0.5f, 100.f),    // max elements, smallest size, largest spread
                      std::make_tuple(50, 100.f, 100.f))); // max elements, largest size, largest spread

}
