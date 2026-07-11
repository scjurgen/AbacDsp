#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "BlockProcessors/BlockProcLowpass.h"
#include "NaiveGenerators/Generator.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};

[[nodiscard]] float processConstant(BlockProc::Lowpass<kBlockSize>& sut, const float value, const size_t numBlocks)
{
    std::array<float, kBlockSize> blk{};
    float last{0.0f};
    for (size_t b = 0; b < numBlocks; ++b)
    {
        blk.fill(value);
        sut.process(blk);
        last = blk.back();
    }
    return last;
}

[[nodiscard]] float settledPeakAmplitude(const float cutoff, const float frequency, const size_t numBlocks)
{
    BlockProc::Lowpass<kBlockSize> sut{kSampleRate};
    sut.setCutoff(cutoff);
    Generator<Wave::Sine> wave{kSampleRate, frequency};

    float peak{0.0f};
    std::array<float, kBlockSize> blk{};
    for (size_t b = 0; b < numBlocks; ++b)
    {
        wave.render(blk.begin(), blk.end());
        sut.process(blk);
        if (b >= numBlocks / 2)
        {
            for (const auto sample : blk)
            {
                peak = std::max(peak, std::abs(sample));
            }
        }
    }
    return peak;
}
}

TEST(BlockProcLowpassTest, DcInputConvergesToSameConstant)
{
    BlockProc::Lowpass<kBlockSize> sut{kSampleRate};
    const auto settled = processConstant(sut, 0.75f, 200);
    EXPECT_NEAR(settled, 0.75f, 1e-4f);
}

TEST(BlockProcLowpassTest, LowFrequencyPassesMoreThanHighFrequency)
{
    constexpr float cutoff{1000.f};
    const auto lowFreqPeak = settledPeakAmplitude(cutoff, 100.f, 400);
    const auto highFreqPeak = settledPeakAmplitude(cutoff, 8000.f, 400);

    EXPECT_GT(lowFreqPeak, highFreqPeak * 2.0f);
}

TEST(BlockProcLowpassTest, RaisingCutoffIncreasesAmplitudeAtFixedFrequency)
{
    constexpr float testFrequency{2000.f};
    const auto lowCutoffPeak = settledPeakAmplitude(500.f, testFrequency, 400);
    const auto highCutoffPeak = settledPeakAmplitude(8000.f, testFrequency, 400);

    EXPECT_GT(highCutoffPeak, lowCutoffPeak);
}

TEST(BlockProcLowpassTest, StepResponseIsMonotonicAndSettles)
{
    BlockProc::Lowpass<kBlockSize> sut{kSampleRate};
    std::array<float, kBlockSize> blk{};

    float previous{0.0f};
    float last{0.0f};
    for (size_t b = 0; b < 100; ++b)
    {
        blk.fill(1.0f);
        sut.process(blk);
        for (const auto sample : blk)
        {
            EXPECT_GE(sample, previous - 1e-6f);
            previous = sample;
        }
        last = blk.back();
    }
    EXPECT_NEAR(last, 1.0f, 1e-3f);
}

}
