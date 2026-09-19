#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

#include "Delays/WobbleDelay.h"

namespace AbacDsp::Test
{

namespace
{

constexpr size_t kBuffer{8192};
constexpr size_t kTile{8};
constexpr float kRate{48000.f};
constexpr float kToneCyclesPerSample{0.01f};
constexpr float kToneAmplitude{0.5f};

using Sut = WobbleDelay<kBuffer, kTile>;

[[nodiscard]] std::vector<float> makeSine(const size_t numSamples, const float cyclesPerSample = kToneCyclesPerSample)
{
    std::vector<float> signal(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        signal[i] =
            kToneAmplitude * std::sin(2.f * std::numbers::pi_v<float> * cyclesPerSample * static_cast<float>(i));
    }
    return signal;
}

[[nodiscard]] std::vector<float> runThrough(Sut& sut, const std::vector<float>& input, const size_t blockSize = 64)
{
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += blockSize)
    {
        const auto count = std::min(blockSize, input.size() - pos);
        sut.processBlock(std::span<const float>{input}.subspan(pos, count),
                         std::span<float>{output}.subspan(pos, count));
    }
    return output;
}

struct DelayRange
{
    float min;
    float max;
};

// Steps the delay with silence and records the smallest and largest currentDelay() seen.
[[nodiscard]] DelayRange observeDelay(Sut& sut, const size_t numSamples)
{
    DelayRange range{std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()};
    for (size_t i = 0; i < numSamples; ++i)
    {
        static_cast<void>(sut.step(0.f));
        range.min = std::min(range.min, sut.currentDelay());
        range.max = std::max(range.max, sut.currentDelay());
    }
    return range;
}

void enableModulation(Sut& sut)
{
    sut.seed(1234);
    sut.setWowDepth(1.f);
    sut.setWowRate(1.f);
    sut.setWowVariance(0.5f);
    sut.setFlutterDepth(2.f);
    sut.setFlutterRate(8.f);
}

}

TEST(WobbleDelayTest, IntegerDelayIsExact)
{
    Sut sut(kRate);
    sut.setDelay(100.f, true);
    std::vector<float> input(400, 0.f);
    input[50] = 1.f;
    const auto output = runThrough(sut, input);

    for (size_t i = 0; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], i == 150 ? 1.f : 0.f, 1e-6f) << "at " << i;
    }
}

TEST(WobbleDelayTest, FractionalDelayMatchesAnalyticSine)
{
    Sut sut(kRate);
    sut.setDelay(20.5f, true);
    const auto input = makeSine(2000);
    const auto output = runThrough(sut, input);

    for (size_t i = 100; i < output.size(); ++i)
    {
        const auto expected = kToneAmplitude * std::sin(2.f * std::numbers::pi_v<float> * kToneCyclesPerSample *
                                                        (static_cast<float>(i) - 20.5f));
        ASSERT_NEAR(output[i], expected, 5e-4f) << "at " << i;
    }
}

TEST(WobbleDelayTest, UnmodulatedDelayStaysConstant)
{
    Sut sut(kRate);
    sut.setDelay(300.f, true);
    const auto range = observeDelay(sut, 50000);
    EXPECT_FLOAT_EQ(range.min, 300.f);
    EXPECT_FLOAT_EQ(range.max, 300.f);
}

TEST(WobbleDelayTest, ForcedDelayJumpsAtOnce)
{
    Sut sut(kRate);
    sut.setDelay(700.f, true);
    EXPECT_FLOAT_EQ(sut.currentDelay(), 700.f);
}

TEST(WobbleDelayTest, RetuneGlidesAtBoundedSpeed)
{
    Sut sut(kRate);
    sut.setDelay(200.f, true);
    sut.setDelay(1000.f);

    float previous = sut.currentDelay();
    size_t arrivedAt{0};
    for (size_t i = 1; i <= 4000; ++i)
    {
        static_cast<void>(sut.step(0.f));
        const auto now = sut.currentDelay();
        ASSERT_LE(std::abs(now - previous), Sut::kRetuneSlope + 1e-3f) << "at " << i;
        previous = now;
        if (arrivedAt == 0 && std::abs(now - 1000.f) < 1e-3f)
        {
            arrivedAt = i;
        }
    }
    EXPECT_FLOAT_EQ(sut.currentDelay(), 1000.f);
    EXPECT_GE(arrivedAt, 1500u);
    EXPECT_LE(arrivedAt, 1700u);
}

