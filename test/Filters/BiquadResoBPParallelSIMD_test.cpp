
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "Filters/Biquad.h"
#include "Filters/BiquadResoBP.h"
#include "Filters/BiquadResoBPParallelSIMD.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Test
{

TEST(BiquadResoBPParallelSIMDTest, identicalResponse4)
{
    constexpr size_t BlockSize{128};
    constexpr float sampleRate{48000.f};
    constexpr size_t NumElements{4};

    BiquadResoBPParallelSIMD<NumElements, BlockSize> sut{sampleRate};
    std::array<BiquadResoBP, NumElements> reference{BiquadResoBP(sampleRate), BiquadResoBP(sampleRate),
                                                    BiquadResoBP(sampleRate), BiquadResoBP(sampleRate)};
    constexpr std::array frequencies{20.f, 100.f, 1000.f, 2000.f};
    constexpr std::array qFactors{20.f, 10.f, 2.f, 0.707f};
    for (size_t i = 0; i < NumElements; ++i)
    {
        sut.computeCoefficients(i, 0, frequencies[i], qFactors[i]);
        sut.computeCoefficients(i, 1, frequencies[i], qFactors[i]);
        reference[i].computeCoefficients(0, frequencies[i], qFactors[i]);
        reference[i].computeCoefficients(1, frequencies[i], qFactors[i]);
    }
    std::array<float, BlockSize> inValues{1.f};
    std::array<float, BlockSize> refResult{};
    std::array<float, BlockSize> result{};

    sut.process(inValues.data(), result.data());

    for (size_t i = 0; i < BlockSize; ++i)
    {
        refResult[i] = reference[0].step(inValues[i]);
        refResult[i] += reference[1].step(inValues[i]);
        refResult[i] += reference[2].step(inValues[i]);
        refResult[i] += reference[3].step(inValues[i]);
    }
    for (size_t i = 0; i < BlockSize; ++i)
    {
        EXPECT_NEAR(refResult[i], result[i], 1E-6f) << "failed at index " << i;
    }
}

TEST(BiquadResoBPParallelSIMDTest, isActiveTracksRealProcessedState)
{
    constexpr size_t BlockSize{16};
    constexpr float sampleRate{48000.f};
    constexpr size_t NumElements{4};

    BiquadResoBPParallelSIMD<NumElements, BlockSize> sut{sampleRate};
    sut.setByDecay(0, 0, 1000.f, 0.01f); // 10 ms decay, element 0 only
    sut.reset(0, 1.f, 0.f);

    const std::array<float, BlockSize> silence{};
    size_t blocksRun = 0;
    while (sut.isActive(0))
    {
        std::array<float, BlockSize> out{};
        sut.process(silence.data(), out.data());
        ++blocksRun;
        ASSERT_LT(blocksRun, 1000u); // must go inactive once the real state has decayed
    }
    EXPECT_GT(blocksRun, 0u);
}

}
