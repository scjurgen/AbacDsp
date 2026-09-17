#include <array>
#include <cmath>
#include <memory>
#include <random>

#include "gtest/gtest.h"

#include "Audio/AudioBuffer.h"
#include "Delays/OrganicChorusTransport.h"
#include "Filters/Sinc/sinc_4.h"
#include "impl/PathfinderImpl.h"

namespace
{

constexpr size_t BlockSize{16};
constexpr float SampleRate{48000.f};

using Impl = PathfinderImpl<BlockSize>;

// Mirrors organicchorus's own safety-margin test: reports the peak read/write distance
// deviation from the target delay over a multi-minute session at full Depth/Aggressivity.
float measurePeakDriftSamples(const float speedHz, const size_t numBlocks)
{
    Impl::Transport transport{SampleRate, std::make_shared<AbacDsp::SincFilter>(sinc4)};
    const float targetDistance = Impl::kBaseDelayMs * 0.001f * SampleRate;
    transport.setReadHeadSafetyMargin(Impl::kSafetyMarginSamples);
    transport.setReadHead(0, targetDistance, true);
    transport.setReadHeadCorrectionThreshold(0, Impl::kCorrectionThresholdSamples);
    transport.setWowRate(speedHz);
    transport.setWowDepth(0.45f);
    transport.setWowVariance(0.6f);
    transport.setWowDrift(0.6f);
    transport.setFlutterRate(std::max(speedHz, Impl::kFlutterRateFloorHz));
    transport.setFlutterDepth(0.5f);
    transport.setRatio(1.f, true);

    float peak = 0.f;
    const std::array<float, 2 * BlockSize> silence{};
    for (size_t b = 0; b < numBlocks; ++b)
    {
        transport.feed(silence);
        std::array<float, 2 * BlockSize> discard{};
        transport.readBlock(0, discard);

        auto delta = static_cast<double>(transport.writeHead()) - transport.readHead(0);
        while (delta < 0.0)
        {
            delta += Impl::kBufferSize;
        }
        while (delta >= Impl::kBufferSize)
        {
            delta -= Impl::kBufferSize;
        }
        peak = std::max(peak, static_cast<float>(std::abs(delta - static_cast<double>(targetDistance))));
    }
    return peak;
}

// A coarse sweep once found a worse peak at an untested rate in between two tested points
// on organicchorus's own transport (see its own test's history) - sweep finely enough here
// to actually find the worst case rather than assuming it is monotonic.
TEST(PathfinderSafetyTest, EveryReachableSpeedStaysWithinConfiguredSafetyMargin)
{
    constexpr size_t numBlocks = static_cast<size_t>(60 * SampleRate / BlockSize);
    constexpr int kSteps = 15;
    for (int i = 0; i <= kSteps; ++i)
    {
        const float speedHz = 0.05f + static_cast<float>(i) / kSteps * (6.0f - 0.05f);
        const auto peak = measurePeakDriftSamples(speedHz, numBlocks);
        EXPECT_LT(peak, Impl::kSafetyMarginSamples) << "speedHz=" << speedHz << " peak=" << peak;
    }
}

void feedNoiseBlocks(Impl& impl, const size_t numBlocks, std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    AbacDsp::AudioBuffer<2, BlockSize> in{};
    AbacDsp::AudioBuffer<2, BlockSize> out{};
    for (size_t b = 0; b < numBlocks; ++b)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            in(i, 0) = dist(rng);
            in(i, 1) = dist(rng);
        }
        impl.processBlock(in, out);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_TRUE(std::isfinite(out(i, 1)));
            ASSERT_LT(std::abs(out(i, 0)), 10.f);
            ASSERT_LT(std::abs(out(i, 1)), 10.f);
        }
    }
}

TEST(PathfinderImplTest, StaysFiniteAcrossFullMacroSweep)
{
    std::mt19937 rng{1};
    Impl impl{SampleRate};
    for (const float macro : {0.f, 25.f, 50.f, 75.f, 100.f})
    {
        impl.setDepth(macro);
        impl.setSpeed(0.05f + macro * 0.0595f); // 0.05..6.0 Hz
        impl.setAggressivity(macro);
        impl.setCharacter(macro);
        feedNoiseBlocks(impl, 50, rng);
    }
}

TEST(PathfinderImplTest, CharacterExtremesStayFiniteWithActiveModulation)
{
    std::mt19937 rng{2};
    for (const float character : {0.f, 25.f, 50.f, 75.f, 100.f})
    {
        Impl impl{SampleRate};
        impl.setDepth(100.f);
        impl.setSpeed(3.f);
        impl.setAggressivity(100.f);
        impl.setCharacter(character);
        feedNoiseBlocks(impl, 100, rng);
    }
}

// Confirms the read head is actually wired to the expected base delay: with modulation
// off, an impulse's energy should surface close to kBaseDelayMs, not at zero delay or
// somewhere arbitrary in the buffer.
TEST(PathfinderImplTest, ImpulseEnergySurfacesNearTheConfiguredBaseDelay)
{
    Impl impl{SampleRate};
    impl.setDepth(0.f);
    impl.setAggressivity(0.f);
    impl.setCharacter(50.f);

    AbacDsp::AudioBuffer<2, BlockSize> in{};
    AbacDsp::AudioBuffer<2, BlockSize> out{};
    in(0, 0) = 1.f;
    in(0, 1) = 1.f;
    impl.processBlock(in, out);
    in(0, 0) = 0.f;
    in(0, 1) = 0.f;

    const auto target = static_cast<size_t>(Impl::kBaseDelayMs * 0.001f * SampleRate);
    constexpr size_t numBlocks = 1024 / BlockSize;
    size_t peakSample = 0;
    float peakValue = 0.f;
    for (size_t b = 0; b < numBlocks; ++b)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (const auto magnitude = std::abs(out(i, 0)); magnitude > peakValue)
            {
                peakValue = magnitude;
                peakSample = b * BlockSize + i;
            }
        }
    }
    EXPECT_GT(peakSample, target / 2);
    EXPECT_LT(peakSample, target * 3 / 2);
}

}
