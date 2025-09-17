
#include "Filters/Biquad.h"
#include "Filters/BiquadResoBP.h"
#include "Numbers/Convert.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <algorithm>

#include <array>
#include <cmath>
#include <numbers>

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

}