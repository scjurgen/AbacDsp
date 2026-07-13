#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Generators/ResoParallelSIMD.h"

namespace AbacDsp::Test
{

TEST(ResoBpParallelSIMD, processTriggeredBandsProducesFiniteOutput)
{
    constexpr size_t numElements{8};
    constexpr size_t blockSize{16};
    ResoBpParallelSIMD<numElements, blockSize> reso(48000.f);

    for (size_t i = 0; i < numElements; ++i)
    {
        reso.setByDecay(i, 0, 220.f * static_cast<float>(i + 1), 0.05f);
        reso.reset(i, 1.f, 0.f);
    }

    std::array<float, blockSize> out{};
    bool sawNonZero = false;
    for (int block = 0; block < 20; ++block)
    {
        reso.process(out);
        for (const float v : out)
        {
            ASSERT_TRUE(std::isfinite(v));
            sawNonZero = sawNonZero || v != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
}

TEST(ResoBpParallelSIMD, isActiveBecomesFalseAfterSilence)
{
    constexpr size_t numElements{4};
    constexpr size_t blockSize{8};
    ResoBpParallelSIMD<numElements, blockSize> reso;
    reso.setByDecay(0, 0, 440.f, 0.01f);
    reso.reset(0, 0.f, 0.f);

    bool eventuallyInactive = false;
    for (int i = 0; i < 200; ++i)
    {
        if (!reso.isActive(0))
        {
            eventuallyInactive = true;
            break;
        }
    }
    EXPECT_TRUE(eventuallyInactive);
}

TEST(ResoBpParallelSIMD, magnitudeIsFiniteAcrossSpectrum)
{
    constexpr size_t numElements{4};
    constexpr size_t blockSize{8};
    ResoBpParallelSIMD<numElements, blockSize> reso(48000.f);
    reso.setByDecay(0, 0, 1000.f, 0.1f);

    for (float hz = 100.f; hz < 10000.f; hz += 500.f)
    {
        EXPECT_TRUE(std::isfinite(reso.magnitude(0, 0, hz)));
    }
}

}
