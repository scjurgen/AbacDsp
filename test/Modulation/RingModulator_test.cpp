#include "gtest/gtest.h"

#include "Analysis/FftMisc.h"
#include "Generators/ReferenceWave.h"
#include "Modulation/RingModulator.h"

namespace AbacDsp::Test
{

TEST(RingModulatorTest, ProducesSumAndDifferenceSidebandsNotTheOriginalTone)
{
    constexpr float sampleRate{48000};
    constexpr float signalHz{1000.f};
    constexpr float carrierHz{300.f};

    std::vector<float> buffer(20000);
    renderReferenceSineWave(buffer, sampleRate, signalHz);

    RingModulator sut{sampleRate};
    sut.setFrequency(carrierHz);
    std::transform(buffer.begin(), buffer.end(), buffer.begin(), [&sut](const float in) { return sut.step(in); });

    constexpr size_t windowSize{4096};
    const size_t windowStart{buffer.size() - windowSize};
    std::vector<float> analyse(buffer.begin() + windowStart, buffer.begin() + windowStart + windowSize);
    std::vector<float> fftResult;
    BasicFFT::realDataToMagnitude(analyse, fftResult);

    const auto bin = [&](const float hz)
    { return static_cast<size_t>(std::round(hz / sampleRate * static_cast<float>(windowSize))); };

    const auto sumBin = bin(signalHz + carrierHz);
    const auto diffBin = bin(signalHz - carrierHz);
    const auto originalBin = bin(signalHz);

    EXPECT_GT(std::log10(fftResult[sumBin]) * 20, -20.f);
    EXPECT_GT(std::log10(fftResult[diffBin]) * 20, -20.f);
    // The original tone itself must not survive at anywhere near sideband level -
    // that's what distinguishes ring modulation from plain amplitude modulation.
    EXPECT_LT(fftResult[originalBin], fftResult[sumBin] * 0.1f);
    EXPECT_LT(fftResult[originalBin], fftResult[diffBin] * 0.1f);
}

TEST(RingModulatorTest, SilentCarrierPhaseZeroesTheOutput)
{
    constexpr float sampleRate{48000};
    RingModulator sut{sampleRate};
    sut.setFrequency(0.f); // sin(phase) starts at sin(0) = 0 and stays there
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_FLOAT_EQ(sut.step(1.f), 0.f);
    }
}

}
