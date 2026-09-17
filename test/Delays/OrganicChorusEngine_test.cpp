#include <array>
#include <cmath>
#include <memory>
#include <random>

#include "gtest/gtest.h"

#include "Delays/OrganicChorusEngine.h"
#include "Filters/Sinc/sinc_4.h"

namespace AbacDsp::Test
{

constexpr size_t TileSize{8};
constexpr size_t BufferSize{20000};
constexpr float SampleRate{48000.f};

using Engine = OrganicChorusEngine<BufferSize, TileSize, 4>;

[[nodiscard]] static std::shared_ptr<SincFilter> makeSincFilter()
{
    return std::make_shared<SincFilter>(sinc4);
}

void configureVoice(Engine& engine, const size_t voice, const float delaySamples)
{
    engine.setVoiceReadHeadSafetyMargin(voice, 64.f);
    engine.setVoiceReadHeadCorrectionThreshold(voice, delaySamples * 0.2f);
    engine.setVoiceCentreDelay(voice, delaySamples, true);
    engine.setVoiceWow(voice, 0.4f + 0.05f * static_cast<float>(voice), 0.5f, 0.2f, 0.2f);
    engine.setVoiceFlutter(voice, 0.5f, 0.2f);
    engine.setVoiceTone(voice, 80.f, 6000.f, 5000.f);
    engine.setVoiceSaturation(voice, 0.1f);
    engine.setVoiceFeedback(voice, 0.f, 4000.f);
    engine.setVoiceGain(voice, 1.f);
}

TEST(OrganicChorusEngineTest, OutputStaysFiniteAcrossEveryVoiceCountAndFeedback)
{
    for (size_t voiceCount = 1; voiceCount <= 4; ++voiceCount)
    {
        Engine engine{SampleRate, makeSincFilter()};
        engine.setActiveVoiceCount(voiceCount);
        engine.setMix(0.6f);
        for (size_t v = 0; v < voiceCount; ++v)
        {
            configureVoice(engine, v, 400.f + 50.f * static_cast<float>(v));
            engine.setVoiceFeedback(v, 0.85f, 4000.f);
            engine.setVoicePan(v, -1.f + 2.f * static_cast<float>(v) / 3.f);
        }

        std::mt19937 rng{1};
        std::uniform_real_distribution<float> dist{-1.f, 1.f};
        std::array<float, 2 * TileSize> out{};
        for (size_t b = 0; b < 2000; ++b)
        {
            std::array<float, 2 * TileSize> in{};
            std::ranges::generate(in, [&] { return dist(rng); });
            engine.processBlock(in, out);
            for (const auto sample : out)
            {
                ASSERT_TRUE(std::isfinite(sample)) << "voiceCount=" << voiceCount;
                ASSERT_LT(std::abs(sample), 10.f) << "voiceCount=" << voiceCount;
            }
        }
    }
}

TEST(OrganicChorusEngineTest, MonoSummedInputStaysStableUnderFeedback)
{
    Engine engine{SampleRate, makeSincFilter()};
    engine.setActiveVoiceCount(2);
    engine.setMix(1.f);
    configureVoice(engine, 0, 400.f);
    configureVoice(engine, 1, 420.f);
    engine.setVoiceFeedback(0, 0.9f, 3000.f);
    engine.setVoiceFeedback(1, -0.9f, 3000.f);

    std::array<float, 2 * TileSize> out{};
    for (size_t b = 0; b < 5000; ++b)
    {
        std::array<float, 2 * TileSize> in{};
        in[0] = b == 0 ? 1.f : 0.f;
        in[1] = b == 0 ? 1.f : 0.f;
        engine.processBlock(in, out);
        for (const auto sample : out)
        {
            ASSERT_TRUE(std::isfinite(sample));
            ASSERT_LT(std::abs(sample), 10.f);
        }
    }
}

TEST(OrganicChorusEngineTest, ZeroMixIsBitIdenticalToDryInput)
{
    Engine engine{SampleRate, makeSincFilter()};
    engine.setActiveVoiceCount(4);
    engine.setMix(0.f);
    for (size_t v = 0; v < 4; ++v)
    {
        configureVoice(engine, v, 400.f);
    }

    std::mt19937 rng{7};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    std::array<float, 2 * TileSize> out{};
    for (size_t b = 0; b < 100; ++b)
    {
        std::array<float, 2 * TileSize> in{};
        std::ranges::generate(in, [&] { return dist(rng); });
        engine.processBlock(in, out);
        EXPECT_EQ(in, out);
    }
}

}
