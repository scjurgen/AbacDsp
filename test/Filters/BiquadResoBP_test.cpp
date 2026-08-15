
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <tuple>

#include "gtest/gtest.h"

#include "Filters/Biquad.h"
#include "Filters/BiquadResoBP.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Test
{

TEST(BiquadResoBPTest, responseDecay)
{
    constexpr float sampleRate{48000.f};
    for (size_t n = 21; n < 100; ++n)
    {
        const float f = Convert::noteToFrequency(static_cast<float>(n));
        BiquadResoBP sut{sampleRate};
        sut.setByDecay(0, f, 1.f);
        float maxInit = 0.f;
        float periodLength = sampleRate / f;
        size_t localCount = 0;
        float localMax = 0.f;
        float lastMax = 0.f;
        for (size_t i = 0; i < 48000; ++i)
        {
            if (localCount == 0)
            {
                lastMax = localMax;
                localMax = 0.f;
                localCount = static_cast<size_t>(2 * periodLength);
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
        EXPECT_LT(std::log10(lastMax) * 20, -50.f) << "failed at note " << n;
    }
}

TEST(BiquadResoBPTest, quickReleaseDamping)
{
    constexpr float sampleRate{48000.f};
    constexpr float f = 200.f;
    BiquadResoBP sut{sampleRate};
    sut.damp(false);
    sut.setByDecay(0, f, 1.f);
    sut.setByDecay(1, f, 0.01f);
    for (size_t i = 0; i < 4800; ++i)
    {
        std::ignore = sut.step(i < 7 ? 1024.f : 0.f);
    }
    float preDecayMax = sut.step(0.f);
    for (size_t i = 0; i < 480; ++i)
    {
        preDecayMax = std::max(std::abs(sut.step(0.f)), preDecayMax);
    }
    sut.damp(true);
    for (size_t i = 0; i < 700; ++i)
    {
        std::ignore = sut.step(0.f);
    }
    float currentMax = sut.step(0.f);
    for (size_t i = 0; i < 400; ++i)
    {
        currentMax = std::max(std::abs(sut.step(0.f)), currentMax);
    }
    EXPECT_GT(preDecayMax, 1.0f);
    EXPECT_LT(currentMax, 1E-4f);
}

TEST(BiquadResoBPTest, triggeredForcesActiveWindowThenGoesInactive)
{
    constexpr float sampleRate{48000.f};
    BiquadResoBP sut{sampleRate};
    sut.setByDecay(0, 1000.f, 0.001f); // decayMax = sampleRate * 0.001 = 48 samples
    sut.reset();
    sut.triggered();

    EXPECT_TRUE(sut.isActive()); // forced-active branch (m_decayCount > 0)

    size_t activeCalls = 1;
    while (sut.isActive())
    {
        ++activeCalls;
        ASSERT_LT(activeCalls, 1000u); // must terminate well before this
    }
    EXPECT_FALSE(sut.isActive());
    EXPECT_GE(activeCalls, 48u);
}

TEST(BiquadResoBPTest, setDecayTreatsArgumentAsMilliseconds)
{
    constexpr float sampleRate{48000.f};
    constexpr float freq = 300.f;
    constexpr float decayMs = 50.f;

    BiquadResoBP sut{sampleRate};
    sut.computeCoefficients(0, freq); // establishes K/kSquare at freq
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

}
