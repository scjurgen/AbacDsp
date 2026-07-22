#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "BlockProcessors/BlockProcPhaseVocoderPitch.h"
#include "NaiveGenerators/Generator.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};
constexpr float kBaseFrequency{220.f};
constexpr float kPitchTolerance{10.0f};

[[nodiscard]] std::vector<float> generateSine(const float frequency, const size_t numSamples)
{
    std::vector<float> signal(numSamples);
    Generator<Wave::Sine> generator{kSampleRate, frequency};
    generator.render(signal.begin(), signal.end());
    return signal;
}

[[nodiscard]] std::vector<float> processThroughPitch(const std::vector<float>& input, const float semitones,
                                                     const float mix)
{
    BlockProc::PhaseVocoderPitch<kBlockSize> sut{kSampleRate};
    sut.setPitch(semitones);
    sut.setPitchMix(mix);

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

TEST(BlockProcPhaseVocoderPitchTest, ZeroMixIsBitExactBypass)
{
    const auto input = generateSine(kBaseFrequency, 4000);
    const auto output = processThroughPitch(input, 12.0f, 0.0f);

    ASSERT_EQ(output.size(), (input.size() / kBlockSize) * kBlockSize);
    for (size_t i = 0; i < output.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], input[i]) << "mismatch at sample " << i;
    }
}

TEST(BlockProcPhaseVocoderPitchTest, PlusOneOctaveDoublesFrequency)
{
    const auto totalSamples = static_cast<size_t>(400.0 * kSampleRate / kBaseFrequency);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, 12.0f, 1.0f);

    const std::vector<float> settled(output.begin() + static_cast<std::ptrdiff_t>(output.size() / 2), output.end());
    const auto detected = measureFrequency(settled);

    EXPECT_NEAR(detected, kBaseFrequency * 2.0f, kPitchTolerance);
}

TEST(BlockProcPhaseVocoderPitchTest, ZeroSemitonesKeepsFrequencyUnchanged)
{
    const auto totalSamples = static_cast<size_t>(200.0 * kSampleRate / kBaseFrequency);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, 0.0f, 1.0f);

    const std::vector<float> settled(output.begin() + static_cast<std::ptrdiff_t>(output.size() / 2), output.end());
    const auto detected = measureFrequency(settled);

    EXPECT_NEAR(detected, kBaseFrequency, kPitchTolerance);
}

TEST(BlockProcPhaseVocoderPitchTest, PitchDownStaysFiniteAndBounded)
{
    const auto totalSamples = static_cast<size_t>(200.0 * kSampleRate / kBaseFrequency);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, -12.0f, 1.0f);

    for (const auto sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
        EXPECT_LE(std::abs(sample), 2.0f);
    }
}

TEST(BlockProcPhaseVocoderPitchTest, LongRunStaysFiniteAndBounded)
{
    constexpr size_t totalSamples = 4 * static_cast<size_t>(kSampleRate);
    const auto input = generateSine(kBaseFrequency, totalSamples);
    const auto output = processThroughPitch(input, 5.0f, 1.0f);

    ASSERT_FALSE(output.empty());
    float maxAbs = 0.0f;
    for (const auto sample : output)
    {
        ASSERT_TRUE(std::isfinite(sample));
        maxAbs = std::max(maxAbs, std::abs(sample));
    }
    EXPECT_GT(maxAbs, 0.1f) << "Output should not have gone silent across a buffer compaction";
    EXPECT_LE(maxAbs, 2.0f);
}

TEST(BlockProcPhaseVocoderPitchTest, ResetIsCallableWithoutDisruptingProcessing)
{
    BlockProc::PhaseVocoderPitch<kBlockSize> sut{kSampleRate};
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
