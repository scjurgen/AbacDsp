#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "Spectral/PhaseVocoderPitcher.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};

[[nodiscard]] std::vector<float> generateSine(const float frequency, const size_t numSamples)
{
    std::vector<float> signal(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float t = static_cast<float>(i) / kSampleRate;
        signal[i] = std::sin(2.0f * std::numbers::pi_v<float> * frequency * t);
    }
    return signal;
}

[[nodiscard]] size_t countRisingZeroCrossings(const float* data, const size_t n)
{
    size_t count = 0;
    for (size_t i = 1; i < n; ++i)
    {
        if (data[i - 1] < 0.0f && data[i] >= 0.0f)
        {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] float measureFrequency(const float* data, const size_t n)
{
    const size_t crossings = countRisingZeroCrossings(data, n);
    return static_cast<float>(crossings) * kSampleRate / static_cast<float>(n);
}
}

TEST(PhaseVocoderPitcherTest, UnityRatioIsNearIdentityAfterLatency)
{
    PhaseVocoderPitcher pvp{kSampleRate};
    pvp.setPitchRatio(1.0f);

    constexpr size_t numSamples = 48000;
    const auto input = generateSine(440.0f, numSamples);
    std::vector<float> output(numSamples, 0.0f);
    pvp.processBlock(input.data(), output.data(), numSamples);

    const size_t latency = PhaseVocoderPitcher::latencySamples();
    ASSERT_LT(latency + 2000, numSamples);

    float maxErr = 0.0f;
    for (size_t i = latency + 2000; i < numSamples; ++i)
    {
        maxErr = std::max(maxErr, std::abs(output[i] - input[i - latency]));
    }
    EXPECT_LT(maxErr, 0.01f) << "Unity pitch ratio should reconstruct the delayed input almost exactly";
}

TEST(PhaseVocoderPitcherTest, OctaveUpDoublesFrequency)
{
    PhaseVocoderPitcher pvp{kSampleRate};
    pvp.setPitchRatio(2.0f);

    constexpr size_t numSamples = 48000;
    const auto input = generateSine(440.0f, numSamples);
    std::vector<float> output(numSamples, 0.0f);
    pvp.processBlock(input.data(), output.data(), numSamples);

    const size_t settleStart = PhaseVocoderPitcher::latencySamples() + 4000;
    ASSERT_LT(settleStart, numSamples);
    const float measured = measureFrequency(output.data() + settleStart, numSamples - settleStart);
    EXPECT_NEAR(measured, 880.0f, 20.0f);
}

TEST(PhaseVocoderPitcherTest, OctaveDownHalvesFrequency)
{
    PhaseVocoderPitcher pvp{kSampleRate};
    pvp.setPitchRatio(0.5f);

    constexpr size_t numSamples = 48000;
    const auto input = generateSine(440.0f, numSamples);
    std::vector<float> output(numSamples, 0.0f);
    pvp.processBlock(input.data(), output.data(), numSamples);

    const size_t settleStart = PhaseVocoderPitcher::latencySamples() + 4000;
    ASSERT_LT(settleStart, numSamples);
    const float measured = measureFrequency(output.data() + settleStart, numSamples - settleStart);
    EXPECT_NEAR(measured, 220.0f, 15.0f);
}

TEST(PhaseVocoderPitcherTest, LatencyIsFixedAndModest)
{
    // The whole point of the streaming rewrite: latency is a small, fixed
    // number of samples (one analysis window), not hundreds of milliseconds.
    const float latencyMs = static_cast<float>(PhaseVocoderPitcher::latencySamples()) / kSampleRate * 1000.0f;
    EXPECT_LT(latencyMs, 100.0f);
}

TEST(PhaseVocoderPitcherTest, StaysFiniteAndBoundedOverLongRun)
{
    PhaseVocoderPitcher pvp{kSampleRate};
    pvp.setPitchRatio(0.6674f);

    constexpr size_t numSamples = 4 * static_cast<size_t>(kSampleRate);
    const auto input = generateSine(220.0f, numSamples);
    std::vector<float> output(numSamples, 0.0f);
    pvp.processBlock(input.data(), output.data(), numSamples);

    float maxAbs = 0.0f;
    for (const auto sample : output)
    {
        ASSERT_TRUE(std::isfinite(sample));
        maxAbs = std::max(maxAbs, std::abs(sample));
    }
    EXPECT_GT(maxAbs, 0.1f);
    EXPECT_LE(maxAbs, 2.0f);
}

TEST(PhaseVocoderPitcherTest, ProcessingInSmallBlocksMatchesOneShot)
{
    PhaseVocoderPitcher blockwise{kSampleRate};
    PhaseVocoderPitcher oneShot{kSampleRate};
    blockwise.setPitchRatio(1.5f);
    oneShot.setPitchRatio(1.5f);

    constexpr size_t numSamples = 16000;
    constexpr size_t chunkSize = 17;
    const auto input = generateSine(300.0f, numSamples);

    std::vector<float> blockwiseOutput(numSamples, 0.0f);
    size_t produced = 0;
    while (produced < numSamples)
    {
        const size_t toRequest = std::min(chunkSize, numSamples - produced);
        blockwise.processBlock(input.data() + produced, blockwiseOutput.data() + produced, toRequest);
        produced += toRequest;
    }

    std::vector<float> oneShotOutput(numSamples, 0.0f);
    oneShot.processBlock(input.data(), oneShotOutput.data(), numSamples);

    for (size_t i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ(blockwiseOutput[i], oneShotOutput[i]) << "mismatch at sample " << i;
    }
}

}
