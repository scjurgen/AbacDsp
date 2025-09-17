#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "Analysis/ZeroCrossings.h"
#include "Sampler/SamplePlayerBasic.h"

namespace AbacDsp::Test
{

class SamplePlayerTest : public ::testing::Test
{
  protected:
    static constexpr float sampleRate{48000.f};
    void SetUp() override
    {
        m_player = std::make_unique<SamplePlayerBasic>(sampleRate);
        generateInterleavedSineWave();
    }

    void generateInterleavedSineWave()
    {
        const float frequency = 440.0f;
        const float duration = 0.1f;
        const size_t numSamples = static_cast<size_t>(sampleRate * duration);
        m_sineData = std::make_shared<std::vector<float>>(numSamples * 2);
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float>(i) / sampleRate;
            const float sample = std::sin(2.0f * std::numbers::pi_v<float> * frequency * t);
            (*m_sineData)[i * 2] = sample;
            (*m_sineData)[i * 2 + 1] = sample;
        }
    }

    std::unique_ptr<SamplePlayerBasic> m_player;
    std::shared_ptr<std::vector<float>> m_sineData;
};

TEST_F(SamplePlayerTest, ZeroCrossingsAcrossPitchRange)
{
    constexpr size_t bufferSize = 48000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    for (int semitone = -48; semitone <= 48; ++semitone)
    {
        const float playbackRate = std::pow(2.0f, static_cast<float>(semitone) / 12.0f);

        m_player->runStereo(m_sineData);
        m_player->setPlaybackRate(playbackRate);
        m_player->restart();

        std::ranges::fill(leftOut, 0.0f);
        std::ranges::fill(rightOut, 0.0f);

        m_player->processBlock(leftOut.data(), rightOut.data(), bufferSize);
        const auto stats = AbacDsp::calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

        if (stats.periodCount > 0)
        {
            const float expectedPeriod = sampleRate / (440.0f * playbackRate);
            const float tolerance = expectedPeriod * 1E-2f;
            EXPECT_NEAR(stats.meanPeriodLen, expectedPeriod, tolerance)
                << "Semitone: " << semitone << " Rate: " << playbackRate << " Expected period: " << expectedPeriod
                << " Mean period: " << stats.meanPeriodLen;
        }
    }
}

}