#include <array>
#include <cmath>
#include <memory>
#include <random>

#include "gtest/gtest.h"

#include "Delays/OrganicChorusVoice.h"
#include "Filters/Sinc/sinc_4.h"

namespace AbacDsp::Test
{

constexpr size_t TileSize{8};
constexpr size_t BufferSize{20000};
constexpr float SampleRate{48000.f};

using Voice = OrganicChorusVoice<BufferSize, TileSize>;

[[nodiscard]] static std::shared_ptr<SincFilter> makeSincFilter()
{
    return std::make_shared<SincFilter>(sinc4);
}

// Empirical basis for a configuration's read-head safety margin: the largest observed
// deviation, in samples, of the actual read/write distance from its nominal target.
[[nodiscard]] float measurePeakDriftSamples(const float wowRate, const float wowPerceptualDepth,
                                            const float wowVariance, const float wowDrift, const float flutterRate,
                                            const float flutterDepth, const size_t numBlocks)
{
    WobbleDelay<BufferSize, 1, 1, TileSize> sut{SampleRate, makeSincFilter()};
    constexpr float kTargetDistance{BufferSize / 2.f};
    sut.setReadHeadSafetyMargin(8.f);
    sut.setReadHead(0, kTargetDistance, true);
    // Correction defaults to a 1-sample threshold (see VariSpeedTapeDelay's own comment),
    // which would mask Wow/Flutter's own excursion rather than let it be measured.
    sut.setReadHeadCorrectionThreshold(0, kTargetDistance);
    sut.setWowRate(wowRate);
    sut.setWowDepth(wowPerceptualDepth);
    sut.setWowVariance(wowVariance);
    sut.setWowDrift(wowDrift);
    sut.setFlutterRate(flutterRate);
    sut.setFlutterDepth(flutterDepth);

    float peak = 0.f;
    const std::array<float, TileSize> silence{};
    for (size_t b = 0; b < numBlocks; ++b)
    {
        sut.feed(silence);
        std::array<float, TileSize> discard{};
        sut.readBlock(0, discard);

        auto delta = static_cast<double>(sut.writeHead()) - sut.readHead(0);
        while (delta < 0.0)
        {
            delta += BufferSize;
        }
        while (delta >= BufferSize)
        {
            delta -= BufferSize;
        }
        peak = std::max(peak, static_cast<float>(std::abs(delta - kTargetDistance)));
    }
    return peak;
}

// Settings representative of a tame, short-delay configuration (Classic/Flanger); keep
// in sync by hand with examples/organicchorus/src/impl/ChorusConfigurations.h.
TEST(OrganicChorusVoiceSafetyMarginMeasurement, TameSettingsStayWellUnderTwoMilliseconds)
{
    const auto peak = measurePeakDriftSamples(0.6f, 0.35f, 0.1f, 0.2f, 0.5f, 0.15f, 20 * SampleRate / TileSize);
    EXPECT_LT(peak, SampleRate * 0.002f);
}

// Settings representative of a deeper, wider configuration (Tri Ensemble): still bounded,
// but a noticeably larger excursion than the tame case above.
TEST(OrganicChorusVoiceSafetyMarginMeasurement, DeeperSettingsStayBounded)
{
    const auto peak = measurePeakDriftSamples(0.3f, 0.7f, 0.3f, 0.4f, 0.4f, 0.3f, 20 * SampleRate / TileSize);
    EXPECT_LT(peak, SampleRate * 0.004f);
}

// The correction mechanism itself (not just Wow/Flutter depth) must actually be raised
// past a modulation's own excursion, or the intended modulation is corrected away as if
// it were drift - this is the regression that motivated exposing the threshold at all.
TEST(OrganicChorusVoiceSafetyMarginMeasurement, DefaultCorrectionThresholdWouldMaskTheModulation)
{
    WobbleDelay<BufferSize, 1, 1, TileSize> sut{SampleRate, makeSincFilter()};
    constexpr float kTargetDistance{BufferSize / 2.f};
    sut.setReadHeadSafetyMargin(8.f);
    sut.setReadHead(0, kTargetDistance, true);
    sut.setWowRate(0.3f);
    sut.setWowDepth(0.7f);
    sut.setWowVariance(0.3f);
    sut.setWowDrift(0.4f);
    sut.setFlutterRate(0.4f);
    sut.setFlutterDepth(0.3f);

    float peak = 0.f;
    const std::array<float, TileSize> silence{};
    for (size_t b = 0; b < 20 * SampleRate / TileSize; ++b)
    {
        sut.feed(silence);
        std::array<float, TileSize> discard{};
        sut.readBlock(0, discard);
        auto delta = static_cast<double>(sut.writeHead()) - sut.readHead(0);
        while (delta < 0.0)
        {
            delta += BufferSize;
        }
        while (delta >= BufferSize)
        {
            delta -= BufferSize;
        }
        peak = std::max(peak, static_cast<float>(std::abs(delta - kTargetDistance)));
    }
    // Chunked read reconstruction (kLowRateChunkFrames) leaves a ~67-69 sample baseline
    // residual even fully masked now - matches WobbleDelayWriteTest's "quiet" case.
    EXPECT_LT(peak, 100.f);
}

TEST(OrganicChorusVoiceTest, OutputStaysFiniteAndBoundedUnderFullModulationAndFeedback)
{
    Voice voice{SampleRate, makeSincFilter()};
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

    Voice a{SampleRate, makeSincFilter()};
    Voice b{SampleRate, makeSincFilter()};
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

    Voice a{SampleRate, makeSincFilter()};
    Voice b{SampleRate, makeSincFilter()};
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
