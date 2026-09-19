#include <array>
#include <cmath>
#include <random>

#include "gtest/gtest.h"

#include "Delays/OrganicChorusVoice.h"

namespace AbacDsp::Test
{

constexpr size_t TileSize{8};
constexpr size_t BufferSize{20000};
constexpr float SampleRate{48000.f};

using Voice = OrganicChorusVoice<BufferSize, TileSize>;

// Empirical basis for a configuration's safety margin: the largest observed deviation, in
// samples, of the read/write distance from its nominal target.
[[nodiscard]] float measurePeakExcursionSamples(const float wowRate, const float wowPerceptualDepth,
                                                const float wowVariance, const float wowDrift, const float flutterRate,
                                                const float flutterDepth, const size_t numSamples)
{
    WobbleDelay<BufferSize, TileSize> sut{SampleRate};
    constexpr float kTargetDistance{BufferSize / 2.f};
    sut.setSafetyMargin(8.f);
    sut.setDelay(kTargetDistance, true);
    sut.setWowRate(wowRate);
    sut.setWowDepth(wowPerceptualDepth);
    sut.setWowVariance(wowVariance);
    sut.setWowDrift(wowDrift);
    sut.setFlutterRate(flutterRate);
    sut.setFlutterDepth(flutterDepth);

    float peak = 0.f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        static_cast<void>(sut.step(0.f));
        peak = std::max(peak, std::abs(sut.currentDelay() - kTargetDistance));
    }
    return peak;
}

// Settings representative of a tame, short-delay configuration (Classic/Flanger); keep
// in sync by hand with examples/organicchorus/src/impl/ChorusConfigurations.h.
TEST(OrganicChorusVoiceSafetyMarginMeasurement, TameSettingsStayWellUnderTwoMilliseconds)
{
    const auto peak = measurePeakExcursionSamples(0.6f, 0.35f, 0.1f, 0.2f, 0.5f, 0.15f, 20 * SampleRate);
    EXPECT_LT(peak, SampleRate * 0.002f);
}

// Settings representative of a deeper, wider configuration (Tri Ensemble): still bounded,
// but a noticeably larger excursion than the tame case above.
TEST(OrganicChorusVoiceSafetyMarginMeasurement, DeeperSettingsStayBounded)
{
    const auto peak = measurePeakExcursionSamples(0.3f, 0.7f, 0.3f, 0.4f, 0.4f, 0.3f, 20 * SampleRate);
    EXPECT_LT(peak, SampleRate * 0.004f);
}

// The modulation must actually move the read head, not just stay inside the bound.
TEST(OrganicChorusVoiceSafetyMarginMeasurement, DeeperSettingsActuallyModulateTheDelay)
{
    const auto tame = measurePeakExcursionSamples(0.6f, 0.35f, 0.1f, 0.2f, 0.5f, 0.15f, 20 * SampleRate);
    const auto deeper = measurePeakExcursionSamples(0.3f, 0.7f, 0.3f, 0.4f, 0.4f, 0.3f, 20 * SampleRate);
    EXPECT_GT(tame, 1.f);
    EXPECT_GT(deeper, tame);
}

TEST(OrganicChorusVoiceTest, OutputStaysFiniteAndBoundedUnderFullModulationAndFeedback)
{
    Voice voice{SampleRate};
    voice.setReadHeadSafetyMargin(64.f);
    voice.setCentreDelay(500.f, true);
    voice.setWowRate(0.4f);
    voice.setWowDepth(0.8f);
    voice.setWowVariance(0.3f);
    voice.setWowDrift(0.4f);
    voice.setFlutterRate(0.5f);
    voice.setFlutterDepth(0.3f);
    voice.setToneHighPass(80.f);
    voice.setTonePreLowPass(6000.f);
    voice.setTonePostLowPass(5000.f);
    voice.setSaturation(0.3f);
    voice.setFeedback(0.9f, 4000.f);

    std::mt19937 rng{1234};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    std::array<float, TileSize> out{};
    for (size_t b = 0; b < 2000; ++b)
    {
        std::array<float, TileSize> in{};
        std::ranges::generate(in, [&] { return dist(rng); });
        voice.processBlock(in, out);
        for (const auto sample : out)
        {
            ASSERT_TRUE(std::isfinite(sample));
            ASSERT_LT(std::abs(sample), 10.f);
        }
    }
}

TEST(OrganicChorusVoiceTest, SameSeedProducesIdenticalOutput)
{
    const auto configure = [](Voice& voice)
    {
        voice.setCentreDelay(500.f, true);
        voice.setWowRate(0.5f);
        voice.setWowDepth(0.5f);
        voice.setWowVariance(0.2f);
        voice.setWowDrift(0.2f);
        voice.setFlutterRate(0.4f);
        voice.setFlutterDepth(0.2f);
        voice.seed(42);
    };

    Voice a{SampleRate};
    Voice b{SampleRate};
    configure(a);
    configure(b);

    const std::array<float, TileSize> in{1.f};
    for (size_t block = 0; block < 500; ++block)
    {
        std::array<float, TileSize> outA{};
        std::array<float, TileSize> outB{};
        a.processBlock(in, outA);
        b.processBlock(in, outB);
        EXPECT_EQ(outA, outB) << "diverged at block " << block;
    }
}

TEST(OrganicChorusVoiceTest, DifferentSeedsEventuallyDiverge)
{
    const auto configure = [](Voice& voice, const std::mt19937::result_type seed)
    {
        voice.setCentreDelay(500.f, true);
        voice.setWowRate(0.5f);
        voice.setWowDepth(0.5f);
        voice.setWowVariance(0.3f);
        voice.setWowDrift(0.3f);
        voice.seed(seed);
    };

    Voice a{SampleRate};
    Voice b{SampleRate};
    configure(a, 1);
    configure(b, 2);

    const std::array<float, TileSize> in{1.f};
    bool diverged = false;
    for (size_t block = 0; block < 2000 && !diverged; ++block)
    {
        std::array<float, TileSize> outA{};
        std::array<float, TileSize> outB{};
        a.processBlock(in, outA);
        b.processBlock(in, outB);
        diverged = outA != outB;
    }
    EXPECT_TRUE(diverged);
}

}
