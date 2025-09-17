#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include <vector>
#include <memory>
#include "Sampler/ResamplingPitchShifter.h"
#include "Analysis/ZeroCrossings.h"

namespace AbacDsp::Test
{

class MockSampleProducer
{
public:
    explicit MockSampleProducer(float frequency, float sampleRate = 48000.0f)
        : m_frequency(frequency), m_sampleRate(sampleRate), m_sampleCount(0)
    {
    }

    bool produceSamples(float* outLeft, float* outRight, const size_t numSamples)
    {
        constexpr size_t maxSamples = 144000;

        for (size_t i = 0; i < numSamples; ++i)
        {
            if (m_sampleCount >= maxSamples)
            {
                return false;
            }

            const float t = static_cast<float>(m_sampleCount) / m_sampleRate;
            const float sample = std::sin(2.0f * std::numbers::pi_v<float> * m_frequency * t);

            outLeft[i] = sample;
            outRight[i] = sample;
            m_sampleCount++;
        }

        return true;
    }

private:
    float m_frequency;
    float m_sampleRate;
    size_t m_sampleCount;
};

class ResamplingPitchShifterTest : public ::testing::Test
{
protected:
    static constexpr float sampleRate{48000.0f};
    static constexpr float frequency{440.0f};

    void SetUp() override
    {
        m_producer = std::make_unique<MockSampleProducer>(frequency, sampleRate);
        m_pitchShifter = std::make_unique<ResamplingPitchShifter<MockSampleProducer>>(m_producer.get());
    }

    std::unique_ptr<MockSampleProducer> m_producer;
    std::unique_ptr<ResamplingPitchShifter<MockSampleProducer>> m_pitchShifter;
};

TEST_F(ResamplingPitchShifterTest, BasicSampleProduction)
{
    constexpr size_t bufferSize = 1024;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_pitchShifter->setPitchRatio(1.0f);
    const bool success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    float maxAmplitude = 0.0f;
    for (const auto sample : leftOut)
    {
        maxAmplitude = std::max(maxAmplitude, std::abs(sample));
    }

    EXPECT_GT(maxAmplitude, 0.1f) << "Output should contain audio signal";
}

TEST_F(ResamplingPitchShifterTest, PitchRatioUnity)
{
    constexpr size_t bufferSize = 24000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_pitchShifter->setPitchRatio(1.0f);
    const bool success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    const auto stats = calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

    EXPECT_GT(stats.periodCount, 0u) << "Should have detected zero crossings";

    const float expectedFrequency = frequency;
    const float measuredFrequency = sampleRate / stats.meanPeriodLen;
    const float tolerance = expectedFrequency * 0.05f;

    EXPECT_NEAR(measuredFrequency, expectedFrequency, tolerance)
        << "Frequency should remain at " << frequency << "Hz with unity pitch ratio";

    float rms = 0.0f;
    for (const auto sample : leftOut)
    {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / bufferSize);
    EXPECT_GT(rms, 0.5f) << "Signal should have sufficient energy";
}

TEST_F(ResamplingPitchShifterTest, PitchUpOctave)
{
    constexpr size_t bufferSize = 24000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_pitchShifter->setPitchRatio(2.0f);
    const bool success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    const auto stats = calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

    EXPECT_GT(stats.periodCount, 0u) << "Should have detected zero crossings";

    const float expectedFrequency = frequency * 2.0f;
    const float measuredFrequency = sampleRate / stats.meanPeriodLen;
    const float tolerance = expectedFrequency * 0.08f;

    EXPECT_NEAR(measuredFrequency, expectedFrequency, tolerance)
        << "Frequency should be doubled (octave up) with pitch ratio 2.0";

    float rms = 0.0f;
    for (const auto sample : leftOut)
    {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / bufferSize);
    EXPECT_GT(rms, 0.3f) << "Signal should have sufficient energy";
}

TEST_F(ResamplingPitchShifterTest, PitchDownOctave)
{
    constexpr size_t bufferSize = 48000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_pitchShifter->setPitchRatio(0.5f);
    const bool success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), bufferSize);
    EXPECT_TRUE(success);

    const auto stats = calculateZeroCrossingStatistics(leftOut.data(), bufferSize, true);

    EXPECT_GT(stats.periodCount, 0u) << "Should have detected zero crossings";

    const float expectedFrequency = frequency * 0.5f;
    const float measuredFrequency = sampleRate / stats.meanPeriodLen;
    const float tolerance = expectedFrequency * 0.08f;

    EXPECT_NEAR(measuredFrequency, expectedFrequency, tolerance)
        << "Frequency should be halved (octave down) with pitch ratio 0.5";

    float rms = 0.0f;
    for (const auto sample : leftOut)
    {
        rms += sample * sample;
    }
    rms = std::sqrt(rms / bufferSize);
    EXPECT_GT(rms, 0.3f) << "Signal should have sufficient energy";
}

TEST_F(ResamplingPitchShifterTest, StereoSymmetry)
{
    constexpr size_t bufferSize = 2048;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_pitchShifter->setPitchRatio(1.5f);
    m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), bufferSize);

    for (size_t i = 0; i < bufferSize; ++i)
    {
        EXPECT_NEAR(leftOut[i], rightOut[i], 0.001f)
            << "Stereo channels should be identical at sample " << i;
    }
}

TEST_F(ResamplingPitchShifterTest, StopsAtEnd)
{
    constexpr size_t bufferSize = 200000;
    std::vector<float> leftOut(bufferSize);
    std::vector<float> rightOut(bufferSize);

    m_pitchShifter->setPitchRatio(1.0f);
    const bool success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), bufferSize);

    EXPECT_FALSE(success);
    EXPECT_TRUE(m_pitchShifter->isDone());
}

TEST_F(ResamplingPitchShifterTest, VariablePitchRatio)
{
    constexpr size_t chunkSize = 8000;
    std::vector<float> leftOut(chunkSize);
    std::vector<float> rightOut(chunkSize);

    m_pitchShifter->setPitchRatio(1.0f);
    bool success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), chunkSize);
    EXPECT_TRUE(success);

    auto stats1 = calculateZeroCrossingStatistics(leftOut.data(), chunkSize, true);
    const float freq1 = sampleRate / stats1.meanPeriodLen;

    m_pitchShifter->setPitchRatio(1.5f);
    success = m_pitchShifter->produceSamples(leftOut.data(), rightOut.data(), chunkSize);
    EXPECT_TRUE(success);

    auto stats2 = calculateZeroCrossingStatistics(leftOut.data(), chunkSize, true);
    const float freq2 = sampleRate / stats2.meanPeriodLen;

    const float expectedRatio = 1.5f;
    const float actualRatio = freq2 / freq1;
    const float tolerance = expectedRatio * 0.1f;

    EXPECT_NEAR(actualRatio, expectedRatio, tolerance)
        << "Pitch ratio change should affect output frequency proportionally";
}

} // namespace AbacDsp::Test
