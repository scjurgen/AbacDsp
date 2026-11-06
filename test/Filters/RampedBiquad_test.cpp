#include "gtest/gtest.h"

#include "Filters/RampedBiquad.h"

namespace AbacDsp::Test
{

namespace
{
const BiquadSection kA{0.2f, 0.1f, -0.05f, -0.9f, 0.3f};
const BiquadSection kB{0.6f, -0.2f, 0.1f, 0.4f, -0.2f};
} // namespace

TEST(RampedBiquad, reachesTheTargetExactlyAfterTheRamp)
{
    RampedBiquad filter;
    filter.setTarget(kA);
    EXPECT_EQ(filter.getCurrent().b0, kA.b0);
    filter.setRampSamples(16);
    filter.setTarget(kB);
    for (int i = 0; i < 15; ++i)
    {
        EXPECT_TRUE(filter.isRamping());
        static_cast<void>(filter.process(0.0f));
    }
    static_cast<void>(filter.process(0.0f));
    const BiquadSection& c = filter.getCurrent();
    EXPECT_FALSE(filter.isRamping());
    EXPECT_EQ(c.b0, kB.b0);
    EXPECT_EQ(c.b1, kB.b1);
    EXPECT_EQ(c.b2, kB.b2);
    EXPECT_EQ(c.a1, kB.a1);
    EXPECT_EQ(c.a2, kB.a2);
}

TEST(RampedBiquad, movesLinearlyAndRetargetsFromWhereItIs)
{
    RampedBiquad filter;
    filter.setTarget(kA);
    filter.setRampSamples(10);
    filter.setTarget(kB);
    for (int i = 0; i < 5; ++i)
    {
        static_cast<void>(filter.process(0.0f));
    }
    EXPECT_NEAR(filter.getCurrent().a1, 0.5f * (kA.a1 + kB.a1), 1e-6f);
    EXPECT_NEAR(filter.getCurrent().b0, 0.5f * (kA.b0 + kB.b0), 1e-6f);
    const float from = filter.getCurrent().a1;
    filter.setTarget(kA);
    static_cast<void>(filter.process(0.0f));
    EXPECT_NEAR(filter.getCurrent().a1, from + (kA.a1 - from) / 10.0f, 1e-6f);
    filter.snapToTarget();
    EXPECT_EQ(filter.getCurrent().a1, kA.a1);
}

TEST(RampedBiquad, theStateIsClearedByResetAndTheCoefficientsStay)
{
    RampedBiquad filter;
    filter.setTarget(kA);
    static_cast<void>(filter.process(1.0f));
    filter.reset();
    EXPECT_EQ(filter.getCurrent().a1, kA.a1);
    RampedBiquad fresh;
    fresh.setTarget(kA);
    EXPECT_EQ(fresh.process(0.5f), filter.process(0.5f));
}

TEST(RampedBiquad, aNewBiquadPassesTheInputThrough)
{
    RampedBiquad filter;
    for (int i = 0; i < 10; ++i)
    {
        const auto x = static_cast<float>(i) * 0.1f;
        EXPECT_EQ(x, filter.process(x));
    }
}

} // namespace AbacDsp::Test
