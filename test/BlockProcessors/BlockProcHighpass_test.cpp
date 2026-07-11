#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "BlockProcessors/BlockProcHighpass.h"
#include "NaiveGenerators/Generator.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};

[[nodiscard]] float processConstant(BlockProc::Highpass<kBlockSize>& sut, const float value, const size_t numBlocks)
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
    BlockProc::Highpass<kBlockSize> sut{kSampleRate};
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

TEST(BlockProcHighpassTest, DcInputDecaysTowardZero)
{
    BlockProc::Highpass<kBlockSize> sut{kSampleRate};
    const auto settled = processConstant(sut, 0.75f, 200);
    EXPECT_NEAR(settled, 0.0f, 1e-3f);
}

TEST(BlockProcHighpassTest, HighFrequencyPassesMoreThanLowFrequency)
{
    constexpr float cutoff{1000.f};
    const auto lowFreqPeak = settledPeakAmplitude(cutoff, 100.f, 400);
    const auto highFreqPeak = settledPeakAmplitude(cutoff, 8000.f, 400);

    EXPECT_GT(highFreqPeak, lowFreqPeak * 2.0f);
}

TEST(BlockProcHighpassTest, LoweringCutoffIncreasesAmplitudeAtFixedFrequency)
{
    constexpr float testFrequency{2000.f};
    const auto lowCutoffPeak = settledPeakAmplitude(500.f, testFrequency, 400);
    const auto highCutoffPeak = settledPeakAmplitude(8000.f, testFrequency, 400);

    EXPECT_GT(lowCutoffPeak, highCutoffPeak);
}

}
