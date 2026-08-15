#include "gtest/gtest.h"

#include "Analysis/SimpleStats.h"
#include "Analysis/ZeroCrossings.h"
#include "Filters/SvfResoBP.h"
#include "Numbers/Convert.h"

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
        (void) sut.step(i < 7 ? 1024.f : 0.f);
    }

    float preDecayMax = sut.step(0.f);
    for (size_t i = 0; i < 480; ++i)
    {
        preDecayMax = std::max(std::abs(sut.step(0.f)), preDecayMax);
    }

    sut.damp(true);
    for (size_t i = 0; i < 700; ++i)
    {
        (void) sut.step(0.f);
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
    SvfResoBP sut{sampleRate};
    sut.setByDecay(0, freq, decay);
    sut.reset(0, compFactor);
    for (size_t j = 0; j < 600; ++j)
    {
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
                std::array<float, 1> out{};
                sut.process0(out.data(), 1);
                const auto v = out[0];
                pitchMaxValue = std::max(std::abs(v), pitchMaxValue);
            }
        }
        EXPECT_LT(pitchMaxValue, maxValue);
    }
}

TEST(SvfResoBPTest, triggeredForcesActiveWindowThenGoesInactive)
{
    SvfResoBP sut{sampleRate};
    sut.setByDecay(0, 1000.f, 0.001f); // decayMax = sampleRate * 0.001 = 48 samples
    sut.reset();                       // no state energy, so activity hinges on the forced window
    sut.triggered();                   // decayCount = decayMax > 0

    EXPECT_TRUE(sut.isActive()); // forced-active branch (m_decayCount > 0)

    size_t activeCalls = 1;
    while (sut.isActive())
    {
        ++activeCalls;
        ASSERT_LT(activeCalls, 1000u); // must terminate well before this
    }
    EXPECT_FALSE(sut.isActive()); // quiet state exceeded the inactivity budget
    EXPECT_GE(activeCalls, 48u);  // at least the whole forced window reported active
}

TEST(SvfResoBPTest, activeWhileStateHasEnergy)
{
    SvfResoBP sut{sampleRate};
    sut.setByDecay(0, 1000.f, 0.05f);
    sut.reset();
    (void) sut.step(1024.f); // inject energy without triggering the forced window
    EXPECT_GT(sut.currentMagnitude(), 0.f);
    EXPECT_TRUE(sut.isActive()); // ringing state keeps it active
}

TEST(SvfResoBPTest, setDecayUpdatesResonanceInPlace)
{
    SvfResoBP sut{sampleRate};
    sut.computeCoefficients(0, 1000.f); // establishes g for coefficient set 0
    sut.setDecay(0, 200.f);             // t in ms: updateK recomputes a1..a3 from the stored g
    sut.reset();
    float peak = 0.f;
    for (int i = 0; i < 2000; ++i)
    {
        peak = std::max(std::abs(sut.step(i < 3 ? 1024.f : 0.f)), peak);
    }
    EXPECT_GT(peak, 0.f);
    EXPECT_TRUE(std::isfinite(peak));
}

TEST(SvfResoBPTest, pumpScalesStateAndResetClears)
{
    SvfResoBP sut{sampleRate};
    sut.setByDecay(0, 1000.f, 0.05f);
    sut.reset(0.5f, -0.5f);
    EXPECT_FLOAT_EQ(sut.currentMagnitudeSquared(), 0.5f); // 0.25 + 0.25
    sut.pump(0.5f);                                       // halves each state
    EXPECT_NEAR(sut.currentMagnitudeSquared(), 0.125f, 1e-6f);
    sut.reset();
    EXPECT_FLOAT_EQ(sut.currentMagnitude(), 0.f);
}

