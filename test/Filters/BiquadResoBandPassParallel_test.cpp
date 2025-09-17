
#include "Filters/Biquad.h"
#include "Filters/BiquadResoBP.h"
#include "Filters/BiquadResoBandPassParallel.h"
#include "Numbers/Convert.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <algorithm>

#include <array>
#include <cmath>
#include <numbers>

namespace AbacDsp::Test
{

TEST(BiquadResoBandPassParallelTest, identicalResponse)
{
    constexpr float sampleRate{48000.f};
    constexpr size_t NumElements{4};
    constexpr size_t BlockSize{128};

    BiquadResoBandPassParallel<NumElements, BlockSize> sut{sampleRate};
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


}