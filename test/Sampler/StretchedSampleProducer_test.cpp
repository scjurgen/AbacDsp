#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include <vector>
#include <memory>
#include "Sampler/StretchedSampleProducer.h"
#include "Analysis/ZeroCrossings.h"

namespace AbacDsp::Test
{

class StretchedSampleProducerTest : public ::testing::Test
{
  protected:
    static constexpr float sampleRate{48000.f};

    void SetUp() override
    {
        m_producer = std::make_unique<StretchedSampleProducer>(sampleRate);
        generateInterleavedSineWave();
    }

    void generateInterleavedSineWave()
    {
        const float frequency = 440.0f;
        const float duration = 1.5f;
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

    std::unique_ptr<StretchedSampleProducer> m_producer;
    std::shared_ptr<std::vector<float>> m_sineData;
};

TEST_F(StretchedSampleProducerTest, BasicSampleProduction)
{
    constexpr size_t bufferSize = 1024;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(1.0f);
    m_producer->setPosition(0);

    const bool success = m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    float maxAmplitude = 0.0f;
    for (const auto sample : leftOut)
    {
        maxAmplitude = std::max(maxAmplitude, std::abs(sample));
    }

    EXPECT_GT(maxAmplitude, 0.1f) << "Output should contain audio signal";
}

TEST_F(StretchedSampleProducerTest, ProducesExpectedSampleCount)
{
    constexpr size_t requestedSamples = 48000;
    std::vector<float> leftOut(requestedSamples);
    std::vector<float> rightOut(requestedSamples);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(1.0f);
    m_producer->setPosition(0);

    size_t totalProduced = 0;
    constexpr size_t chunkSize = 512;

    while (totalProduced < requestedSamples)
    {
        const size_t remaining = requestedSamples - totalProduced;
        const size_t toRequest = std::min(chunkSize, remaining);

        const bool success =
            m_producer->produceSamples(leftOut.data() + totalProduced, rightOut.data() + totalProduced, toRequest);

        if (!success)
        {
            break;
        }

        totalProduced += toRequest;
    }

    EXPECT_EQ(totalProduced, requestedSamples);
}

TEST_F(StretchedSampleProducerTest, FeedRateHalfSpeed)
{
    constexpr size_t bufferSize = 24000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(0.5f);
    m_producer->setPosition(0);
    m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);

    const auto stats = calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

    EXPECT_GT(stats.periodCount, 0u) << "Should have detected zero crossings";

    const float expectedFrequency = 440.0f;
    const float measuredFrequency = sampleRate / stats.meanPeriodLen;
    const float tolerance = expectedFrequency * 0.02f;

    EXPECT_NEAR(measuredFrequency, expectedFrequency, tolerance)
        << "Frequency should remain at 440Hz with half-speed feed rate";

    float rms = 0.0f;
    for (const auto sample : leftOut)
    {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / bufferSize);
    EXPECT_GT(rms, 0.5f) << "Signal should have sufficient energy";
}

TEST_F(StretchedSampleProducerTest, FeedRateDoubleSpeed)
{
    constexpr size_t bufferSize = 24000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(2.0f);
    m_producer->setPosition(0);

    const bool success = m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    const auto stats = calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

    EXPECT_GT(stats.periodCount, 0u) << "Should have detected zero crossings";

    const float expectedFrequency = 440.0f;
    const float measuredFrequency = sampleRate / stats.meanPeriodLen;
    const float tolerance = expectedFrequency * 0.06f;

    EXPECT_NEAR(measuredFrequency, expectedFrequency, tolerance)
        << "Frequency should remain at 440Hz with double-speed feed rate";

    float rms = 0.0f;
    for (const auto sample : leftOut)
    {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / bufferSize);
    EXPECT_GT(rms, 0.25f) << "Signal should have sufficient energy";
}

TEST_F(StretchedSampleProducerTest, SineWaveIntegrity)
{
    constexpr size_t bufferSize = 24000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(1.0f);
    m_producer->setPosition(0);

    m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);

    const auto stats = calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

    EXPECT_GT(stats.periodCount, 0u) << "Should have detected zero crossings";

    const float expectedFrequency = 440.0f;
    const float measuredFrequency = sampleRate / stats.meanPeriodLen;
    const float tolerance = expectedFrequency * 0.02f;

    EXPECT_NEAR(measuredFrequency, expectedFrequency, tolerance) << "Measured frequency should match input frequency";

    EXPECT_LT(stats.standardDeviation, stats.meanPeriodLen * 0.1f)
        << "Period length should be stable (low std deviation)";
}

TEST_F(StretchedSampleProducerTest, PositionControl)
{
    constexpr size_t bufferSize = 2048;
    constexpr size_t jumpPosition = 4800;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(1.0f);
    m_producer->setPosition(jumpPosition);

    const bool success = m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    float rms = 0.0f;
    for (const auto sample : leftOut)
    {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / bufferSize);

    EXPECT_GT(rms, 0.1f) << "Should produce audio after position jump";
}

TEST_F(StretchedSampleProducerTest, StopsAtEnd)
{
    constexpr size_t bufferSize = 100000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(1.0f);
    m_producer->setPosition(0);

    const bool success = m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_FALSE(success);
    EXPECT_TRUE(m_producer->isDone());
}

TEST_F(StretchedSampleProducerTest, StereoSymmetry)
{
    constexpr size_t bufferSize = 2048;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_producer->setRawStereoSample(m_sineData);
    m_producer->setFeedRate(1.0f);
    m_producer->setPosition(0);

    m_producer->produceSamples(leftOut.data(), rightOut.data(), bufferSize);

    for (size_t i = 0; i < bufferSize; ++i)
    {
        EXPECT_NEAR(leftOut[i], rightOut[i], 0.001f)
            << "Stereo channels should be identical for mono input at sample " << i;
    }
}

}