TEST(SvfResoBPTest, computeCoefficientsAppliesExplicitQ)
{
    constexpr float freq = 500.f;
    constexpr float Q = 3.f;
    constexpr auto decayConst = 0.1447648273f;
    const float equivalentDecay = Q / (std::numbers::pi_v<float> * freq * decayConst);

    SvfResoBP viaQ{sampleRate};
    viaQ.computeCoefficients(0, freq, Q);

    SvfResoBP viaDecay{sampleRate};
    viaDecay.setByDecay(0, freq, equivalentDecay);

    for (int i = 0; i < 64; ++i)
    {
        const float x = i == 0 ? 1024.f : 0.f;
        EXPECT_FLOAT_EQ(viaQ.step(x), viaDecay.step(x)) << "at sample " << i;
    }
}

TEST(SvfResoBPTest, setDecayTreatsArgumentAsMilliseconds)
{
    constexpr float freq = 300.f;
    constexpr float decayMs = 50.f;

    SvfResoBP sut{sampleRate};
    sut.computeCoefficients(0, freq); // establishes g at freq
    sut.setDecay(0, decayMs);

    float peak = 0.f;
    for (int i = 0; i < 3; ++i)
    {
        peak = std::max(peak, sut.step(1024.f));
    }

    const auto samplesAfterFiveDecays = static_cast<size_t>(sampleRate * decayMs * 0.001f * 5.f);
    float tail = 0.f;
    for (size_t i = 0; i < samplesAfterFiveDecays; ++i)
    {
        tail = sut.step(0.f);
    }
    EXPECT_LT(std::abs(tail), peak * 0.01f);
}

TEST(SvfResoBPTest, pitchBendSurvivesDampSwitch)
{
    constexpr float baseFreq = 440.f;
    constexpr float decayTime = 2.f;
    constexpr size_t stabilizeSamples = 480;
    constexpr size_t measureSamples = 4800;

    SvfResoBP sut{sampleRate};
    sut.damp(false);
    sut.setByDecay(0, baseFreq, decayTime);
    sut.setByDecay(1, baseFreq, decayTime);
    sut.pitchBendCents(1200.f);

    for (size_t i = 0; i < stabilizeSamples; ++i)
    {
        (void) sut.step(i == 0 ? 1024.f : 0.f);
    }
    sut.damp(true); // switch to the set pitchBendCents() previously left unbent

    std::vector<float> signal(measureSamples);
    for (size_t i = 0; i < measureSamples; ++i)
    {
        signal[i] = sut.step(0.f);
    }

    const auto stats = calculateZeroCrossingStatistics(signal.data(), measureSamples, true);
    const float measuredFreq = sampleRate / stats.meanPeriodLen;
    const float expectedFreq = baseFreq * 2.f; // +1200 cents
    const float errorPercent = std::abs(measuredFreq - expectedFreq) / expectedFreq * 100.f;
    EXPECT_LT(errorPercent, 2.f) << "measured=" << measuredFreq << "Hz, expected=" << expectedFreq << "Hz";
}

TEST(SvfResoBPTest, resonanceCompensationClampsAtDomainEdges)
{
    const auto atEdge = ResonanceCompensation::compensate(60.f, 64.f);
    const auto pastEdge = ResonanceCompensation::compensate(60.f, 128.f);
    EXPECT_FLOAT_EQ(atEdge, pastEdge);

    const auto belowEdge = ResonanceCompensation::compensate(60.f, 48.f);
    EXPECT_GT(atEdge, belowEdge);
}

TEST(SvfResoBPTest, setSampleRateMatchesConstructionAtThatRate)
{
    SvfResoBP constructed{44100.f};
    SvfResoBP reconfigured{}; // default rate 48000
    reconfigured.setSampleRate(44100.f);
    constructed.computeCoefficients(0, 1000.f);
    reconfigured.computeCoefficients(0, 1000.f);
    for (int i = 0; i < 64; ++i)
    {
        const float x = i == 0 ? 1024.f : 0.f;
        EXPECT_FLOAT_EQ(constructed.step(x), reconfigured.step(x)) << "at sample " << i;
    }
}

}
