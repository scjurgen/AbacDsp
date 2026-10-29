#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>

#include "gtest/gtest.h"

#include "Analysis/LevelMeter.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kBlockSize{16};

void feedConstant(LevelMeter& meter, const float value, const float seconds)
{
    std::array<float, kBlockSize> block{};
    block.fill(value);
    const auto blocks = static_cast<size_t>(seconds * kSampleRate) / kBlockSize;
    for (size_t i = 0; i < blocks; ++i)
    {
        meter.process(block);
    }
}

void feedSine(LevelMeter& meter, const float amplitude, const float frequency, const float seconds)
{
    std::array<float, kBlockSize> block{};
    const auto blocks = static_cast<size_t>(seconds * kSampleRate) / kBlockSize;
    size_t n = 0;
    for (size_t b = 0; b < blocks; ++b)
    {
        for (auto& sample : block)
        {
            sample = amplitude *
                     std::sin(2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(n++) / kSampleRate);
        }
        meter.process(block);
    }
}
}

TEST(LevelMeterTest, SilenceReadsFloor)
{
    LevelMeter meter(kSampleRate);
    feedConstant(meter, 0.f, 0.5f);
    EXPECT_FLOAT_EQ(meter.peakDb(), LevelMeter::kFloorDb);
    EXPECT_FLOAT_EQ(meter.rmsDb(), LevelMeter::kFloorDb);
}

TEST(LevelMeterTest, FullScaleSineRmsIsMinus3Db)
{
    LevelMeter meter(kSampleRate);
    feedSine(meter, 1.f, 1000.f, 0.5f);
    EXPECT_NEAR(meter.rmsDb(), -3.0103f, 0.02f);
    EXPECT_NEAR(meter.peakDb(), 0.f, 0.01f);
}

TEST(LevelMeterTest, PeakRisesInstantly)
{
    LevelMeter meter(kSampleRate);
    std::array<float, kBlockSize> block{};
    block[kBlockSize - 1] = 0.5f;
    meter.process(block);
    EXPECT_NEAR(meter.peakDb(), -6.0206f, 0.001f);
}

TEST(LevelMeterTest, PeakFallsAtTheConfiguredRate)
{
    for (const float rate : {6.f, 11.8f, 24.f, 48.f})
    {
        LevelMeter meter(kSampleRate);
        meter.setPeakFallDbPerSecond(rate);
        feedConstant(meter, 1.f, 0.1f);
        feedConstant(meter, 0.f, 1.f);
        EXPECT_NEAR(meter.peakDb(), -rate, 0.01f) << "rate " << rate;
    }
}

TEST(LevelMeterTest, HoldKeepsMaximumForHoldTimeThenFollowsPeak)
{
    LevelMeter meter(kSampleRate);
    meter.setPeakFallDbPerSecond(10.f);
    meter.setPeakHoldSeconds(1.f);
    feedConstant(meter, 1.f, 0.1f);
    feedConstant(meter, 0.f, 0.9f);
    EXPECT_FLOAT_EQ(meter.holdDb(), 0.f);
    EXPECT_NEAR(meter.peakDb(), -9.f, 0.01f);

    feedConstant(meter, 0.f, 0.2f);
    EXPECT_FLOAT_EQ(meter.holdDb(), meter.peakDb());
}

TEST(LevelMeterTest, HoldOffTracksPeak)
{
    LevelMeter meter(kSampleRate);
    feedConstant(meter, 1.f, 0.1f);
    feedConstant(meter, 0.f, 0.5f);
    EXPECT_FLOAT_EQ(meter.holdDb(), meter.peakDb());
}

TEST(LevelMeterTest, InfiniteHoldLastsUntilReset)
{
    LevelMeter meter(kSampleRate);
    meter.setPeakHoldSeconds(std::numeric_limits<float>::infinity());
    feedConstant(meter, 0.5f, 0.1f);
    feedConstant(meter, 0.f, 20.f);
    EXPECT_NEAR(meter.holdDb(), -6.0206f, 0.001f);

    meter.reset();
    EXPECT_FLOAT_EQ(meter.holdDb(), meter.peakDb());
}

TEST(LevelMeterTest, ClipLatchesUntilReset)
{
    LevelMeter meter(kSampleRate);
    feedConstant(meter, 0.99f, 0.1f);
    EXPECT_FALSE(meter.isClipped());
    feedConstant(meter, 1.f, 0.01f);
    feedConstant(meter, 0.f, 1.f);
    EXPECT_TRUE(meter.isClipped());
    meter.reset();
    EXPECT_FALSE(meter.isClipped());
}

TEST(LevelMeterTest, RmsWindowSetsIntegrationTime)
{
    LevelMeter meter(kSampleRate);
    meter.setRmsWindowSeconds(0.05f);
    feedConstant(meter, 1.f, 0.1f);
    feedConstant(meter, 0.f, 0.025f);
    EXPECT_NEAR(meter.rmsDb(), 20.f * std::log10(std::sqrt(0.5f)), 0.1f);
    feedConstant(meter, 0.f, 0.03f);
    EXPECT_FLOAT_EQ(meter.rmsDb(), LevelMeter::kFloorDb);
}

}
