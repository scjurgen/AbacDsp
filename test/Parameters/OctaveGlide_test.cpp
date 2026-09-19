#include <cmath>
#include <gtest/gtest.h>

#include "Parameters/OctaveGlide.h"

namespace AbacDsp::Test
{

namespace
{

constexpr float kRate{48000.f};
constexpr float kArrivalTolerance{16.f};

// Samples until the glide reaches its target, counted one sample at a time.
[[nodiscard]] size_t samplesToReach(OctaveGlide& glide, const float target)
{
    size_t count{0};
    while (std::abs(glide.getValue() - target) > 1e-4f && count < 1000000)
    {
        ++count;
    }
    return count;
}

}

TEST(OctaveGlideTest, OneOctaveUpTakesOneOverTheAccelRateWhateverTheStart)
{
    for (const float start : {0.25f, 1.f, 3.f})
    {
        OctaveGlide glide(kRate, start);
        glide.setTarget(start * 2.f);
        const auto expected = kRate / OctaveGlide::kAccelOctavesPerSec;
        EXPECT_NEAR(static_cast<float>(samplesToReach(glide, start * 2.f)), expected, kArrivalTolerance)
            << "start " << start;
    }
}

TEST(OctaveGlideTest, OneOctaveDownIsSlowerThanUp)
{
    OctaveGlide glide(kRate, 2.f);
    glide.setTarget(1.f);
    const auto expected = kRate / OctaveGlide::kBrakeOctavesPerSec;
    EXPECT_NEAR(static_cast<float>(samplesToReach(glide, 1.f)), expected, kArrivalTolerance);
    EXPECT_GT(OctaveGlide::kAccelOctavesPerSec, OctaveGlide::kBrakeOctavesPerSec);
}

TEST(OctaveGlideTest, ForcedTargetIsReachedAtOnce)
{
    OctaveGlide glide(kRate);
    glide.setTarget(4.f, true);
    EXPECT_FLOAT_EQ(glide.getValue(), 4.f);
}

TEST(OctaveGlideTest, BlockwiseAdvanceMatchesSamplewise)
{
    OctaveGlide blockwise(kRate);
    OctaveGlide samplewise(kRate);
    blockwise.setTarget(2.f);
    samplewise.setTarget(2.f);
    float last{0.f};
    for (size_t i = 0; i < 100; ++i)
    {
        last = blockwise.getValue(8);
        for (size_t s = 0; s < 8; ++s)
        {
            static_cast<void>(samplewise.getValue());
        }
    }
    EXPECT_NEAR(last, samplewise.getValue(0), 1e-4f);
}

}
