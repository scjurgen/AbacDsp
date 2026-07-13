#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Generators/ResoGenerator.h"

namespace AbacDsp::Test
{

TEST(ResoGenerator, triggeredHarmonicsProduceFiniteOutputAndDecayToInactive)
{
    constexpr size_t blockSize{32};
    constexpr size_t numElements{4};
    ResoGenerator<blockSize, numElements> reso(48000.f);

    std::array<Harmonic, numElements> harmonics{
        Harmonic{220.f, 1.f, 0.01f, 0},
        Harmonic{440.f, 0.5f, 0.01f, 0},
        Harmonic{660.f, 0.25f, 0.01f, 0},
        Harmonic{880.f, 0.125f, 0.01f, 0},
    };
    reso.runHarmonicList(harmonics.data(), harmonics.size());

    EXPECT_TRUE(reso.isActive());

    std::array<float, blockSize> block{};
    bool sawNonZero = false;
    for (int i = 0; i < 2000 && reso.isActive(); ++i)
    {
        reso.processBlock(block);
        for (const float v : block)
        {
            ASSERT_TRUE(std::isfinite(v));
            sawNonZero = sawNonZero || v != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
    EXPECT_FALSE(reso.isActive());
}

TEST(ResoGenerator, softExcitationStaysFinite)
{
    constexpr size_t blockSize{16};
    constexpr size_t numElements{2};
    ResoGenerator<blockSize, numElements> reso(44100.f);
    reso.setSoftExcitation(0.3f);
    reso.setAttack(2.f);

    std::array<Harmonic, numElements> harmonics{
        Harmonic{330.f, 1.f, 8.f, 0},
        Harmonic{550.f, 0.6f, 8.f, 1},
    };
    reso.runHarmonicList(harmonics.data(), harmonics.size());

    std::array<float, blockSize> block{};
    for (int i = 0; i < 50; ++i)
    {
        reso.processBlock(block);
        for (const float v : block)
        {
            EXPECT_TRUE(std::isfinite(v));
        }
    }
}

}
