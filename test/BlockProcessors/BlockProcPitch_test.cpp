#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "NaiveGenerators/Generator.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};
constexpr float kBaseFrequency{220.f};
constexpr float kPitchTolerance{5.0f};

[[nodiscard]] std::vector<float> generateSine(const float frequency, const size_t numSamples)
{
    std::vector<float> signal(numSamples);
    Generator<Wave::Sine> generator{kSampleRate, frequency};
    generator.render(signal.begin(), signal.end());
    return signal;
}

[[nodiscard]] std::vector<float> processThroughPitch(const std::vector<float>& input, const float semitones,
                                                     const float mix, const bool reverse = false)
{
    BlockProc::Pitch<kBlockSize> sut{kSampleRate};
    sut.setPitch(semitones);
    sut.setPitchMix(mix);
    sut.setReverse(reverse);

    std::vector<float> output;
    output.reserve(input.size());
    std::array<float, kBlockSize> blk{};
    for (size_t i = 0; i + kBlockSize <= input.size(); i += kBlockSize)
    {
        std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(i), kBlockSize, blk.begin());
        sut.process(blk);
        output.insert(output.end(), blk.begin(), blk.end());
    }
    return output;
}

[[nodiscard]] float measureFrequency(const std::vector<float>& signal)
{
    YinPitchDetector detector{kSampleRate, 60.0f, 1000.0f, 50.f};
    std::vector<float> pitches;
    for (const auto sample : signal)
    {
        std::ignore = detector.step(sample);
    }
    for (const auto sample : signal)
    {
        const float pitch = detector.step(sample);
        if (detector.hasNewPitch())
        {
            pitches.push_back(pitch);
        }
    }
    if (pitches.empty())
    {
        return 0.0f;
    }
    std::ranges::sort(pitches);
    return pitches[pitches.size() / 2];
}
}

TEST(BlockProcPitchTest, ZeroMixIsBitExactBypass)
{
    const auto input = generateSine(kBaseFrequency, 4000);
    const auto output = processThroughPitch(input, 12.0f, 0.0f);

    ASSERT_EQ(output.size(), (input.size() / kBlockSize) * kBlockSize);
    for (size_t i = 0; i < output.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], input[i]) << "mismatch at sample " << i;
    }
}

TEST(BlockProcPitchTest, PlusOneOctaveDoublesFrequency)
{
    const auto totalSamples = static_cast<size_t>(80.0 * kSampleRate / kBaseFrequency);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, 12.0f, 1.0f);

    const std::vector<float> settled(output.begin() + static_cast<std::ptrdiff_t>(output.size() / 2), output.end());
    const auto detected = measureFrequency(settled);

    EXPECT_NEAR(detected, kBaseFrequency * 2.0f, kPitchTolerance);
}

TEST(BlockProcPitchTest, MinusOneOctaveHalvesFrequency)
{
    // Shifted output is at half the input frequency, so it needs twice as many
    // samples to contain the same number of periods for a stable Yin measurement.
    const auto totalSamples = static_cast<size_t>(160.0 * kSampleRate / kBaseFrequency);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, -12.0f, 1.0f);

    const std::vector<float> settled(output.begin() + static_cast<std::ptrdiff_t>(output.size() / 2), output.end());
    const auto detected = measureFrequency(settled);

    EXPECT_NEAR(detected, kBaseFrequency * 0.5f, kPitchTolerance);
}

TEST(BlockProcPitchTest, ZeroSemitonesKeepsFrequencyUnchanged)
{
    const auto totalSamples = static_cast<size_t>(80.0 * kSampleRate / kBaseFrequency);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, 0.0f, 1.0f);

    const std::vector<float> settled(output.begin() + static_cast<std::ptrdiff_t>(output.size() / 2), output.end());
    const auto detected = measureFrequency(settled);

    EXPECT_NEAR(detected, kBaseFrequency, kPitchTolerance);
}

TEST(BlockProcPitchTest, ReverseStaysFinite)
{
    const auto input = generateSine(kBaseFrequency, 20000);
    const auto output = processThroughPitch(input, 7.0f, 1.0f, true);

    for (const auto sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(BlockProcPitchTest, ResetIsCallableWithoutDisruptingProcessing)
{
    BlockProc::Pitch<kBlockSize> sut{kSampleRate};
    sut.setPitch(3.0f);
    sut.setPitchMix(1.0f);

    const auto input = generateSine(kBaseFrequency, 4000);
    std::array<float, kBlockSize> blk{};
    for (size_t i = 0; i + kBlockSize <= input.size(); i += kBlockSize)
    {
        std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(i), kBlockSize, blk.begin());
        sut.process(blk);
        if (i == input.size() / 2)
        {
            sut.reset();
        }
        for (const auto sample : blk)
        {
            EXPECT_TRUE(std::isfinite(sample));
        }
    }
}

}
