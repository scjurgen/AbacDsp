#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <string_view>

#include "Filters/LadderFilter.h"


class FilterTestFixture : public ::testing::Test
{
protected:
    static constexpr float cfStepMultiplier = 1.2f;
    static constexpr float SampleRate = 48000.0f;
    static constexpr size_t BlockSize = 4000;
    static constexpr float InputDb = -40.0f;

    void runMagnitudeTest(size_t index, float resonance, float maxDbError)
    {
        const float inputAmplitude = std::pow(10.0f, InputDb / 20.0f);
        AbacDsp::FourStageFilterTheoretical<float> theoretical(SampleRate, {0, 0, 0, 0, 1});
        const auto config = AbacDsp::poleMixingList[index];
        {
            for (float hz = 50; hz <= 10000.f; hz *= cfStepMultiplier)
            {
                float dbErrorSum{0.f};
                size_t countSums = 0;
                theoretical.setCoefficients(config.cf);
                for (float cf = 50.f; cf <= 10000.f; cf *= cfStepMultiplier)
                {
                    const float magnitude = static_cast<float>(theoretical.magnitudeBP(
                        AbacDsp::Filter1Pole4StageSmooth::adaptResonanceFrequency(cf), hz, resonance));
                    float expectedDb = std::log10(magnitude) * 20.0f;
                    if (expectedDb < -30)
                    {
                        continue;
                    }
                    // ignore if the expectedDb is very low
                    AbacDsp::Filter1Pole4StageSmooth filter{SampleRate};
                    filter.setFilterCoefficients(config.cf);
                    filter.setCutoffFrequency(cf);
                    filter.setResonance(resonance);

                    std::vector<float> wave(BlockSize);
                    for (size_t i = 0; i < BlockSize; ++i)
                    {
                        wave[i] = inputAmplitude * std::sin(
                                      2 * std::numbers::pi_v<float> * hz * static_cast<float>(i) / SampleRate);
                    }

                    filter.processBlock(wave.data(), wave.data(), wave.size());

                    const size_t half = wave.size() / 2;
                    auto minmax = std::minmax_element(wave.begin() + half, wave.end());
                    float maxAbs = std::max(std::abs(*minmax.first), std::abs(*minmax.second));
                    float db = std::log10(maxAbs / inputAmplitude) * 20.0f;
                    // adapt db for filter types
                    dbErrorSum += std::abs(db - expectedDb);
                    countSums++;
                }
                if (countSums)
                {
                    EXPECT_NEAR(dbErrorSum / countSums, 0, maxDbError);
                }
            }
        }
    }
};

TEST_F(FilterTestFixture, Resonance0p0)
{
    constexpr float resonance{0.f};
    constexpr float maxDbError{0.01f};
    runMagnitudeTest(AbacDsp::findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(AbacDsp::findFilterIndex("BP4"), resonance, maxDbError);
    runMagnitudeTest(AbacDsp::findFilterIndex("HP4"), resonance, maxDbError);
    runMagnitudeTest(AbacDsp::findFilterIndex("AP4"), resonance, maxDbError);
}

TEST_F(FilterTestFixture, Resonance1)
{
    constexpr float resonance{1.f};
    constexpr float maxDbError{0.4f};
    runMagnitudeTest(AbacDsp::findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(AbacDsp::findFilterIndex("HP4"), resonance, maxDbError);
}

TEST_F(FilterTestFixture, Resonance2)
{
    constexpr float resonance{2.f};
    constexpr float maxDbError{1.5f};
    runMagnitudeTest(AbacDsp::findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(AbacDsp::findFilterIndex("BP4"), resonance, maxDbError);
}

TEST_F(FilterTestFixture, Resonance3p5)
{
    constexpr float resonance{3.5f};
    constexpr float maxDbError{3.5f};
    runMagnitudeTest(AbacDsp::findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(AbacDsp::findFilterIndex("AP4"), resonance, maxDbError);
}

TEST(LadderFilterTests, magnitudeFunction)
{
    constexpr float sampleRate{48000.f};
    AbacDsp::FourStageFilterTheoretical<float> sut(sampleRate, {0, 0, 0, 0, 1});
    for (float f = 250; f < 5000; f *= 1.2f)
    {
        constexpr float cutoff{1000.f};
        const float analog = 20.0f * std::log10(sut.magnitude(cutoff, f, 0.f));
        const float digital = 20.0f * std::log10(sut.magnitudeBP(cutoff, f, 0.f));
        EXPECT_NEAR(analog, digital, 1.f) << "failed at " << f;
    }
    // account for differences at higher frequencies
    for (float f = 5000; f < 12000; f *= 1.2f)
    {
        constexpr float cutoff{1000.f};
        const float analog = 20.0f * std::log10(sut.magnitude(cutoff, f, 0.f));
        const float digital = 20.0f * std::log10(sut.magnitudeBP(cutoff, f, 0.f));
        EXPECT_NEAR(analog, digital, 3.f) << "failed at " << f;
    }
}