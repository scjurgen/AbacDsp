#include "gtest/gtest.h"

#include "Analysis/ZeroCrossings.h"
#include "NaiveGenerators/Generator.h"
#include "Numbers/Convert.h"

#include <array>

namespace AbacDsp::Test
{
constexpr float sampleRate{48000.f};


TEST(ZeroCrossingsTest, correctPeriod)
{
    for (size_t i = 10; i < 127; ++i)
    {
        const float frequency = Convert::noteToFrequency(static_cast<float>(i));

        Generator<Wave::Sine> wg{sampleRate};
        std::vector wave(10000 + static_cast<int>(30 * sampleRate / frequency), 0.f);
        wg.renderWithFrequency(wave.begin(), wave.end(), frequency, 1);

        const auto periodLength =
            periodLengthByZeroCrossingAverage(wave.data(), wave.size(), [](const float in) { return in; });
        // check the ratio is 1.0, not the single numbers
        EXPECT_NEAR(periodLength / (sampleRate / frequency), 1.f, 1E-4f);
    }
}

TEST(ZeroCrossingsTest, correctPeriodWithManualDcElemination)
{
    for (size_t i = 10; i < 127; ++i)
    {
        const float frequency = Convert::noteToFrequency(static_cast<float>(i));

        Generator<Wave::Sine> wg{sampleRate};
        std::vector<float> wave(10000 + static_cast<int>(30 * sampleRate / frequency), 0.f);
        wg.renderWithFrequency(wave.begin(), wave.end(), frequency, 1);
        std::transform(wave.begin(), wave.end(), wave.begin(), [](const float in) { return in + 2.f; });
        const auto periodLength =
            periodLengthByZeroCrossingAverage(wave.data(), wave.size(), [](const float in) { return in - 2; });
        EXPECT_NEAR(periodLength / (sampleRate / frequency), 1.f, 1E-4f);
    }
}

TEST(ZeroCrossingsTest, correctPeriodWithAutoDcElemination)
{
    for (size_t i = 10; i < 127; ++i)
    {
        const float frequency = Convert::noteToFrequency(static_cast<float>(i));

        Generator<Wave::Sine> wg{sampleRate};
        std::vector<float> wave(10000 + static_cast<int>(30 * sampleRate / frequency), 0.f);
        wg.renderWithFrequency(wave.begin(), wave.end(), frequency, 1);
        std::transform(wave.begin(), wave.end(), wave.begin(), [](const float in) { return in + 2.f; });
        const auto periodLength = periodLengthByZeroCrossingAverage(wave.data(), wave.size(), true);
        EXPECT_NEAR(periodLength / (sampleRate / frequency), 1.f, 1E-4f);
    }
}

TEST(ZeroCrossingsTest, statisticsConstantFrequency)
{
    constexpr float frequency{440.f}; // A4
    constexpr float expectedPeriodLength = sampleRate / frequency;
    constexpr size_t periodsGenerated{40};

    Generator<Wave::Sine> wg{sampleRate};
    std::vector<float> wave(static_cast<size_t>(periodsGenerated * expectedPeriodLength), 0.f);
    wg.renderWithFrequency(wave.begin(), wave.end(), frequency, 1.0f);

    const auto stats = calculateZeroCrossingStatistics(wave.data(), wave.size());

    EXPECT_NEAR(stats.meanPeriodLen, expectedPeriodLength, 0.1f);
    // expect strong deviation because of discrete nature of zero crossing samples
    // 48000/440 = 109.09 -> every ~11 is 110
    EXPECT_GT(stats.standardDeviation, 0.2f);
    EXPECT_EQ(stats.periodCount, periodsGenerated - 2); // removes first and last ZC
    EXPECT_NEAR(stats.maxPeriodLength - stats.minPeriodLength, 1, 1E-5f);
}

TEST(ZeroCrossingsTest, statisticsModulatedFrequency)
{
    constexpr float centerFrequency{440.f};
    constexpr float modulationFrequency{5.f};
    constexpr float modulationDepth{0.2f};

    // Generate frequency modulated sine wave
    std::vector<float> wave(2 * sampleRate, 0.f);

    constexpr float pi2 = 2.0f * std::numbers::pi_v<float>;
    constexpr float modulationPhaseIncrement = pi2 * modulationFrequency / sampleRate;
    float phase = 0.0f;
    float modulationPhase = 0.0f;

    for (size_t i = 0; i < wave.size(); ++i)
    {
        const float modulation = std::sin(modulationPhase) * modulationDepth;
        const float instantaneousFrequency = centerFrequency * (1.0f + modulation);
        const float phaseIncrement = pi2 * instantaneousFrequency / sampleRate;
        wave[i] = std::sin(phase);
        phase += phaseIncrement;
        modulationPhase += modulationPhaseIncrement;
    }

    const auto stats = calculateZeroCrossingStatistics(wave, [](const float in) { return in; });

    constexpr auto expectedMean = sampleRate / centerFrequency;
    EXPECT_NEAR(stats.meanPeriodLen, expectedMean, 0.1f);
    EXPECT_GT(stats.standardDeviation, 15.0f);
    EXPECT_EQ(stats.periodCount, 878);

    constexpr auto expectedMin = sampleRate / (centerFrequency * (1.0f + modulationDepth));
    constexpr auto expectedMax = sampleRate / (centerFrequency * (1.0f - modulationDepth));
    EXPECT_LT(stats.minPeriodLength, expectedMin + 1);
    EXPECT_GT(stats.maxPeriodLength, expectedMax - 1);
    EXPECT_NEAR(stats.minPeriodLength, expectedMin, 1.f);
    EXPECT_NEAR(stats.maxPeriodLength, expectedMax, 1.f);
}
}