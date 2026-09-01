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

namespace
{
Envelope<4> makeAdsrEnvelope(const float sampleRate)
{
    Envelope<4> env{sampleRate};
    env.setSegment(0, 10.f, 1.f, 0.f);
    env.setSegment(1, 200.f, 0.4f, 0.f);
    env.setSegmentHoldPreviousValue(2, 1000.f);
    env.markSustainAt(2);
    env.setSegment(3, 100.f, 0.f, 0.f);
    return env;
}
}

TEST(Envelope, emergencyReleaseJumpsToReleaseSegmentFromAnyState)
{
    auto env = makeAdsrEnvelope(kSampleRate);
    env.trigger();
    for (int i = 0; i < 100; ++i)
    {
        std::ignore = env.step();
    }
    ASSERT_EQ(env.currentSegmentIndex(), 0u);

    env.emergencyRelease(5.f);
    EXPECT_EQ(env.currentSegmentIndex(), 3u);
    const auto releaseSamples = static_cast<int>(5.f * kSampleRate / 1000.f);
    for (int i = 0; i < releaseSamples; ++i)
    {
        const float v = env.step();
        ASSERT_TRUE(std::isfinite(v));
    }
    EXPECT_NEAR(env.step(), 0.f, 1e-3f);
}

TEST(Envelope, quickModifyIfSegmentActiveOnlyAffectsTheCurrentSegment)
{
    auto env = makeAdsrEnvelope(kSampleRate);
    env.trigger();
    const auto attackSamples = static_cast<int>(10.f * kSampleRate / 1000.f);
    for (int i = 0; i < attackSamples; ++i)
    {
        std::ignore = env.step();
    }
    ASSERT_EQ(env.currentSegmentIndex(), 1u);

    env.quickModifyIfSegmentActive(0, 50); // segment 0 isn't active: must be a no-op
    for (int i = 0; i < 50; ++i)
    {
        std::ignore = env.step();
    }
    EXPECT_NE(env.step(), 0.4f) << "an inactive-segment call must not have retargeted the active segment";

    env.quickModifyIfSegmentActive(1, 10);
    for (int i = 0; i < 10; ++i)
    {
        std::ignore = env.step();
    }
    EXPECT_NEAR(env.step(), 0.4f, 1e-3f);
}

TEST(Envelope, modifyTargetIfActiveKeepsRemainingTimeButChangesDestination)
{
    auto env = makeAdsrEnvelope(kSampleRate);
    env.trigger();
    const auto attackSamples = static_cast<int>(10.f * kSampleRate / 1000.f);
    for (int i = 0; i < attackSamples; ++i)
    {
        std::ignore = env.step();
    }
    ASSERT_EQ(env.currentSegmentIndex(), 1u);

    env.setSegment(1, 200.f, 0.8f, 0.f);
    env.modifyTargetIfActive(1);

    // Stop just short of the decay segment's original 200ms duration: modifyTarget() keeps the
    // remaining sample count, so the ramp still lands here, one step before Envelope<N> would
    // advance into segment 2 (whose hold target is a separate, still-stale snapshot).
    const auto decaySamples = static_cast<int>(200.f * kSampleRate / 1000.f);
    float v = 0.f;
    for (int i = 0; i < decaySamples - 1; ++i)
    {
        v = env.step();
        ASSERT_TRUE(std::isfinite(v));
    }
    EXPECT_NEAR(v, 0.8f, 1e-3f);
    EXPECT_EQ(env.currentSegmentIndex(), 1u);
}

}
