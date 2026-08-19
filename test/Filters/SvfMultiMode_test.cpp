#include <cmath>
#include <numbers>
#include <tuple>

#include "gtest/gtest.h"

#include "Filters/SvfMultiMode.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float sampleRate{48000.f};
constexpr float cutoff{1000.f};
constexpr float testQ{2.f};

enum class Tap
{
    Low,
    High,
    Band,
    Notch
};

[[nodiscard]] float tapValue(const SvfMultiMode::Outputs& out, const Tap tap) noexcept
{
    switch (tap)
    {
        case Tap::Low:
            return out.low;
        case Tap::High:
            return out.high;
        case Tap::Band:
            return out.band;
        case Tap::Notch:
            return out.notch;
    }
    return 0.f;
}

// Feeds a unit-amplitude sine, lets the resonance settle, then measures the steady-state
// peak of the requested tap - a plain gain-at-frequency probe, same spirit as
// SvfResoBP_test.cpp's peak-picking.
[[nodiscard]] float measureGain(const float frequency, const Tap tap) noexcept
{
    SvfMultiMode filter{sampleRate};
    filter.computeCoefficients(cutoff, testQ);

    constexpr size_t settleSamples = 4000;
    constexpr size_t measureSamples = 4000;
    float peak = 0.f;
    for (size_t i = 0; i < settleSamples + measureSamples; ++i)
    {
        const float in = std::sin(2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(i) / sampleRate);
        const auto out = filter.step(in);
        if (i >= settleSamples)
        {
            peak = std::max(peak, std::abs(tapValue(out, tap)));
        }
    }
    return peak;
}
}

TEST(SvfMultiModeTest, LowPassPassesLowFrequency)
{
    EXPECT_NEAR(measureGain(50.f, Tap::Low), 1.f, 0.05f);
}

TEST(SvfMultiModeTest, LowPassAttenuatesHighFrequency)
{
    EXPECT_LT(measureGain(15000.f, Tap::Low), 0.1f);
}

TEST(SvfMultiModeTest, HighPassAttenuatesLowFrequency)
{
    EXPECT_LT(measureGain(50.f, Tap::High), 0.1f);
}

TEST(SvfMultiModeTest, HighPassPassesHighFrequency)
{
    EXPECT_NEAR(measureGain(15000.f, Tap::High), 1.f, 0.05f);
}

TEST(SvfMultiModeTest, BandPassPeaksAtCutoff)
{
    EXPECT_NEAR(measureGain(cutoff, Tap::Band), testQ, 0.1f);
}

TEST(SvfMultiModeTest, BandPassAttenuatesFarFromCutoff)
{
    EXPECT_LT(measureGain(50.f, Tap::Band), 0.2f);
    EXPECT_LT(measureGain(15000.f, Tap::Band), 0.2f);
}

TEST(SvfMultiModeTest, NotchRemovesCutoff)
{
    EXPECT_LT(measureGain(cutoff, Tap::Notch), 0.05f);
}

TEST(SvfMultiModeTest, NotchPassesFarFromCutoff)
{
    EXPECT_NEAR(measureGain(50.f, Tap::Notch), 1.f, 0.05f);
    EXPECT_NEAR(measureGain(15000.f, Tap::Notch), 1.f, 0.05f);
}

TEST(SvfMultiModeTest, ResetClearsState)
{
    SvfMultiMode filter{sampleRate};
    filter.computeCoefficients(cutoff, testQ);
    for (size_t i = 0; i < 100; ++i)
    {
        std::ignore = filter.step(1.f);
    }
    filter.reset();
    const auto out = filter.step(0.f);
    EXPECT_FLOAT_EQ(out.low, 0.f);
    EXPECT_FLOAT_EQ(out.high, 0.f);
    EXPECT_FLOAT_EQ(out.band, 0.f);
    EXPECT_FLOAT_EQ(out.notch, 0.f);
}

}
