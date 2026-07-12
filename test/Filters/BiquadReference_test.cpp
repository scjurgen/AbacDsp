
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Filters/Biquad.h"
#include "Filters/BiquadReference.h"
#include "Generators/ReferenceWave.h"

namespace AbacDsp::Test
{

constexpr double maxDeltaDb = 0.1;
constexpr double kSampleRate = 48000.0;

struct BiquadReferenceCoefficients
{
    double a1{};
    double a2{};
    double b0{};
    double b1{};
    double b2{};
};

BiquadReferenceCoefficients getCoefficients(const BiquadReference& ref)
{
    BiquadReferenceCoefficients c;
    ref.getCoefficients(c.a1, c.a2, c.b0, c.b1, c.b2);
    return c;
}

float measureMagnitudeDb(const BiquadReferenceCoefficients& c, const double frequency, const bool needsSettling)
{
    Biquad<BiquadFilterType::FreeCoefficients> sut;
    sut.setCoefficients(static_cast<float>(c.b0), static_cast<float>(c.b1), static_cast<float>(c.b2),
                        static_cast<float>(c.a1), static_cast<float>(c.a2));

    std::vector<float> wave(needsSettling ? 22000 : 5000, 0.0f);
    renderReferenceSineWave(wave, kSampleRate, frequency);
    sut.processBlock(wave.data(), wave.data(), wave.size());

    const auto [minV, maxV] = std::minmax_element(wave.begin() + wave.size() / 2, wave.end());
    const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
    return std::log10(maxValue) * 20.0f;
}

struct FilterTypeCase
{
    BiquadFilterType type;
    double gain;
};

std::ostream& operator<<(std::ostream& os, const FilterTypeCase& tc)
{
    return os << "type=" << static_cast<int>(tc.type) << " gain=" << tc.gain;
}

static std::array<FilterTypeCase, 8> kAllDispatchedTypes{{
    {BiquadFilterType::LowPass, 0.0},
    {BiquadFilterType::HighPass, 0.0},
    {BiquadFilterType::BandPass, 0.0},
    {BiquadFilterType::Notch, 0.0},
    {BiquadFilterType::Peak, 6.0},
    {BiquadFilterType::Peak, -6.0},
    {BiquadFilterType::LoShelf, 6.0},
    {BiquadFilterType::HiShelf, 6.0},
}};

class BiquadReferenceDispatchTest : public testing::TestWithParam<FilterTypeCase>
{
};

TEST_P(BiquadReferenceDispatchTest, calculateCoefficientsDispatchesToDirectMethod)
{
    const auto tc = GetParam();
    constexpr double cf = 1000.0;
    constexpr double q = 0.70710678;

    BiquadReference viaSwitch{kSampleRate, tc.type};
    viaSwitch.calculateCoefficients(cf, q, tc.gain);
    const auto switched = getCoefficients(viaSwitch);

    BiquadReference direct{kSampleRate, tc.type};
    switch (tc.type)
    {
        case BiquadFilterType::LowPass:
            direct.lowpass(cf, q);
            break;
        case BiquadFilterType::HighPass:
            direct.highpass(cf, q);
            break;
        case BiquadFilterType::BandPass:
            direct.bandpass(cf, q);
            break;
        case BiquadFilterType::Notch:
            direct.notch(cf, q);
            break;
        case BiquadFilterType::Peak:
            direct.peak(cf, q, tc.gain);
            break;
        case BiquadFilterType::LoShelf:
            direct.loshelf(cf, q, tc.gain);
            break;
        case BiquadFilterType::HiShelf:
            direct.hishelf(cf, q, tc.gain);
            break;
        case BiquadFilterType::AllPass:
            direct.allpass(cf, q);
            break;
        default:
            FAIL() << "unexpected type in test data";
    }
    const auto expected = getCoefficients(direct);

    EXPECT_DOUBLE_EQ(switched.a1, expected.a1);
    EXPECT_DOUBLE_EQ(switched.a2, expected.a2);
    EXPECT_DOUBLE_EQ(switched.b0, expected.b0);
    EXPECT_DOUBLE_EQ(switched.b1, expected.b1);
    EXPECT_DOUBLE_EQ(switched.b2, expected.b2);
}

TEST_P(BiquadReferenceDispatchTest, magnitudeMatchesRenderedSignal)
{
    const auto tc = GetParam();
    std::vector<double> qValues{0.70710678, 10.0, 0.2};
    std::vector<double> frequencies{25.0, 200.0, 800.0, 6400.0};

    for (const auto q : qValues)
    {
        for (const auto hz : frequencies)
        {
            BiquadReference ref{kSampleRate, tc.type};
            ref.calculateCoefficients(1000.0, q, tc.gain);
            const auto coeffs = getCoefficients(ref);

            const auto measuredDb = measureMagnitudeDb(coeffs, hz, q > 0.8);
            const auto expectedDb = ref.magnitude(hz);
            EXPECT_NEAR(measuredDb, expectedDb, maxDeltaDb) << tc << " q:" << q << " hz:" << hz;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(AllTypes, BiquadReferenceDispatchTest, testing::ValuesIn(kAllDispatchedTypes));

TEST(BiquadReferenceTest, allpassDispatch)
{
    constexpr double cf = 1000.0;
    constexpr double q = 0.70710678;

    BiquadReference viaSwitch{kSampleRate, BiquadFilterType::AllPass};
    viaSwitch.calculateCoefficients(cf, q, 0.0);
    const auto switched = getCoefficients(viaSwitch);

    BiquadReference direct{kSampleRate, BiquadFilterType::AllPass};
    direct.allpass(cf, q);
    const auto expected = getCoefficients(direct);

    EXPECT_DOUBLE_EQ(switched.a1, expected.a1);
    EXPECT_DOUBLE_EQ(switched.a2, expected.a2);
    EXPECT_DOUBLE_EQ(switched.b0, expected.b0);
    EXPECT_DOUBLE_EQ(switched.b1, expected.b1);
    EXPECT_DOUBLE_EQ(switched.b2, expected.b2);
}

TEST(BiquadReferenceTest, allpassHasUnityMagnitudeEverywhere)
{
    BiquadReference ref{kSampleRate, BiquadFilterType::AllPass};
    ref.calculateCoefficients(1000.0, 0.70710678, 0.0);

    for (const double hz : {25.0, 200.0, 800.0, 6400.0, 15000.0})
    {
        EXPECT_NEAR(ref.magnitude(hz), 0.0, 1e-6) << "allpass must have flat magnitude at " << hz;
    }
}

TEST(BiquadReferenceTest, calculateCoefficientsThrowsOnUnsupportedType)
{
    BiquadReference sutOnePole{kSampleRate, BiquadFilterType::OnePole};
    EXPECT_THROW(sutOnePole.calculateCoefficients(1000.0, 0.7, 0.0), std::invalid_argument);

    BiquadReference sutFree{kSampleRate, BiquadFilterType::FreeCoefficients};
    EXPECT_THROW(sutFree.calculateCoefficients(1000.0, 0.7, 0.0), std::invalid_argument);
}

TEST(BiquadReferenceTest, signalGainSetsOnlyB0)
{
    BiquadReference sut{kSampleRate, BiquadFilterType::LowPass};
    sut.signalGain(0.5);

    const auto c = getCoefficients(sut);
    EXPECT_DOUBLE_EQ(c.b0, 0.5);
    EXPECT_DOUBLE_EQ(c.b1, 0.0);
    EXPECT_DOUBLE_EQ(c.b2, 0.0);
    EXPECT_DOUBLE_EQ(c.a1, 0.0);
    EXPECT_DOUBLE_EQ(c.a2, 0.0);
}

TEST(BiquadReferenceTest, signalGainMagnitudeIsFlatAndMatchesGainInDb)
{
    BiquadReference sut{kSampleRate, BiquadFilterType::LowPass};
    sut.signalGain(2.0);

    const auto expectedDb = 20.0 * std::log10(2.0);
    for (const double hz : {25.0, 1000.0, 10000.0})
    {
        EXPECT_NEAR(sut.magnitude(hz), expectedDb, 1e-9);
    }
}

TEST(BiquadReferenceTest, peakPositiveAndNegativeGainAreInverses)
{
    // Symmetric construction: boosting and cutting by the same amount at the same
    // frequency should produce (approximately) opposite magnitude deviations.
    BiquadReference boost{kSampleRate, BiquadFilterType::Peak};
    boost.calculateCoefficients(1000.0, 0.70710678, 6.0);

    BiquadReference cut{kSampleRate, BiquadFilterType::Peak};
    cut.calculateCoefficients(1000.0, 0.70710678, -6.0);

    EXPECT_NEAR(boost.magnitude(1000.0), 6.0, 0.05);
    EXPECT_NEAR(cut.magnitude(1000.0), -6.0, 0.05);
}

TEST(BiquadReferenceTest, lowShelfAndHighShelfApproachExpectedGainAtExtremes)
{
    BiquadReference lowShelf{kSampleRate, BiquadFilterType::LoShelf};
    lowShelf.calculateCoefficients(1000.0, 0.70710678, 12.0);
    EXPECT_NEAR(lowShelf.magnitude(1.0), 12.0, 0.5);
    EXPECT_NEAR(lowShelf.magnitude(kSampleRate / 2.0 - 1.0), 0.0, 1.0);

    BiquadReference highShelf{kSampleRate, BiquadFilterType::HiShelf};
    highShelf.calculateCoefficients(1000.0, 0.70710678, 12.0);
    EXPECT_NEAR(highShelf.magnitude(1.0), 0.0, 1.0);
    EXPECT_NEAR(highShelf.magnitude(kSampleRate / 2.0 - 1.0), 12.0, 0.5);
}

TEST(BiquadReferenceTest, notchAttenuatesAtCenterFrequency)
{
    BiquadReference sut{kSampleRate, BiquadFilterType::Notch};
    sut.calculateCoefficients(1000.0, 10.0, 0.0);
    EXPECT_LT(sut.magnitude(1000.0), -20.0);
    EXPECT_NEAR(sut.magnitude(1.0), 0.0, 0.5);
}

TEST(BiquadReferenceTest, bandpassPeaksAtCenterFrequency)
{
    BiquadReference sut{kSampleRate, BiquadFilterType::BandPass};
    sut.calculateCoefficients(1000.0, 10.0, 0.0);
    EXPECT_NEAR(sut.magnitude(1000.0), 0.0, 0.5);
    EXPECT_LT(sut.magnitude(25.0), -20.0);
    EXPECT_LT(sut.magnitude(15000.0), -20.0);
}

}
