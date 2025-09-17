#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include <algorithm>
#include "Analysis/OctaveBandAnalyzer.h"

namespace AbacDsp::Test
{

constexpr size_t TestBlockSize = 4;
constexpr float TestSampleRate = 48000.0f;
constexpr size_t NumCalls = 12000;

class OctaveBandAnalyzerTest : public ::testing::Test
{
  protected:
    std::array<std::array<float, TestBlockSize>, 2> generateSineWave(float frequency, size_t phase,
                                                                     float amplitude = 1.0f)
    {
        std::array<std::array<float, TestBlockSize>, 2> result{};
        const float omega = 2.0f * std::numbers::pi_v<float> * frequency / TestSampleRate;

        for (size_t i = 0; i < TestBlockSize; ++i)
        {
            const float sample = amplitude * std::sin(omega * static_cast<float>(phase + i));
            result[0][i] = sample;
            result[1][i] = sample;
        }
        return result;
    }

    std::array<std::array<float, TestBlockSize>, 2> generateSilence()
    {
        std::array<std::array<float, TestBlockSize>, 2> result{};
        return result;
    }

    std::array<std::array<float, TestBlockSize>, 2> generateNoise()
    {
        std::array<std::array<float, TestBlockSize>, 2> result{};
        for (size_t i = 0; i < TestBlockSize; ++i)
        {
            result[0][i] = ((rand() % RAND_MAX) / static_cast<float>(RAND_MAX)) * 2 - 1;
            result[1][i] = ((rand() % RAND_MAX) / static_cast<float>(RAND_MAX)) * 2 - 1;
        }
        return result;
    }

    void feedSignal(auto& analyzer, float frequency, float amplitude = 1.0f)
    {
        for (size_t call = 0; call < NumCalls; ++call)
        {
            auto signal = generateSineWave(frequency, call * TestBlockSize, amplitude);
            analyzer.process(signal);
        }
    }

    void feedSilence(auto& analyzer)
    {
        auto silence = generateSilence();
        for (size_t call = 0; call < NumCalls; ++call)
        {
            analyzer.process(silence);
        }
    }

    void feedNoise(auto& analyzer)
    {
        for (size_t call = 0; call < NumCalls; ++call)
        {
            auto noise = generateNoise();
            analyzer.process(noise);
        }
    }

    size_t findPeakBand(const auto& levels)
    {
        size_t peakBand = 0;
        float peakLevel = 0.0f;
        for (size_t i = 0; i < levels.size(); ++i)
        {
            if (levels[i] > peakLevel)
            {
                peakLevel = levels[i];
                peakBand = i;
            }
        }
        return peakBand;
    }

    float midiNoteToFrequency(int midiNote)
    {
        return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
    }
};

TEST_F(OctaveBandAnalyzerTest, TemplateConstraints)
{
    EXPECT_TRUE((std::is_constructible_v<OctaveBandAnalyzer<TestBlockSize, 1>, float>) );
    EXPECT_TRUE((std::is_constructible_v<OctaveBandAnalyzer<TestBlockSize, 3>, float>) );
    EXPECT_TRUE((std::is_constructible_v<OctaveBandAnalyzer<TestBlockSize, 6>, float>) );
}

TEST_F(OctaveBandAnalyzerTest, CorrectNumberOfBins)
{
    OctaveBandAnalyzer<TestBlockSize, 1> analyzer1(TestSampleRate);
    EXPECT_EQ(analyzer1.NumBins, 10);

    OctaveBandAnalyzer<TestBlockSize, 3> analyzer3(TestSampleRate);
    EXPECT_EQ(analyzer3.NumBins, 30);

    OctaveBandAnalyzer<TestBlockSize, 6> analyzer6(TestSampleRate);
    EXPECT_EQ(analyzer6.NumBins, 60);
}

TEST_F(OctaveBandAnalyzerTest, CenterFrequenciesAvailable)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer(TestSampleRate);
    auto centerFreqs = analyzer.getCenterFrequencies();

    EXPECT_EQ(centerFreqs.size(), 30);

    for (size_t i = 1; i < centerFreqs.size(); ++i)
    {
        EXPECT_GT(centerFreqs[i], centerFreqs[i - 1]) << "Band " << i << " frequency not increasing";
    }

    EXPECT_GT(centerFreqs[0], 18.0f);
    EXPECT_LT(centerFreqs[centerFreqs.size() - 1], TestSampleRate / 2.0f);
}

TEST_F(OctaveBandAnalyzerTest, NoiseIntoBuffers)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer(TestSampleRate);
    feedNoise(analyzer);
    auto levels = analyzer.getLevels();

    for (size_t i = 0; i < levels.size(); ++i)
    {
        EXPECT_GT(levels[i], 0.0f) << "Band " << i << " is silent";
    }
}

