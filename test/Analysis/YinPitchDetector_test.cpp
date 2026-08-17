#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/YinPitchDetector.h"
#include "NaiveGenerators/Generator.h"

namespace AbacDsp::Test
{

constexpr float kSampleRate = 48000.0f;
constexpr float kPitchTolerance = 2.0f; // Hz tolerance for pitch detection

namespace
{
class YinPitchDetectorTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_detector = std::make_unique<YinPitchDetector>(kSampleRate, 80.0f, 1000.0f, 50.f);
    }

    void feedSignalAndMeasurePitch(const std::vector<float>& signal, float& detectedPitch, float& confidence) const
    {
        std::vector<float> pitchResults;
        pitchResults.reserve(signal.size());

        for (const auto sample : signal)
        {
            std::ignore = m_detector->step(sample);
        }
        for (const auto sample : signal)
        {
            const float pitch = m_detector->step(sample);
            if (m_detector->hasNewPitch())
            {
                pitchResults.push_back(pitch);
            }
        }

        if (pitchResults.empty())
        {
            detectedPitch = 0.0f;
            confidence = 0.0f;
            return;
        }

        // Calculate median pitch for robustness against outliers
        std::ranges::sort(pitchResults);
        detectedPitch = pitchResults[pitchResults.size() / 2];

        // Confidence as inverse of the median absolute deviation: under harsh noise, a lone
        // octave-error window (a well-known YIN failure mode) otherwise dominates a mean-based
        // variance and collapses confidence even when most windows agree with the median.
        std::vector<float> absoluteDeviations;
        absoluteDeviations.reserve(pitchResults.size());
        for (const auto pitch : pitchResults)
        {
            absoluteDeviations.push_back(std::abs(pitch - detectedPitch));
        }
        std::ranges::sort(absoluteDeviations);
        const float medianAbsoluteDeviation = absoluteDeviations[absoluteDeviations.size() / 2];
        confidence = 1.0f / (1.0f + medianAbsoluteDeviation);
    }

    std::unique_ptr<YinPitchDetector> m_detector;
};
}

TEST_F(YinPitchDetectorTest, feedSine)
{
    const std::vector<float> testFrequencies = {110.0f, 220.0f, 440.0f, 880.0f};

    for (const float targetFreq : testFrequencies)
    {
        SCOPED_TRACE("Testing sine wave at " + std::to_string(targetFreq) + " Hz");

        Generator<Wave::Sine> generator(kSampleRate, targetFreq);

        const auto numSamples = static_cast<size_t>(80.0f * kSampleRate / targetFreq);
        std::vector<float> signal(numSamples);
        generator.render(signal.begin(), signal.end());

        float detectedPitch, confidence;
        feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

        EXPECT_GT(detectedPitch, 0.0f) << "No pitch detected for sine wave at " << targetFreq << " Hz";
        EXPECT_NEAR(detectedPitch, targetFreq, kPitchTolerance)
            << "Detected pitch " << detectedPitch << " Hz differs from expected " << targetFreq << " Hz";
        EXPECT_GT(confidence, 0.7f) << "Low confidence " << confidence << " for sine wave detection";
    }
}

TEST_F(YinPitchDetectorTest, feedSineWithNoise)
{
    const float targetFreq = 440.0f;
    const std::vector<float> noiseAmplitudes = {0.05f, 0.1f, 1.0f}; // extreme noise too

    for (const float noiseAmp : noiseAmplitudes)
    {
        SCOPED_TRACE("Testing sine with noise amplitude " + std::to_string(noiseAmp));

        Generator<Wave::Sine> sineGen(kSampleRate, targetFreq);
        Generator<Wave::Noise> noiseGen(kSampleRate, 2000.0f); // High-frequency noise

        const auto numSamples = static_cast<size_t>(40.0f * kSampleRate / targetFreq);
        std::vector<float> signal(numSamples);

        for (size_t i = 0; i < numSamples; ++i)
        {
            signal[i] = sineGen.step() + noiseAmp * noiseGen.step();
        }

        float detectedPitch, confidence;
        feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

        EXPECT_GT(detectedPitch, 0.0f) << "No pitch detected with noise amplitude " << noiseAmp;
        EXPECT_NEAR(detectedPitch, targetFreq, kPitchTolerance * (1.0f + noiseAmp * 2.0f))
            << "Pitch detection failed with noise amplitude " << noiseAmp;

        // Confidence should decrease with increasing noise
        const float expectedMinConfidence = std::max(0.2f, 0.8f - noiseAmp * 3.0f);
        EXPECT_GT(confidence, expectedMinConfidence)
            << "Confidence too low (" << confidence << ") with noise amplitude " << noiseAmp;
    }
}

