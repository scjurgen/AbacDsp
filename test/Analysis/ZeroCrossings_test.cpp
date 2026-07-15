#include <array>
#include <span>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/ZeroCrossings.h"
#include "NaiveGenerators/Generator.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Test
{
constexpr float sampleRate{48000.f};

// N periods of a period-4 square wave (optionally DC-shifted). Rising zero
// crossings land at samples 2, 6, 10, ... so every period length is exactly 4.
[[nodiscard]] inline std::vector<float> squarePeriods(const size_t periods, const float dc = 0.f)
{
    std::vector<float> v;
    for (size_t p = 0; p < periods; ++p)
    {
        for (const float x : {-1.f, -1.f, 1.f, 1.f})
        {
            v.push_back(x + dc);
        }
    }
    return v;
}


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

TEST(ZeroCrossingsTest, calculateDcEmptyAndNonEmpty)
{
    EXPECT_FLOAT_EQ(calculateDC<float>(nullptr, 0), 0.f); // size == 0 guard
    const std::array<float, 4> d{1.f, 2.f, 3.f, 4.f};
    EXPECT_FLOAT_EQ(calculateDC(d.data(), d.size()), 2.5f);
}

TEST(ZeroCrossingsTest, findFirstZeroCrossingEdgesAndBoolOverload)
{
    const auto sig = squarePeriods(3);
    EXPECT_EQ(findFirstZeroCrossingNP(sig.data(), sig.size(), [](const float x) { return x; }), 2u);

    // maxSize < 2 returns maxSize unchanged
    const std::array<float, 1> one{-1.f};
    EXPECT_EQ(findFirstZeroCrossingNP(one.data(), one.size(), [](const float x) { return x; }), 1u);
    EXPECT_EQ(findFirstZeroCrossingNP(one.data(), size_t{0}, [](const float x) { return x; }), 0u);

    // no rising crossing returns maxSize
    const std::array<float, 4> allNeg{-1.f, -2.f, -1.f, -2.f};
    EXPECT_EQ(findFirstZeroCrossingNP(allNeg.data(), allNeg.size(), [](const float x) { return x; }), allNeg.size());

    // bool overload: both removeDC branches
    EXPECT_EQ(findFirstZeroCrossingNP(sig.data(), sig.size(), false), 2u);
    const auto shifted = squarePeriods(3, 2.f);
    EXPECT_EQ(findFirstZeroCrossingNP(shifted.data(), shifted.size(), true), 2u);              // DC removed
    EXPECT_EQ(findFirstZeroCrossingNP(shifted.data(), shifted.size(), false), shifted.size()); // stays positive
}

TEST(ZeroCrossingsTest, periodLengthBoolOverloadAndEmptyCases)
{
    const auto sig = squarePeriods(6);
    EXPECT_NEAR(periodLengthByZeroCrossingAverage(sig.data(), sig.size(), false), 4.f, 1e-5f);

    const auto shifted = squarePeriods(6, 2.f);
    EXPECT_NEAR(periodLengthByZeroCrossingAverage(shifted.data(), shifted.size(), true), 4.f, 1e-5f);

    // no crossing at all -> 0 (firstIndex == size)
    EXPECT_FLOAT_EQ(periodLengthByZeroCrossingAverage(shifted.data(), shifted.size(), false), 0.f);

    // exactly one crossing -> cnt == 0 -> 0
    const std::array<float, 4> single{-1.f, -1.f, 1.f, 1.f};
    EXPECT_FLOAT_EQ(periodLengthByZeroCrossingAverage(single.data(), single.size(), [](const float x) { return x; }),
                    0.f);
}

TEST(ZeroCrossingsTest, statisticsOverloadsAndEmptyCases)
{
    const auto sig = squarePeriods(6); // period lengths all 4, five of them
    const auto shifted = squarePeriods(6, 2.f);

    const auto ptrFalse = calculateZeroCrossingStatistics(sig.data(), sig.size(), false);
    EXPECT_EQ(ptrFalse.periodCount, 5u);
    EXPECT_FLOAT_EQ(ptrFalse.meanPeriodLen, 4.f);
    EXPECT_FLOAT_EQ(ptrFalse.minPeriodLength, 4.f);
    EXPECT_FLOAT_EQ(ptrFalse.maxPeriodLength, 4.f);
    EXPECT_FLOAT_EQ(ptrFalse.standardDeviation, 0.f);

    EXPECT_EQ(calculateZeroCrossingStatistics(shifted.data(), shifted.size(), true).periodCount, 5u);

    // span + preprocess, span + removeDC (both branches)
    EXPECT_EQ(calculateZeroCrossingStatistics(std::span<const float>{sig}, [](const float x) { return x; }).periodCount,
              5u);
    EXPECT_EQ(calculateZeroCrossingStatistics(std::span<const float>{sig}, false).periodCount, 5u);
    EXPECT_EQ(calculateZeroCrossingStatistics(std::span<const float>{shifted}, true).periodCount, 5u);

    // contiguous-iterator + removeDC (both branches)
    EXPECT_EQ(calculateZeroCrossingStatistics<float>(sig.begin(), sig.end(), false).periodCount, 5u);
    EXPECT_EQ(calculateZeroCrossingStatistics<float>(shifted.begin(), shifted.end(), true).periodCount, 5u);

    // no crossing -> empty stats (firstIndex == size)
    EXPECT_EQ(calculateZeroCrossingStatistics(shifted.data(), shifted.size(), false).periodCount, 0u);

    // one crossing -> empty period list -> empty stats
    const std::array<float, 4> single{-1.f, -1.f, 1.f, 1.f};
    EXPECT_EQ(calculateZeroCrossingStatistics(single.data(), single.size(), false).periodCount, 0u);
}
}