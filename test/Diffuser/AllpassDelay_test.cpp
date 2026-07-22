#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "Diffuser/AllpassDelay.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};

// Encoding the sample index directly as the input value lets us recover the actual read/write
// separation at every sample from the output alone: with feedback and the write-path lowpass
// disabled, step() is a pure delay pass-through, so output[i] == input[i - effectiveDelay] and
// effectiveDelay = i - output[i]. This exposes exactly what a modulation-depth bug looks like -
// the effective delay collapsing to (or near) zero, going negative, or jumping to a wildly
// different value via ring-buffer wraparound - none of which a simple isfinite()/boundedness
// check on a musical test signal would necessarily catch (the collided read position is still
// reading real, bounded samples, just from the wrong place in time).
[[nodiscard]] std::pair<float, float> effectiveDelayRange(ModulatingAllPassDelay<24000>& delay,
                                                          const size_t settleSamples, const size_t numSamples)
{
    delay.setFeedback(0.0f);
    delay.setLowpass(21000.f); // 21000 > maxFilterFrequency (20001) => filter disabled

    std::vector<float> input(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        input[i] = static_cast<float>(i);
    }
    std::vector<float> output(numSamples, 0.0f);
    delay.processBlock(input.data(), output.data(), numSamples);

    float minDelay = std::numeric_limits<float>::max();
    float maxDelay = std::numeric_limits<float>::lowest();
    for (size_t i = settleSamples; i < numSamples; ++i)
    {
        const float effectiveDelay = static_cast<float>(i) - output[i];
        minDelay = std::min(minDelay, effectiveDelay);
        maxDelay = std::max(maxDelay, effectiveDelay);
    }
    return {minDelay, maxDelay};
}
}

// Reported bug: a delay well under ~0.8m (~115 samples @48kHz) with modulation depth at the UI
// maximum (1.0) let the modulated read head reach (or pass) the write head. 72 samples is
// roughly 0.5m at this sample rate - in the middle of the previously-broken range.
TEST(ModulatingAllPassDelayTest, ShortDelayWithMaxModulationDepthNeverReachesWriteHead)
{
    constexpr size_t width{72};
    ModulatingAllPassDelay<24000> delay(kSampleRate);
    delay.setSize(width, skipSmoothing);
    delay.setModulationSpeed(2.0f);
    delay.setModulationDepth(1.0f);

    const auto [minDelay, maxDelay] = effectiveDelayRange(delay, width * 2, 48000);
    EXPECT_GT(minDelay, 0.0f) << "read head reached or passed the write head";
    EXPECT_LE(maxDelay, static_cast<float>(width) + 4.0f);
}

TEST(ModulatingAllPassDelayTest, MinimumDelaySizeWithMaxModulationDepthNeverReachesWriteHead)
{
    constexpr size_t width{ModulatingAllPassDelay<24000>::minDelaySize};
    ModulatingAllPassDelay<24000> delay(kSampleRate);
    delay.setSize(1, skipSmoothing); // clamped internally up to minDelaySize
    delay.setModulationSpeed(2.0f);
    delay.setModulationDepth(1.0f);

    const auto [minDelay, maxDelay] = effectiveDelayRange(delay, width * 2, 48000);
    EXPECT_GT(minDelay, 0.0f) << "read head reached or passed the write head";
    EXPECT_LE(maxDelay, static_cast<float>(width) + 4.0f);
}

// A delay long enough that the collision-safety clamp should never bind must still show close
// to the full effect of a high modulation depth, not be silently neutered by an
// over-conservative margin (regression check for the same fix that protects the short-delay
// case above).
TEST(ModulatingAllPassDelayTest, LongDelayModulationDepthIsNotOverClamped)
{
    constexpr size_t width{4000};
    ModulatingAllPassDelay<24000> delay(kSampleRate);
    delay.setSize(width, skipSmoothing);
    delay.setModulationSpeed(2.0f);
    delay.setModulationDepth(1.0f);

    const auto [minDelay, maxDelay] = effectiveDelayRange(delay, width * 2, 48000);
    EXPECT_GT(minDelay, 0.0f);
    EXPECT_LE(maxDelay, static_cast<float>(width) + 4.0f);
    EXPECT_GT(maxDelay - minDelay, 400.0f) << "modulation swing on a long delay should be close to "
                                              "the full requested depth (~500 samples at UI max)";
}

TEST(ModulatingAllPassDelayTest, ZeroModulationDepthStaysFinite)
{
    ModulatingAllPassDelay<24000> delay(kSampleRate);
    delay.setSize(500, skipSmoothing);
    delay.setFeedback(0.3f);
    delay.setModulationDepth(0.0f);

    std::vector<float> input(kSampleRate / 4);
    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::sin(static_cast<float>(i) * 0.05f);
    }
    std::vector<float> output(input.size());
    delay.processBlock(input.data(), output.data(), output.size());
    for (const auto v : output)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}

// Shrinking a delay while a high modulation depth was already set for its previous (longer)
// width must re-trim the depth for the new, shorter width - not leave a now-unsafe depth in
// place until the next setModulationDepth() call.
TEST(ModulatingAllPassDelayTest, ShrinkingSizeAfterSettingDepthNeverReachesWriteHead)
{
    constexpr size_t shortWidth{72};
    ModulatingAllPassDelay<24000> delay(kSampleRate);
    delay.setSize(4000, skipSmoothing);
    delay.setModulationSpeed(2.0f);
    delay.setModulationDepth(1.0f); // safe for width 4000, must be re-trimmed once width shrinks
    delay.setSize(shortWidth, skipSmoothing);

    const auto [minDelay, maxDelay] = effectiveDelayRange(delay, shortWidth * 2, 48000);
    EXPECT_GT(minDelay, 0.0f) << "read head reached or passed the write head after shrinking";
    EXPECT_LE(maxDelay, static_cast<float>(shortWidth) + 4.0f);
}

}