TEST_F(YinPitchDetectorTest, feedSawtooth)
{
    const std::vector<float> testFrequencies = {110.0f, 220.0f, 440.0f};

    for (const float targetFreq : testFrequencies)
    {
        SCOPED_TRACE("Testing sawtooth wave at " + std::to_string(targetFreq) + " Hz");

        Generator<Wave::Saw> generator(kSampleRate, targetFreq);

        const auto numSamples = static_cast<size_t>(40.0f * kSampleRate / targetFreq);
        std::vector<float> signal(numSamples);
        generator.render(signal.begin(), signal.end());

        float detectedPitch, confidence;
        feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

        EXPECT_GT(detectedPitch, 0.0f) << "No pitch detected for sawtooth at " << targetFreq << " Hz";
        EXPECT_NEAR(detectedPitch, targetFreq, kPitchTolerance * 1.5f) // Slightly more tolerance for harmonics
            << "Sawtooth pitch detection failed";
        EXPECT_GT(confidence, 0.5f) << "Low confidence for sawtooth detection";
    }
}

TEST_F(YinPitchDetectorTest, feedTriangle)
{
    const std::vector<float> testFrequencies = {110.0f, 220.0f, 440.0f};

    for (const float targetFreq : testFrequencies)
    {
        SCOPED_TRACE("Testing triangle wave at " + std::to_string(targetFreq) + " Hz");

        Generator<Wave::Triangle> generator(kSampleRate, targetFreq);

        const auto numSamples = static_cast<size_t>(40.0f * kSampleRate / targetFreq);
        std::vector<float> signal(numSamples);
        generator.render(signal.begin(), signal.end());

        float detectedPitch, confidence;
        feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

        EXPECT_GT(detectedPitch, 0.0f) << "No pitch detected for triangle at " << targetFreq << " Hz";
        EXPECT_NEAR(detectedPitch, targetFreq, kPitchTolerance * 1.5f) << "Triangle pitch detection failed";
        EXPECT_GT(confidence, 0.5f) << "Low confidence for triangle detection";
    }
}

TEST_F(YinPitchDetectorTest, feedSquare)
{
    const std::vector<float> testFrequencies = {110.0f, 220.0f, 440.0f};

    for (const float targetFreq : testFrequencies)
    {
        SCOPED_TRACE("Testing square wave at " + std::to_string(targetFreq) + " Hz");

        Generator<Wave::Square> generator(kSampleRate, targetFreq);

        const auto numSamples = static_cast<size_t>(40.0f * kSampleRate / targetFreq);
        std::vector<float> signal(numSamples);
        generator.render(signal.begin(), signal.end());

        float detectedPitch, confidence;
        feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

        EXPECT_GT(detectedPitch, 0.0f) << "No pitch detected for square wave at " << targetFreq << " Hz";
        EXPECT_NEAR(detectedPitch, targetFreq, kPitchTolerance * 2.0f) << "Square wave pitch detection failed";
        EXPECT_GT(confidence, 0.4f) << "Low confidence for square wave detection";
    }
}

TEST_F(YinPitchDetectorTest, noiseSignal)
{
    Generator<Wave::Noise> generator(kSampleRate, 1000.0f);

    constexpr size_t numSamples = 8192; // Longer sample for noise test
    std::vector<float> signal(numSamples);
    generator.render(signal.begin(), signal.end());

    float detectedPitch, confidence;
    feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

    // Noise should either produce no pitch or very low confidence
    if (detectedPitch > 0.0f)
    {
        EXPECT_LT(confidence, 0.3f) << "Noise signal should have low pitch confidence";
    }
}

