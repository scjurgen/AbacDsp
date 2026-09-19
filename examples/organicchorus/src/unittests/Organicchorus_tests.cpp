#include <array>
#include <cmath>
#include <memory>
#include <random>

#include "gtest/gtest.h"

#include "Audio/AudioBuffer.h"
#include "Delays/WobbleDelay.h"
#include "impl/ChorusConfigurations.h"
#include "impl/OrganicChorusImpl.h"

namespace
{

constexpr size_t BlockSize{16};
constexpr float SampleRate{48000.f};

using Impl = OrganicChorusImpl<BlockSize>;

// Mirrors OrganicChorusVoice's use of WobbleDelay at one Speed setting: reports the peak
// deviation of the read/write distance from its centre over a multi-minute session.
float measurePeakDriftSamples(const OrganicChorus::ChorusConfigurationSpec& spec, const float wowRateHz,
                              const size_t numSamples)
{
    AbacDsp::WobbleDelay<Impl::kBufferSize, BlockSize> delay{SampleRate};
    const float targetDistance = spec.baseDelayMs.at(1.f) * 0.001f * SampleRate;
    delay.setSafetyMargin(spec.readHeadSafetyMarginSamples);
    delay.setDelay(targetDistance, true);
    delay.seed(2024);
    delay.setWowRate(wowRateHz);
    delay.setWowDepth(spec.wowDepth.at(1.f));
    delay.setWowVariance(0.f);
    delay.setWowDrift(0.f);
    delay.setFlutterRate(std::max(wowRateHz, spec.flutterRateFloorHz));
    delay.setFlutterDepth(spec.flutterDepth.at(1.f));

    float peak = 0.f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        static_cast<void>(delay.step(0.f));
        peak = std::max(peak, std::abs(delay.currentDelay() - targetDistance));
    }
    return peak;
}

// A coarse 3-point sweep over Speed once passed here while the real worst case sat at an
// untested rate in between (see commit history) - the peak-vs-rate relationship is not
// monotonic, so this sweeps finely enough to actually find it.
TEST(OrganicChorusConfigurationSafetyTest, EveryReachableSpeedStaysWithinConfiguredSafetyMargin)
{
    constexpr size_t numSamples = static_cast<size_t>(60 * SampleRate);
    constexpr int kSteps = 15;
    for (const auto& spec : OrganicChorus::kConfigurations)
    {
        for (int i = 0; i <= kSteps; ++i)
        {
            const float rateFraction = static_cast<float>(i) / static_cast<float>(kSteps);
            const auto peak = measurePeakDriftSamples(spec, spec.wowRateHz.at(rateFraction), numSamples);
            EXPECT_LT(peak, spec.readHeadSafetyMarginSamples) << "rateFraction=" << rateFraction << " peak=" << peak;
        }
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

TEST(OrganicChorusImplTest, EveryConfigurationStaysFiniteAcrossFullMacroSweep)
{
    std::mt19937 rng{1};
    for (int config = 0; config < static_cast<int>(OrganicChorus::kConfigurationCount); ++config)
    {
        Impl impl{SampleRate};
        impl.setConfiguration(config);
        impl.setMix(70.f);
        for (const float macro : {0.f, 25.f, 50.f, 75.f, 100.f})
        {
            impl.setTone(macro);
            impl.setSpeed(0.05f + macro * 0.0595f); // 0.05..6.0 Hz
            impl.setDepth(macro);
            impl.setFeedback(macro * 2.f - 100.f);
            impl.setTapeSpeed(macro * 0.48f - 24.f); // -24..24 semitones
            feedNoiseBlocks(impl, 50, rng);
        }
    }
}

TEST(OrganicChorusImplTest, TapeSpeedExtremesStayFiniteWithActiveModulation)
{
    std::mt19937 rng{5};
    for (int config = 0; config < static_cast<int>(OrganicChorus::kConfigurationCount); ++config)
    {
        for (const float semitones : {-24.f, -12.f, 0.f, 12.f, 24.f})
        {
            Impl impl{SampleRate};
            impl.setConfiguration(config);
            impl.setSpeed(0.7f);
            impl.setDepth(100.f);
            impl.setMix(70.f);
            impl.setTapeSpeed(semitones);
            feedNoiseBlocks(impl, 100, rng);
        }
    }
}

// Locks in that Feedback has a clearly audible effect on every configuration - a macro
// range too narrow to matter shipped once (Tri Ensemble's), so this checks the actual
// magnitude, not just "compiles and stays finite".
TEST(OrganicChorusImplTest, FeedbackProducesAMeaningfulEnergyIncreaseOnEveryConfiguration)
{
    const auto impulseTailEnergy = [](const int configuration, const float feedbackPercent)
    {
        Impl impl{SampleRate};
        impl.setConfiguration(configuration);
        impl.setDepth(0.f);
        impl.setMix(100.f);
        impl.setFeedback(feedbackPercent);

        AbacDsp::AudioBuffer<2, BlockSize> in{};
        AbacDsp::AudioBuffer<2, BlockSize> out{};
        in(0, 0) = 1.f;
        in(0, 1) = 1.f;
        impl.processBlock(in, out);
        in(0, 0) = 0.f;
        in(0, 1) = 0.f;

        float energy = 0.f;
        for (size_t b = 0; b < 3000; ++b)
        {
            impl.processBlock(in, out);
            for (size_t i = 0; i < BlockSize; ++i)
            {
                energy += out(i, 0) * out(i, 0) + out(i, 1) * out(i, 1);
            }
        }
        return energy;
    };

    for (int config = 0; config < static_cast<int>(OrganicChorus::kConfigurationCount); ++config)
    {
        const auto atZero = impulseTailEnergy(config, 0.f);
        const auto atMax = impulseTailEnergy(config, 100.f);
        EXPECT_GT(atMax, atZero * 1.2f) << "config=" << config << " atZero=" << atZero << " atMax=" << atMax;
    }
}

TEST(OrganicChorusImplTest, ConfigurationIndexIsClampedToTheTable)
{
    Impl impl{SampleRate};
    impl.setConfiguration(999);
    std::mt19937 rng{2};
    feedNoiseBlocks(impl, 20, rng);
}

TEST(OrganicChorusImplTest, ZeroMixIsBitIdenticalToDryInput)
{
    Impl impl{SampleRate};
    impl.setConfiguration(0);
    impl.setMix(0.f);

    std::mt19937 rng{3};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    AbacDsp::AudioBuffer<2, BlockSize> in{};
    AbacDsp::AudioBuffer<2, BlockSize> out{};
    for (size_t b = 0; b < 20; ++b)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            in(i, 0) = dist(rng);
            in(i, 1) = dist(rng);
        }
        impl.processBlock(in, out);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            EXPECT_FLOAT_EQ(out(i, 0), in(i, 0));
            EXPECT_FLOAT_EQ(out(i, 1), in(i, 1));
        }
    }
}

TEST(OrganicChorusImplTest, DriftAndSpreadLuaKnobsStayFinite)
{
    Impl impl{SampleRate};
    impl.setConfiguration(2);
    impl.setMix(60.f);
    impl.setLuaParam1(1.f);
    impl.setLuaParam2(1.f);
    std::mt19937 rng{4};
    feedNoiseBlocks(impl, 50, rng);
}

}
