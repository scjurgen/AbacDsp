
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Filters/Biquad.h"
#include "Filters/LinkwitzRileySplit.h"

namespace AbacDsp::Test
{

namespace
{

constexpr float sampleRate{48000.0f};

struct Gains
{
    float low{0.0f};
    float high{0.0f};
    float sum{0.0f};
};

/// Peak gains of the bands and of their sum for a settled sine of the given frequency.
Gains measure(LinkwitzRileySplit& sut, const float hz)
{
    sut.reset();
    Gains peak{};
    const int total = static_cast<int>(sampleRate);
    for (int n = 0; n < total; ++n)
    {
        const float x = std::sin(2.0f * std::numbers::pi_v<float> * hz * static_cast<float>(n) / sampleRate);
        const auto [low, high] = sut.step(x);
        if (n > total / 2)
        {
            peak.low = std::max(peak.low, std::abs(low));
            peak.high = std::max(peak.high, std::abs(high));
            peak.sum = std::max(peak.sum, std::abs(low + high));
        }
    }
    return peak;
}

float toDb(const float gain)
{
    return 20.0f * std::log10(gain);
}

} // namespace

TEST(DspLinkwitzRileySplit, bothBandsAreSixDbDownAtTheSplitFrequency)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(1000.0f);
    const Gains g = measure(sut, 1000.0f);
    EXPECT_NEAR(toDb(g.low), -6.02f, 0.1f);
    EXPECT_NEAR(toDb(g.high), -6.02f, 0.1f);
}

TEST(DspLinkwitzRileySplit, bandsPassTheirSideAndRejectTheOther)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(1000.0f);
    const Gains below = measure(sut, 100.0f);
    EXPECT_NEAR(toDb(below.low), 0.0f, 0.1f);
    EXPECT_LT(toDb(below.high), -60.0f);
    const Gains above = measure(sut, 10000.0f);
    EXPECT_LT(toDb(above.low), -60.0f);
    EXPECT_NEAR(toDb(above.high), 0.0f, 0.1f);
}

TEST(DspLinkwitzRileySplit, slopeIsTwentyFourDbPerOctave)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(200.0f);
    const float atTwoOctavesUp = toDb(measure(sut, 800.0f).low);
    const float atThreeOctavesUp = toDb(measure(sut, 1600.0f).low);
    EXPECT_NEAR(atTwoOctavesUp - atThreeOctavesUp, 24.0f, 1.5f);
}

TEST(DspLinkwitzRileySplit, sumHasFlatMagnitude)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(1000.0f);
    for (const float hz : {50.0f, 400.0f, 1000.0f, 2500.0f, 9000.0f})
    {
        EXPECT_NEAR(toDb(measure(sut, hz).sum), 0.0f, 0.05f) << hz;
    }
}

TEST(DspLinkwitzRileySplit, sumIsTheSecondOrderAllpassOfTheInput)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(1000.0f);
    Biquad<BiquadFilterType::AllPass> allpass;
    allpass.computeCoefficients(sampleRate, 1000.0f, 0.70710678f, 0.0f);
    float worst{0.0f};
    for (int n = 0; n < 4096; ++n)
    {
        const float x = std::sin(0.01f * static_cast<float>(n * n % 977));
        const auto [low, high] = sut.step(x);
        worst = std::max(worst, std::abs(low + high - allpass.singleStepAllPass(x)));
    }
    EXPECT_LT(worst, 1e-3f);
}

TEST(DspLinkwitzRileySplit, frequencyIsClamped)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(1.0f);
    EXPECT_FLOAT_EQ(LinkwitzRileySplit::minFrequency, sut.getFrequency());
    sut.setFrequency(1e6f);
    EXPECT_FLOAT_EQ(LinkwitzRileySplit::maxFrequencyRatio * sampleRate, sut.getFrequency());
}

TEST(DspLinkwitzRileySplit, resetClearsTheStates)
{
    LinkwitzRileySplit sut{sampleRate};
    for (int n = 0; n < 100; ++n)
    {
        static_cast<void>(sut.step(1.0f));
    }
    sut.reset();
    LinkwitzRileySplit fresh{sampleRate};
    const auto a = sut.step(0.5f);
    const auto b = fresh.step(0.5f);
    EXPECT_FLOAT_EQ(a.low, b.low);
    EXPECT_FLOAT_EQ(a.high, b.high);
}

TEST(DspLinkwitzRileySplit, sampleRateChangeKeepsTheSplitFrequency)
{
    LinkwitzRileySplit sut{sampleRate};
    sut.setFrequency(2000.0f);
    sut.setSampleRate(44100.0f);
    EXPECT_FLOAT_EQ(2000.0f, sut.getFrequency());
}

} // namespace AbacDsp::Test
