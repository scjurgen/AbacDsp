#include "NonLinear/SimpleHysteresis.h"
#include "Analysis/FftSmall.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace AbacDsp::Test
{
void renderWithSineWave(std::vector<float>& target, const double sampleRate, const double frequency)
{
    double phase = 0.0;
    const double advance = frequency / sampleRate;

    for (size_t frameIdx = 0; frameIdx < target.size(); ++frameIdx)
    {
        target[frameIdx] = static_cast<float>(sin(phase * M_PI * 2));
        phase += advance;
        if (phase > 1.0)
        {
            phase -= 1.0;
        }
    }
}


TEST(SimpleHysteresisTest, checkNonLinearity)
{
    const float f = 1000.f;
    const float a = 18000 - 1.f * 12000;
    const float d = 18000 - 1.f * 6000;
    constexpr float sampleRate{48000};

    AbacDsp::SimpleHysteresis sut{sampleRate};
    sut.setFrequencyResponse(a, d);
    std::vector<float> sutBuffer(20000);
    renderWithSineWave(sutBuffer, sampleRate, f);
    constexpr size_t windowSize{4096};
    const size_t windowStart{sutBuffer.size() - windowSize};
    std::transform(sutBuffer.begin(), sutBuffer.end(), sutBuffer.begin(),
                   [&sut](const float in) { return sut.step(in); });
    std::vector<float> analyse(sutBuffer.begin() + windowStart, sutBuffer.begin() + windowStart + windowSize);
    std::vector<float> fftResult;
    BasicFFT::realDataToMagnitude(analyse, fftResult);
    const auto maxPeak = static_cast<size_t>(round(f / sampleRate * windowSize));

    EXPECT_GT(fftResult[maxPeak], fftResult[maxPeak - 4]);
    EXPECT_GT(fftResult[maxPeak], fftResult[maxPeak + 4]);

    EXPECT_GT(std::log10(fftResult[maxPeak]) * 20, 40);
    for (size_t peakCnt = 2; peakCnt < 8; ++peakCnt)
    {
        // check peaks and valleys of overtones created
        EXPECT_GT(std::log10(fftResult[maxPeak * peakCnt]) * 20, 40 / peakCnt);
        EXPECT_LT(std::log10(fftResult[maxPeak * peakCnt + maxPeak / peakCnt]) * 20, -30);
    }
}

TEST(SimpleHysteresisTest, checkNeutralOnZero)
{
    constexpr float f = 960.f;
    constexpr float a = 18000.f;
    constexpr float d = 18000.f;
    constexpr float sampleRate{48000};

    AbacDsp::SimpleHysteresis sut{sampleRate};
    sut.setFrequencyResponse(a, d);
    std::vector<float> sutBuffer(20000);
    renderWithSineWave(sutBuffer, sampleRate, f);
    constexpr size_t windowSize{4096};
    const size_t windowStart{sutBuffer.size() - windowSize};
    std::transform(sutBuffer.begin(), sutBuffer.end(), sutBuffer.begin(),
                   [&sut](const float in) { return sut.step(in); });
    std::vector<float> analyse(sutBuffer.begin() + windowStart, sutBuffer.begin() + windowStart + windowSize);
    std::vector<float> fftResult;
    BasicFFT::realDataToMagnitude(analyse, fftResult);
    const auto maxPeak = static_cast<size_t>(round(f / sampleRate * windowSize));

    EXPECT_GT(fftResult[maxPeak], fftResult[maxPeak - 4]);
    EXPECT_GT(fftResult[maxPeak], fftResult[maxPeak + 4]);
    // decaying noise floor
    for (size_t i = maxPeak * 2; i < fftResult.size(); ++i)
    {
        EXPECT_LT(std::log10(fftResult[i]) * 20, -50) << "failed at " << i;
    }
}
}