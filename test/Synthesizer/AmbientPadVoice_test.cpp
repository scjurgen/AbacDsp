#include <cmath>

#include "gtest/gtest.h"

#include "Synthesizer/AmbientPadVoice.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;

class AmbientPadVoiceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        voice = std::make_unique<AmbientPadVoice>(kSampleRate, waveShaperTables);
    }

    WaveShaperTableStore waveShaperTables;
    std::unique_ptr<AmbientPadVoice> voice;
};

void assertFiniteAndBounded(const std::array<float, 512>& block)
{
    for (const auto sample : block)
    {
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 8.f);
    }
}
}

TEST_F(AmbientPadVoiceTest, triggerProducesFiniteBoundedOutput)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());
    assertFiniteAndBounded(mono);
}

TEST_F(AmbientPadVoiceTest, isPlayingReflectsEnvelopeLifecycle)
{
    EXPECT_FALSE(voice->isPlaying());
    voice->triggerVoice(69, 100);
    EXPECT_TRUE(voice->isPlaying());

    voice->stopVoice();
    std::array<float, 512> mono{};
    for (int block = 0; block < 400; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
    }
    EXPECT_FALSE(voice->isPlaying());
}

TEST_F(AmbientPadVoiceTest, sweepingMaterialAndLightStaysFiniteAndBounded)
{
    voice->setMotion(1.f);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (int step = 0; step <= 20; ++step)
    {
        const auto position = static_cast<float>(step) / 20.f;
        voice->setMaterial(position);
        voice->setLight(1.f - position);
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
}

TEST_F(AmbientPadVoiceTest, holdFreezesModulationWithoutBreakingOutput)
{
    voice->setMotion(1.f);
    voice->setHold(true);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
}

TEST_F(AmbientPadVoiceTest, distortionStagePassesThroughFiniteOutput)
{
    voice->setDistortion(1);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (int block = 0; block < 10; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
}

TEST_F(AmbientPadVoiceTest, gainSmoothingReachesTargetWithoutDiscontinuity)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());

    voice->setGain(-12.f);
    float previous = mono.back();
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        for (const auto sample : mono)
        {
            ASSERT_LE(std::abs(sample - previous), 1.f);
            previous = sample;
        }
    }
}

}