TEST(WobbleDelayTest, RetuneOutputStaysBounded)
{
    Sut sut(kRate);
    sut.setDelay(200.f, true);
    const auto input = makeSine(6000);
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += 64)
    {
        if (pos == 1024)
        {
            sut.setDelay(1500.f);
        }
        sut.processBlock(std::span<const float>{input}.subspan(pos, 64), std::span<float>{output}.subspan(pos, 64));
    }
    const auto maxAbs = std::abs(
        *std::ranges::max_element(output, [](const float a, const float b) { return std::abs(a) < std::abs(b); }));
    EXPECT_LT(maxAbs, kToneAmplitude * 1.2f);
}

TEST(WobbleDelayTest, WowMovesTheDelayAroundTheBase)
{
    Sut sut(kRate);
    sut.setDelay(1000.f, true);
    sut.seed(1234);
    sut.setWowDepth(1.f);
    sut.setWowRate(1.f);
    sut.setWowVariance(0.5f);
    const auto range = observeDelay(sut, 200000);

    EXPECT_GT(range.max - range.min, 10.f);
    EXPECT_LT(range.max - range.min, 400.f);
    EXPECT_LT(range.min, 1000.f);
    EXPECT_GT(range.max, 1000.f);
}

TEST(WobbleDelayTest, FlutterMovesTheDelayAroundTheBase)
{
    Sut sut(kRate);
    sut.setDelay(1000.f, true);
    sut.setFlutterDepth(2.f);
    sut.setFlutterRate(8.f);
    const auto range = observeDelay(sut, 100000);

    EXPECT_GT(range.max - range.min, 2.f);
    EXPECT_LT(range.min, 1000.f);
    EXPECT_GT(range.max, 1000.f);
}

TEST(WobbleDelayTest, ReadHeadNeverCrossesSafetyMargin)
{
    Sut sut(kRate);
    enableModulation(sut);
    sut.setFlutterDepth(5.f);
    sut.setSafetyMargin(20.f);
    sut.setDelay(25.f, true);
    const auto range = observeDelay(sut, 400000);

    EXPECT_GE(range.min, 20.f - 1e-3f);
}

TEST(WobbleDelayTest, ModulatedToneStaysFiniteAndBounded)
{
    Sut sut(kRate);
    enableModulation(sut);
    sut.setDelay(500.f, true);
    const auto output = runThrough(sut, makeSine(100000));

    EXPECT_TRUE(std::ranges::all_of(output, [](const float v) { return std::isfinite(v); }));
    EXPECT_LT(*std::ranges::max_element(output), kToneAmplitude * 1.2f);
    EXPECT_GT(*std::ranges::min_element(output), -kToneAmplitude * 1.2f);
}

TEST(WobbleDelayTest, OutputDoesNotDependOnBlockSize)
{
    const auto input = makeSine(20000);
    Sut reference(kRate);
    enableModulation(reference);
    reference.setDelay(400.f, true);
    const auto expected = runThrough(reference, input, 64);

    for (const size_t blockSize : {size_t{1}, size_t{5}, size_t{8}, size_t{100}})
    {
        Sut sut(kRate);
        enableModulation(sut);
        sut.setDelay(400.f, true);
        EXPECT_EQ(runThrough(sut, input, blockSize), expected) << "blockSize " << blockSize;
    }
}

TEST(WobbleDelayTest, ResetClearsTheDelayLine)
{
    Sut sut(kRate);
    sut.setDelay(50.f, true);
    static_cast<void>(runThrough(sut, makeSine(1000)));
    sut.reset();
    const auto output = runThrough(sut, std::vector<float>(200, 0.f));

    EXPECT_TRUE(std::ranges::all_of(output, [](const float v) { return v == 0.f; }));
}

}
