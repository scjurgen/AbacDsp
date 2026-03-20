#include "gtest/gtest.h"

#include "Analysis/SimpleStats.h"
#include "Analysis/ZeroCrossings.h"
#include "Numbers/Convert.h"

#include "Filters/SvfResoBP.h"

namespace AbacDsp::Test
{
constexpr float sampleRate{48000.f};

TEST(SvfResoBPTest, responseDecay)
{
    for (size_t n = 21; n < 100; ++n)
    {
        const float f = Convert::noteToFrequency(static_cast<float>(n));
        SvfResoBP sut{sampleRate};
        sut.setByDecay(0, f, 1.f);

        float maxInit = 0.f;
        const float periodLength = sampleRate / f;
        size_t localCount = 0;
        float localMax = 0.f;
        float lastMax = 0.f;

        for (size_t i = 0; i < 48000; ++i)
        {
            if (localCount == 0)
            {
                // hop from valley to valley (or hill to hill) and get the local maximum
                lastMax = localMax;
                localMax = 0.f;
                localCount = static_cast<size_t>(periodLength);
            }
            else
            {
                localCount--;
            }

            const float res = sut.step(i < 3 ? 1024.f : 0.f);
            maxInit = std::max(res, maxInit);
            localMax = std::max(res, localMax);
        }

        EXPECT_GT(std::log10(maxInit) * 20, -1.6f) << "failed at note " << n;
        EXPECT_LT(std::log10(lastMax) * 20, -55.f) << "failed at note " << n;
    }
}

TEST(SvfResoBPTest, quickReleaseDamping)
{
    constexpr float f = 200.f;
    SvfResoBP sut{sampleRate};
    sut.damp(false);
    sut.setByDecay(0, f, 1.f);
    sut.setByDecay(1, f, 0.01f);

    for (size_t i = 0; i < 4800; ++i)
    {
        sut.step(i < 7 ? 1024.f : 0.f);
    }

    float preDecayMax = sut.step(0.f);
    for (size_t i = 0; i < 480; ++i)
    {
        preDecayMax = std::max(std::abs(sut.step(0.f)), preDecayMax);
    }

    sut.damp(true);
    for (size_t i = 0; i < 700; ++i)
    {
        sut.step(0.f);
    }

    float currentMax = sut.step(0.f);
    for (size_t i = 0; i < 400; ++i)
    {
        currentMax = std::max(std::abs(sut.step(0.f)), currentMax);
    }

    EXPECT_GT(preDecayMax, 1.0f);
    EXPECT_LT(currentMax, 0.01f);
}


TEST(SvfResoBPTest, pitchBendFrequencyAccuracy)
{
    constexpr float baseFreq = 440.f;
    constexpr float decayTime = 2.f;
    constexpr size_t stabilizeMs = 10;
    constexpr size_t measureMs = 1000;

    const size_t stabilizeSamples = (stabilizeMs * sampleRate) / 1000;
    const size_t measureSamples = (measureMs * sampleRate) / 1000;

    auto testPitchBend = [&](const float cents, const float expectedRatio)
    {
        SvfResoBP sut{sampleRate};
        sut.setByDecay(0, baseFreq, decayTime);
        sut.pitchBendCents(cents);

        std::vector<float> signal(stabilizeSamples + measureSamples);
        for (size_t i = 0; i < stabilizeSamples; ++i)
        {
            signal[i] = sut.step(i == 0 ? 1024.f : 0.f);
        }
        for (size_t i = stabilizeSamples; i < stabilizeSamples + measureSamples; ++i)
        {
            signal[i] = sut.step(0.f);
        }

        const auto stats = calculateZeroCrossingStatistics(signal.data() + stabilizeSamples, measureSamples, true);

        const float measuredPeriod = stats.meanPeriodLen;
        const float measuredFreq = sampleRate / measuredPeriod;
        const float expectedFreq = baseFreq * expectedRatio;
        const float errorPercent = std::abs(measuredFreq - expectedFreq) / expectedFreq * 100.f;
        EXPECT_LT(errorPercent, 2.f) << "cents=" << cents << ", measured=" << measuredFreq
                                     << "Hz, expected=" << expectedFreq << "Hz";
    };

    testPitchBend(-1200.f, 0.5f);
    testPitchBend(-200.f, 0.8908987181f);
    testPitchBend(0.f, 1.f);
    testPitchBend(200.f, 1.1224620483f);
    testPitchBend(1200.f, 2.f);
}


TEST(SvfResoBPTest, checkCompensationModelForWaveExcitation)
{
    SimpleStats<float> statsAll;
    constexpr auto decayStart = 0.0078125f;
    constexpr auto decayEnd = 64.f;
    for (float n = 0; n <= 126; n += 2.f) // NOLINT(cert-flp30-c, bugprone-float-loop-counter)
    {
        const auto freq = Convert::noteToFrequency(n);

        for (float decay = decayStart; decay <= decayEnd;
             decay *= 1.2f) // NOLINT(cert-flp30-c, bugprone-float-loop-counter)
        {
            SvfResoBP sut{sampleRate};
            sut.setByDecay(0, freq, decay);
            const auto compFactor = ResonanceCompensation::compensate(n, decay);
            sut.reset(0, compFactor);

            float maxValue = 0;
            for (int decayTime = 0; decayTime < 2000; ++decayTime)
            {
                std::array<float, 1> out{};
                sut.process0(out.data(), 1);
                const auto v = out[0];
                maxValue = std::max(std::abs(v), maxValue);
            }
            statsAll.addDataPoint(maxValue);
        }
    }
    EXPECT_NEAR(statsAll.getMean(), 0.9f, 0.15f);
    EXPECT_GT(statsAll.getMin(), 0.05f);
    EXPECT_LT(statsAll.getMax(), 1.15f);
    statsAll.setPrecision(3);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
}

TEST(SvfResoBPTest, pitchBendUpRemainsStable)
{
    const float decay = 5.088f;
    const auto freq = Convert::noteToFrequency(static_cast<float>(48));

    const auto compFactor = ResonanceCompensation::compensate(static_cast<float>(48), decay);
    float maxValue = 0;
    int decayTime = 0;
    SvfResoBP sut{sampleRate};
    sut.setByDecay(0, freq, decay);
    sut.reset(0, compFactor);
    for (size_t j = 0; j < 600; ++j)
    {
        decayTime++;
        std::array<float, 1> out{};
        sut.process0(out.data(), 1);
        const auto v = out[0];
        maxValue = std::max(std::abs(v), maxValue);
    }
    sut.pitchBendCents(1200);
    {
        float pitchMaxValue = 0;
        for (size_t k = 0; k < 126000; ++k)
        {
            decayTime++;
            std::array<float, 1> out{};
            sut.process0(out.data(), 1);
            const auto v = out[0];
            pitchMaxValue = std::max(std::abs(v), pitchMaxValue);
        }
    }
    {
        float pitchMaxValue = 0;
        for (size_t k = 0; k < 1260000; ++k)
        {
            decayTime++;
            std::array<float, 1> out{};
            sut.process0(out.data(), 1);
            const auto v = out[0];
            pitchMaxValue = std::max(std::abs(v), pitchMaxValue);
        }
    }
    for (int i = -12; i <= 12; ++i)
    {
        SvfResoBP sut{sampleRate};
        sut.setByDecay(0, freq, decay);
        sut.reset(0, compFactor);
        for (size_t j = 0; j < 600; ++j)
        {
            decayTime++;
            std::array<float, 1> out{};
            sut.process0(out.data(), 1);
            const auto v = out[0];
            maxValue = std::max(std::abs(v), maxValue);
        }

        float pitchMaxValue = 0;
        float pb = 0;
        for (size_t j = 0; j < 1200; ++j)
        {
            pb += i;

            sut.pitchBendCents(std::clamp(pb, -1200.f, 1200.f));
            for (size_t k = 0; k < 1260; ++k)
            {
                decayTime++;
                std::array<float, 1> out{};
                sut.process0(out.data(), 1);
                const auto v = out[0];
                pitchMaxValue = std::max(std::abs(v), pitchMaxValue);
            }
        }
        EXPECT_LT(pitchMaxValue, maxValue);
    }
}

}
