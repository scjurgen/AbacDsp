#include <cmath>
#include <tuple>

#include "gtest/gtest.h"

#include "Analysis/OnsetDetector.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};

[[nodiscard]] bool fireWithin(OnsetDetector& detector, const int samples, const float level)
{
    for (int i = 0; i < samples; ++i)
    {
        if (detector.step(level))
        {
            return true;
        }
    }
    return false;
}
}

TEST(OnsetDetectorTest, SilenceNeverFires)
{
    OnsetDetector detector(kSampleRate);
    for (int i = 0; i < static_cast<int>(kSampleRate); ++i)
    {
        EXPECT_FALSE(detector.step(0.f));
    }
}

TEST(OnsetDetectorTest, LoudTransientFiresOnce)
{
    OnsetDetector detector(kSampleRate);
    int fireCount = 0;
    for (int i = 0; i < 4800; ++i)
    {
        if (detector.step(0.9f))
        {
            ++fireCount;
        }
    }
    EXPECT_EQ(fireCount, 1);
}

TEST(OnsetDetectorTest, LevelBelowUpperThresholdNeverFires)
{
    OnsetDetector detector(kSampleRate);
    EXPECT_FALSE(fireWithin(detector, static_cast<int>(kSampleRate), 0.07f));
}

TEST(OnsetDetectorTest, MinGapSuppressesASecondImmediateHit)
{
    OnsetDetector detector(kSampleRate);
    ASSERT_TRUE(fireWithin(detector, 10, 0.9f));
    EXPECT_FALSE(detector.step(0.9f));
}

TEST(OnsetDetectorTest, StaysDisarmedUntilEnvelopeDropsBelowLowerThreshold)
{
    OnsetDetector detector(kSampleRate);
    ASSERT_TRUE(fireWithin(detector, 10, 0.9f));
    // ripple stays inside the hysteresis band, so the detector never re-arms
    int fireCount = 0;
    for (int i = 0; i < static_cast<int>(kSampleRate); ++i)
    {
        const float ripple = 0.06f + 0.005f * std::sin(static_cast<float>(i) * 0.01f);
        if (detector.step(ripple))
        {
            ++fireCount;
        }
    }
    EXPECT_EQ(fireCount, 0);
}

TEST(OnsetDetectorTest, RearmsOnceEnvelopeDropsBelowLowerThreshold)
{
    OnsetDetector detector(kSampleRate);
    ASSERT_TRUE(fireWithin(detector, 10, 0.9f));
    EXPECT_FALSE(fireWithin(detector, static_cast<int>(kSampleRate), 0.f));
    EXPECT_TRUE(fireWithin(detector, 10, 0.9f));
}

TEST(OnsetDetectorTest, ResetClearsArmedStateAndEnvelope)
{
    OnsetDetector detector(kSampleRate);
    std::ignore = fireWithin(detector, 10, 0.9f);
    detector.reset();
    EXPECT_TRUE(fireWithin(detector, 10, 0.9f));
}

}
