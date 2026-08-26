#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "Modulation/Tremolo.h"

namespace AbacDsp::Test
{
namespace
{
// Feeding a constant DC input directly reveals the modulator's own gain envelope.
std::vector<float> renderEnvelope(Tremolo& sut, const size_t numSamples)
{
    std::vector<float> result(numSamples);
    for (auto& v : result)
    {
        v = sut.step(1.f);
    }
    return result;
}

size_t fractionInMidBand(const std::vector<float>& envelope)
{
    return static_cast<size_t>(
        std::count_if(envelope.begin(), envelope.end(), [](const float v) { return v > 0.2f && v < 0.8f; }));
}
}

TEST(TremoloTest, PureSineAmplitudeModulationAtZeroDrive)
{
    constexpr float sampleRate{48000.f};
    Tremolo sut{sampleRate};
    sut.setRate(100.f);
    sut.setDepth(1.f);
    sut.setDrive(0.f);

    const auto cycleSamples = static_cast<size_t>(sampleRate / 100.f);
    const auto envelope = renderEnvelope(sut, cycleSamples * 4);

    const auto maxValue = *std::max_element(envelope.begin(), envelope.end());
    const auto minValue = *std::min_element(envelope.begin(), envelope.end());
    EXPECT_NEAR(maxValue, 1.f, 0.01f);
    EXPECT_NEAR(minValue, 0.f, 0.01f);

    // A sine spends a large share of each cycle away from its extremes.
    EXPECT_GT(fractionInMidBand(envelope), envelope.size() / 4);
}

TEST(TremoloTest, MaxDriveSquaresOffTheEnvelopeAndFullyGates)
{
    constexpr float sampleRate{48000.f};
    Tremolo sut{sampleRate};
    sut.setRate(100.f);
    sut.setDepth(1.f);
    sut.setDrive(1.f);

    const auto cycleSamples = static_cast<size_t>(sampleRate / 100.f);
    const auto envelope = renderEnvelope(sut, cycleSamples * 4);

    const auto maxValue = *std::max_element(envelope.begin(), envelope.end());
    const auto minValue = *std::min_element(envelope.begin(), envelope.end());
    EXPECT_NEAR(maxValue, 1.f, 0.05f);
    EXPECT_NEAR(minValue, 0.f, 0.05f);

    // Near-square: the envelope spends only a small fraction of each cycle transitioning.
    EXPECT_LT(fractionInMidBand(envelope), envelope.size() / 10);
}

}
