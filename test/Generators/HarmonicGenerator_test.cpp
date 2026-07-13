#include <array>
#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Generators/HarmonicGenerator.h"

namespace AbacDsp::Test
{

TEST(HarmonicGenerator, addHarmonicsFillsPlausibleFrequencies)
{
    HarmonicGenerator<16> generator(HarmonicFormulas::odd());
    generator.setSkew(0.f);
    generator.setStrength(0.5f);

    std::array<Harmonic, 16> harmonics{};
    const size_t count = generator.addHarmonics(harmonics.data(), 0, harmonics.size(), 220.f, 1.f);

    ASSERT_GT(count, 0u);
    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_TRUE(std::isfinite(harmonics[i].f));
        EXPECT_GT(harmonics[i].f, 0.f);
        EXPECT_TRUE(std::isfinite(harmonics[i].gain));
    }
}

TEST(HarmonicGenerator, randomnessSettingsDoNotProduceNaN)
{
    HarmonicGenerator<8> generator(HarmonicFormulas::stretched());
    generator.setRandPower(0.5f);
    generator.setRandSpread(0.5f);

    std::array<Harmonic, 8> harmonics{};
    const size_t count = generator.addHarmonics(harmonics.data(), 0, harmonics.size(), 110.f, 0.8f);
    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_TRUE(std::isfinite(harmonics[i].f));
        EXPECT_TRUE(std::isfinite(harmonics[i].gain));
    }
}

TEST(HarmonicFormulas, allFormulasProduceFiniteRatiosForFirstOvertones)
{
    const std::vector<HarmonicGenerator<1>::HarmonicFormula> formulas{
        HarmonicFormulas::odd(),
        HarmonicFormulas::even(),
        HarmonicFormulas::stretched(),
        HarmonicFormulas::subharmonic(),
        HarmonicFormulas::subharmonic_range(5),
        HarmonicFormulas::just_major_triad(),
        HarmonicFormulas::just_minor_triad(),
        HarmonicFormulas::pythagorean(),
        HarmonicFormulas::bell_partials(),
        HarmonicFormulas::cymbal_partials(),
        HarmonicFormulas::plate_partials(),
        HarmonicFormulas::vowel_formants(),
        HarmonicFormulas::formant_scaled(1.f),
        HarmonicFormulas::golden_ratio(),
        HarmonicFormulas::fibonacci(),
        HarmonicFormulas::prime_harmonics(),
    };

    for (const auto& formula : formulas)
    {
        for (int overtoneNum = 0; overtoneNum < 4; ++overtoneNum)
        {
            const float ratio = formula(overtoneNum, 0.5f);
            EXPECT_TRUE(std::isfinite(ratio));
        }
    }
}

}
