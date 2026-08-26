#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Dynamics/Compressor.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Test
{
namespace
{
constexpr float kSampleRate{48000.f};

float settledOutputRms(Compressor& sut, const float amplitude, const float toneHz, const size_t numSamples)
{
    float phase = 0.f;
    const float phaseInc = 2.f * std::numbers::pi_v<float> * toneHz / kSampleRate;
    double sumSquares = 0.0;
    const size_t settleSamples = numSamples / 2;
    for (size_t i = 0; i < numSamples; ++i)
    {
        const auto out = sut.step(amplitude * std::sin(phase));
        phase += phaseInc;
        if (i >= settleSamples)
        {
            sumSquares += static_cast<double>(out) * out;
        }
    }
    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(numSamples - settleSamples)));
}
}

TEST(CompressorTest, BelowThresholdPassesThroughAtUnityGain)
{
    Compressor sut{kSampleRate};
    sut.setThresholdDb(-6.f);
    sut.setRatio(4.f);
    sut.setAttackMs(1.f);
    sut.setReleaseMs(20.f);

    const float amplitude = 0.05f; // roughly -26 dBFS, well under the threshold
    const auto outRms = settledOutputRms(sut, amplitude, 440.f, 20000);
    const float inRms = amplitude / std::numbers::sqrt2_v<float>;

    EXPECT_NEAR(outRms, inRms, inRms * 0.1f);
}

TEST(CompressorTest, AboveThresholdReducesGainByRoughlyTheConfiguredRatio)
{
    Compressor sut{kSampleRate};
    sut.setThresholdDb(-20.f);
    sut.setRatio(4.f);
    sut.setAttackMs(1.f);
    sut.setReleaseMs(20.f);

    const float quietAmplitude = 0.03f; // ~ -30 dBFS, below threshold: unity gain reference
    const float loudAmplitude = 1.f;    // 0 dBFS, well above threshold
    const auto quietOutRms = settledOutputRms(sut, quietAmplitude, 440.f, 20000);
    const auto loudOutRms = settledOutputRms(sut, loudAmplitude, 440.f, 20000);

    const float inputDbChange = Convert::gainToDb(loudAmplitude) - Convert::gainToDb(quietAmplitude);
    const float outputDbChange = Convert::gainToDb(loudOutRms) - Convert::gainToDb(quietOutRms);

    // 4:1 above threshold should compress most of a ~30 dB input swing down
    // to roughly a quarter of that in the output (some slack for the knee).
    EXPECT_LT(outputDbChange, inputDbChange * 0.6f);
    EXPECT_GT(outputDbChange, inputDbChange * 0.3f);
}

}