TEST_F(OctaveBandAnalyzerTest, SilenceProducesZeroLevels)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer(TestSampleRate);
    feedSilence(analyzer);
    auto levels = analyzer.getLevels();

    for (size_t i = 0; i < levels.size(); ++i)
    {
        EXPECT_NEAR(levels[i], 0.0f, 1e-6f) << "Band " << i << " not silent";
    }
}

TEST_F(OctaveBandAnalyzerTest, SineWaveDetectedInCorrectBand)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer(TestSampleRate);
    auto cf = analyzer.getCenterFrequencies();
    std::cout << "f\t";
    for (size_t i = 0; i < cf.size(); ++i)
    {
        std::cout << cf[i] << "\t";
    }
    std::cout << std::endl;
    for (int midiNote = 20; midiNote < 122; midiNote += 4)
    {
        const float frequency = midiNoteToFrequency(midiNote);
        if (frequency < 18.0f || frequency > TestSampleRate / 2.0f)
        {
            continue;
        }
        std::cout << frequency << "\t";

        analyzer.reset();
        feedSignal(analyzer, frequency, 0.7f);
        auto levels = analyzer.getLevels();
        auto centerFreqs = analyzer.getCenterFrequencies();
        for (size_t i = 0; i < levels.size(); ++i)
        {
            std::cout << std::log10(levels[i]) * 20 << "\t";
        }
        std::cout << std::endl;

        const size_t peakBand = findPeakBand(levels);
        const float peakFreq = centerFreqs[peakBand];

        const float octaveRatio = std::log2(peakFreq / frequency);
        EXPECT_LT(std::abs(octaveRatio), 0.6f) << "MIDI note " << midiNote << " (" << frequency << " Hz) peaked at "
                                               << peakFreq << " Hz (bin " << peakBand << ")";

        EXPECT_GT(levels[peakBand], 0.2f)
            << "MIDI note " << midiNote << " (" << frequency << " Hz) has insufficient level";
    }
}

TEST_F(OctaveBandAnalyzerTest, MultipleProcessCallsWork)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer(TestSampleRate);

    feedSignal(analyzer, 1000.0f, 0.5f);
    auto levels1 = analyzer.getLevels();

    analyzer.reset();

    feedSignal(analyzer, 2000.0f, 0.5f);
    auto levels2 = analyzer.getLevels();

    auto peak1Idx = std::max_element(levels1.begin(), levels1.end()) - levels1.begin();
    auto peak2Idx = std::max_element(levels2.begin(), levels2.end()) - levels2.begin();
    EXPECT_NE(peak1Idx, peak2Idx);
}

TEST_F(OctaveBandAnalyzerTest, DifferentBinsPerOctave)
{
    OctaveBandAnalyzer<TestBlockSize, 1> analyzer1(TestSampleRate);
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer3(TestSampleRate);

    feedSignal(analyzer1, 1000.0f, 0.5f);
    feedSignal(analyzer3, 1000.0f, 0.5f);

    auto levels1 = analyzer1.getLevels();
    auto levels3 = analyzer3.getLevels();

    float total1 = 0.0f;
    float total3 = 0.0f;
    for (auto level : levels1)
        total1 += level;
    for (auto level : levels3)
        total3 += level;

    EXPECT_GT(total1, 0.1f);
    EXPECT_GT(total3, 0.1f);
}

TEST_F(OctaveBandAnalyzerTest, SampleRateChange)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer(TestSampleRate);
    feedSignal(analyzer, 1000.0f, 0.5f);

    auto centerFreqs1 = analyzer.getCenterFrequencies();

    analyzer.setSampleRate(96000.0f);
    auto centerFreqs2 = analyzer.getCenterFrequencies();

    bool different = false;
    for (size_t i = 0; i < centerFreqs1.size(); ++i)
    {
        if (std::abs(centerFreqs1[i] - centerFreqs2[i]) > 0.01f)
        {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(OctaveBandAnalyzerTest, AmplitudeScaling)
{
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer1(TestSampleRate);
    OctaveBandAnalyzer<TestBlockSize, 3> analyzer2(TestSampleRate);

    feedSignal(analyzer1, 1000.0f, 0.5f);
    auto levels1 = analyzer1.getLevels();

    feedSignal(analyzer2, 1000.0f, 1.0f);
    auto levels2 = analyzer2.getLevels();

    auto peak1 = *std::max_element(levels1.begin(), levels1.end());
    auto peak2 = *std::max_element(levels2.begin(), levels2.end());

    EXPECT_NEAR(peak2 / peak1, 2.0f, 0.3f);
}

} // namespace AbacDsp::Test
