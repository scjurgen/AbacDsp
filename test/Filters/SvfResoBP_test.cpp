#include "Analysis/ZeroCrossings.h"
#include "Filters/SvfResoBP.h"
#include "Numbers/Convert.h"
#include "gtest/gtest.h"

namespace AbacDsp::Test
{

TEST(SvfResoBPTest, responseDecay)
{
    constexpr float sampleRate{48000.f};
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
    constexpr float sampleRate{48000.f};
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
    EXPECT_LT(currentMax, 1E-4f);
}


TEST(SvfResoBPTest, pitchBendFrequencyAccuracy)
{
    constexpr float sampleRate{48000.f};
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
        sut.pitchBend(cents);

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

}
