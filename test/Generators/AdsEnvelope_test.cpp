#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

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

namespace
{
constexpr float kSegmentMs = 100.f;

/// Runs a one-segment envelope from `start` to `target` and returns the value after each step of the segment.
std::vector<float> runSegment(const float start, const float target, const float curve, const SegmentShape shape)
{
    Envelope<2> env{kSampleRate};
    env.setSegment(0, kSegmentMs, target, curve, shape);
    env.setSegmentHoldPreviousValue(1, 0.f);
    env.markSustainAt(1);
    env.triggerFrom(start);
    const auto samples = static_cast<size_t>(kSegmentMs * kSampleRate / 1000.f);
    std::vector<float> values;
    values.reserve(samples);
    for (size_t i = 0; i < samples; ++i)
    {
        values.push_back(env.step());
    }
    return values;
}
}

TEST(ExponentialApproach, followsTheClosedFormAndLandsOnTheTarget)
{
    for (const float k : {-3.f, 0.5f, 1.f, 3.f, 5.f, 7.f})
    {
        ExponentialApproach curve;
        curve.setTimeConstants(k);
        EXPECT_FLOAT_EQ(curve.get(0.f), 0.f) << "k=" << k;
        EXPECT_FLOAT_EQ(curve.get(1.f), 1.f) << "k=" << k;
        for (const float x : {0.1f, 0.25f, 0.5f, 0.75f})
        {
            const double expected =
                (1.0 - std::exp(-static_cast<double>(k) * x)) / (1.0 - std::exp(-static_cast<double>(k)));
            EXPECT_NEAR(curve.get(x), static_cast<float>(expected), 1e-6f) << "k=" << k << " x=" << x;
        }
    }
}

TEST(ExponentialApproach, zeroTimeConstantsIsAStraightLine)
{
    ExponentialApproach curve;
    curve.setTimeConstants(0.f);
    for (const float x : {0.f, 0.1f, 0.5f, 1.f})
    {
        EXPECT_FLOAT_EQ(curve.get(x), x);
    }
    curve.setTimeConstants(5e-5f);
    EXPECT_FLOAT_EQ(curve.get(0.3f), 0.3f) << "below the threshold the curve is straight";
}

TEST(ExponentialApproach, positiveStartsFastNegativeStartsSlowAndBothStayWithinTheSegment)
{
    ExponentialApproach fast;
    fast.setTimeConstants(5.f);
    ExponentialApproach slow;
    slow.setTimeConstants(-5.f);
    float lastFast = 0.f;
    float lastSlow = 0.f;
    for (int i = 1; i <= 1000; ++i)
    {
        const float x = static_cast<float>(i) / 1000.f;
        EXPECT_GT(fast.get(x), x - 1e-6f);
        EXPECT_LT(slow.get(x), x + 1e-6f);
        EXPECT_GE(fast.get(x), lastFast);
        EXPECT_GE(slow.get(x), lastSlow);
        EXPECT_LE(fast.get(x), 1.f);
        EXPECT_LE(slow.get(x), 1.f);
        lastFast = fast.get(x);
        lastSlow = slow.get(x);
    }
}

TEST(ExponentialApproach, extremeValuesStayFiniteAndLandOnTheTarget)
{
    for (const float k : {1e9f, -1e9f, 200.f, -200.f})
    {
        ExponentialApproach curve;
        curve.setTimeConstants(k);
        for (const float x : {0.f, 1e-6f, 0.5f, 1.f, 1.0000001f})
        {
            ASSERT_TRUE(std::isfinite(curve.get(x))) << "k=" << k << " x=" << x;
        }
        EXPECT_NEAR(curve.get(1.f), 1.f, 1e-5f) << "k=" << k;
    }
}

TEST(Envelope, exponentialSegmentFollowsTheClosedFormBetweenStartAndTarget)
{
    constexpr float start = 48.f;
    constexpr float target = 84.f;
    constexpr float k = 4.f;
    const auto values = runSegment(start, target, k, SegmentShape::Exponential);
    const auto n = static_cast<double>(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        const double x = static_cast<double>(i + 1) / n;
        const double expected = static_cast<double>(start) + (1.0 - std::exp(-static_cast<double>(k) * x)) /
                                                                 (1.0 - std::exp(-static_cast<double>(k))) *
                                                                 static_cast<double>(target - start);
        ASSERT_NEAR(values[i], static_cast<float>(expected), 1e-3f) << "step " << i;
    }
    EXPECT_NEAR(values.back(), target, 1e-3f);
}

TEST(Envelope, exponentialSegmentFallingToALowerTargetNeverUndershootsIt)
{
    const auto values = runSegment(84.f, 52.f, 5.f, SegmentShape::Exponential);
    float last = 84.f;
    for (const float v : values)
    {
        ASSERT_LE(v, last + 1e-4f);
        ASSERT_GE(v, 52.f - 1e-3f);
        last = v;
    }
    EXPECT_NEAR(values.back(), 52.f, 1e-3f);
}

TEST(Envelope, segmentShapeDefaultsToPolynomial)
{
    for (const float curve : {-1.f, 0.f, 0.5f})
    {
        const auto implicitShape = runSegment(0.f, 1.f, curve, SegmentShape::Polynomial);
        Envelope<2> env{kSampleRate};
        env.setSegment(0, kSegmentMs, 1.f, curve);
        env.setSegmentHoldPreviousValue(1, 0.f);
        env.markSustainAt(1);
        env.triggerFrom(0.f);
        for (const float v : implicitShape)
        {
            ASSERT_EQ(env.step(), v) << "curve=" << curve;
        }
    }
}

TEST(Envelope, exponentialAttackDecayWithSustainRunsInNotesAndRetriggersFromTheStart)
{
    Envelope<3> env{kSampleRate};
    env.setSegment(0, 5.f, 84.f, 0.f, SegmentShape::Exponential);
    env.setSegment(1, 400.f, 52.f, 5.f, SegmentShape::Exponential);
    env.setSegmentHoldPreviousValue(2, 0.f);
    env.markSustainAt(2);
    env.triggerFrom(48.f);

    float peak = 0.f;
    float low = 1000.f;
    for (int i = 0; i < static_cast<int>(0.5f * kSampleRate); ++i)
    {
        const float v = env.step();
        peak = std::max(peak, v);
        low = std::min(low, v);
    }
    EXPECT_NEAR(peak, 84.f, 1e-2f);
    EXPECT_GE(low, 48.f - 1e-3f);
    EXPECT_NEAR(env.step(), 52.f, 1e-3f) << "the sustain value holds";

    env.triggerFrom(48.f);
    EXPECT_NEAR(env.step(), 48.f, 0.5f) << "a retrigger starts from the start value, not from the sustain";
    EXPECT_EQ(env.currentSegmentIndex(), 0u);
}

}
