#include <cmath>
#include <tuple>

#include "gtest/gtest.h"

#include "Generators/AdsEnvelope.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;
}

TEST(AdsEnvelope, risesThroughAttackThenDecaysToSustainAndHolds)
{
    AdsEnvelope env{kSampleRate};
    env.setAttackDecaySustain(10.f, 200.f, 0.4f);
    env.trigger();

    const auto attackSamples = static_cast<int>(10.f * kSampleRate / 1000.f);
    for (int i = 0; i < attackSamples; ++i)
    {
        std::ignore = env.step();
    }
    EXPECT_NEAR(env.step(), 1.f, 0.01f);

    for (int i = 0; i < 20000; ++i)
    {
        std::ignore = env.step();
    }
    EXPECT_NEAR(env.step(), 0.4f, 1e-4f);
    EXPECT_EQ(env.currentSegmentIndex(), 2u);
    EXPECT_FALSE(env.isDone());
}

TEST(AdsEnvelope, outputStaysFiniteAndBoundedThroughout)
{
    AdsEnvelope env{kSampleRate};
    env.setAttackDecaySustain(5.f, 50.f, 0.7f);
    env.trigger();
    for (int i = 0; i < 10000; ++i)
    {
        const float v = env.step();
        ASSERT_TRUE(std::isfinite(v));
        ASSERT_GE(v, -0.01f);
        ASSERT_LE(v, 1.1f);
    }
}

TEST(AdsEnvelope, retriggerMidEnvelopeContinuesFromCurrentGainWithoutJumpingToZero)
{
    AdsEnvelope env{kSampleRate};
    env.setAttackDecaySustain(10.f, 200.f, 0.4f);
    env.trigger();
    for (int i = 0; i < 20000; ++i)
    {
        std::ignore = env.step();
    }
    const float beforeRetrigger = env.step();
    EXPECT_NEAR(beforeRetrigger, 0.4f, 1e-3f);

    env.trigger();
    const float justAfter = env.step();
    EXPECT_NEAR(justAfter, beforeRetrigger, 0.02f);
    EXPECT_EQ(env.currentSegmentIndex(), 0u);
}

TEST(AdsEnvelope, zeroTimeSegmentsJumpImmediately)
{
    AdsEnvelope env{kSampleRate};
    env.setAttackDecaySustain(0.f, 0.f, 0.6f);
    env.trigger();
    std::ignore = env.step(); // consumes the zero-length attack segment, transitioning into decay
    EXPECT_NEAR(env.step(), 0.6f, 1e-4f);
    EXPECT_NEAR(env.step(), 0.6f, 1e-4f);
}

TEST(AdsEnvelope, curveFactorStaysBoundedAndReachesTarget)
{
    for (const float curve : {-1.f, -0.5f, 0.f, 0.5f, 1.f})
    {
        AdsEnvelope env{kSampleRate};
        env.setAttackDecaySustain(20.f, 20.f, 0.3f, curve);
        env.trigger();
        for (int i = 0; i < 5000; ++i)
        {
            const float v = env.step();
            ASSERT_TRUE(std::isfinite(v)) << "curve=" << curve;
        }
        EXPECT_NEAR(env.step(), 0.3f, 1e-3f) << "curve=" << curve;
    }
}

}