TEST_F(YinPitchDetectorTest, silenceSignal)
{
    constexpr size_t numSamples = 4096;
    std::vector<float> signal(numSamples, 0.0f); // Silence

    float detectedPitch, confidence;
    feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

    EXPECT_EQ(detectedPitch, 0.0f) << "Silence should produce no pitch detection";
}

TEST_F(YinPitchDetectorTest, frequencyRange)
{
    // Test frequencies at the edge of the detection range
    const std::vector<float> edgeFrequencies = {85.0f, 950.0f}; // Near min/max range

    for (const float targetFreq : edgeFrequencies)
    {
        SCOPED_TRACE("Testing edge frequency " + std::to_string(targetFreq) + " Hz");

        Generator<Wave::Sine> generator(kSampleRate, targetFreq);

        const auto numSamples = static_cast<size_t>(60.0f * kSampleRate / targetFreq); // More periods
        std::vector<float> signal(numSamples);
        generator.render(signal.begin(), signal.end());

        float detectedPitch, confidence;
        feedSignalAndMeasurePitch(signal, detectedPitch, confidence);

        EXPECT_GT(detectedPitch, 0.0f) << "No pitch detected at edge frequency " << targetFreq << " Hz";
        EXPECT_NEAR(detectedPitch, targetFreq, kPitchTolerance * 3.0f) // More tolerance at edges
            << "Edge frequency detection failed";
    }
}

TEST_F(YinPitchDetectorTest, getLastConfidenceIsHighForACleanSine)
{
    Generator<Wave::Sine> generator(kSampleRate, 220.0f);
    for (size_t i = 0; i < 4096; ++i)
    {
        std::ignore = m_detector->step(generator.step());
    }
    EXPECT_GT(m_detector->getLastConfidence(), 0.9f);
}

TEST_F(YinPitchDetectorTest, getLastConfidenceIsZeroForSilence)
{
    for (size_t i = 0; i < 4096; ++i)
    {
        std::ignore = m_detector->step(0.0f);
    }
    EXPECT_FLOAT_EQ(m_detector->getLastConfidence(), 0.0f);
}

// Regression guard: an unclamped parabolic-interpolation correction had no bound, so a
// near-flat CMNDF curve around an otherwise valid minimum could send the reported pitch
// arbitrarily far off - swept across every waveform since sine alone didn't trigger it.
TEST_F(YinPitchDetectorTest, DetectedPitchNeverEscapesTheConfiguredRange)
{
    const auto sweep = [this](auto& generator, const char* waveName)
    {
        for (size_t i = 0; i < 8000; ++i)
        {
            const float pitch = m_detector->step(generator.step());
            if (m_detector->hasNewPitch() && pitch > 0.0f)
            {
                EXPECT_GE(pitch, 40.0f) << waveName;
                EXPECT_LE(pitch, 2000.0f) << waveName;
            }
        }
    };

    for (float targetFreq = 80.0f; targetFreq <= 1000.0f; targetFreq += 23.7f)
    {
        SCOPED_TRACE("target " + std::to_string(targetFreq) + " Hz");
        m_detector = std::make_unique<YinPitchDetector>(kSampleRate, 80.0f, 1000.0f, 50.f);
        Generator<Wave::Sine> sine(kSampleRate, targetFreq);
        sweep(sine, "sine");
        m_detector = std::make_unique<YinPitchDetector>(kSampleRate, 80.0f, 1000.0f, 50.f);
        Generator<Wave::Saw> saw(kSampleRate, targetFreq);
        sweep(saw, "saw");
        m_detector = std::make_unique<YinPitchDetector>(kSampleRate, 80.0f, 1000.0f, 50.f);
        Generator<Wave::Square> square(kSampleRate, targetFreq);
        sweep(square, "square");
        m_detector = std::make_unique<YinPitchDetector>(kSampleRate, 80.0f, 1000.0f, 50.f);
        Generator<Wave::Triangle> triangle(kSampleRate, targetFreq);
        sweep(triangle, "triangle");
    }
}

}
