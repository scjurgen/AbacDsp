#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "Analysis/ZeroCrossings.h"
#include "BlockProcessors/BlockProcVibrato.h"
#include "NaiveGenerators/Generator.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};
using SutType = BlockProc::Vibrato<kBlockSize>;

[[nodiscard]] std::vector<float> processThroughVibrato(SutType& sut, const std::vector<float>& input)
{
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

[[nodiscard]] std::vector<float> generateSine(const float frequency, const size_t numSamples)
{
    std::vector<float> signal(numSamples);
    Generator<Wave::Sine> generator{kSampleRate, frequency};
    generator.render(signal.begin(), signal.end());
    return signal;
}
}

TEST(BlockProcVibratoTest, ZeroDepthActsAsFixedDelay)
{
    SutType sut{kSampleRate};
    sut.setModDepth(0.0f);
    sut.setModSpeed(5.0f);

    const auto totalSamples = SutType::MaxDepth + 4000;
    const auto input = generateSine(220.f, totalSamples);
    const auto output = processThroughVibrato(sut, input);

    for (size_t n = 0; n < SutType::MaxDepth; ++n)
    {
        EXPECT_NEAR(output[n], 0.0f, 1e-6f) << "unexpected non-zero output before buffer fill at " << n;
    }
    for (size_t n = SutType::MaxDepth; n < output.size(); ++n)
    {
        EXPECT_NEAR(output[n], input[n - SutType::MaxDepth], 1e-4f) << "delay mismatch at sample " << n;
    }
}

TEST(BlockProcVibratoTest, ResetFullyRestoresInitialState)
{
    const auto totalSamples = SutType::MaxDepth + 2000;
    const auto referenceInput = generateSine(220.f, totalSamples);

    SutType reference{kSampleRate};
    reference.setModDepth(0.0f);
    const auto referenceOutput = processThroughVibrato(reference, referenceInput);

    SutType dirtied{kSampleRate};
    dirtied.setModDepth(0.0f);
    const std::vector<float> garbage(SutType::MaxDepth + 500, 1.0f);
    std::ignore = processThroughVibrato(dirtied, garbage);
    dirtied.reset();
    const auto dirtiedOutput = processThroughVibrato(dirtied, referenceInput);

    ASSERT_EQ(referenceOutput.size(), dirtiedOutput.size());
    for (size_t i = 0; i < referenceOutput.size(); ++i)
    {
        EXPECT_NEAR(referenceOutput[i], dirtiedOutput[i], 1e-6f) << "mismatch at sample " << i;
    }
}

TEST(BlockProcVibratoTest, HigherDepthProducesMorePeriodVariability)
{
    constexpr size_t settleSamples{6000};
    constexpr size_t measureSamples{200000};
    constexpr float testFrequency{300.f};

    const auto measurePeriodStdDev = [&](const float depth)
    {
        SutType sut{kSampleRate};
        sut.setModSpeed(4.0f);
        sut.setModDepth(depth);
        sut.setVariance(0.0f);
        sut.setDrift(0.0f);

        const auto input = generateSine(testFrequency, settleSamples + measureSamples);
        const auto output = processThroughVibrato(sut, input);

        const std::vector<float> settled(output.begin() + static_cast<std::ptrdiff_t>(settleSamples), output.end());
        const auto stats = calculateZeroCrossingStatistics(settled.data(), settled.size(), true);
        return stats.standardDeviation;
    };

    const auto lowDepthStdDev = measurePeriodStdDev(0.05f);
    const auto highDepthStdDev = measurePeriodStdDev(0.9f);

    EXPECT_GT(highDepthStdDev, lowDepthStdDev);
}

TEST(BlockProcVibratoTest, BoundaryValuesStayFinite)
{
    for (const float depth : {0.0f, 0.001f, 0.5f, 1.0f})
    {
        for (const float speed : {0.0f, 0.1f, 20.0f})
        {
            for (const float variance : {0.0f, 1.0f})
            {
                SutType sut{kSampleRate};
                sut.setModDepth(depth);
                sut.setModSpeed(speed);
                sut.setVariance(variance);
                sut.setDrift(variance);

                std::array<float, kBlockSize> blk{};
                for (size_t b = 0; b < 200; ++b)
                {
                    blk.fill(1.0f);
                    sut.process(blk);
                    for (const auto sample : blk)
                    {
                        EXPECT_TRUE(std::isfinite(sample))
                            << "depth=" << depth << " speed=" << speed << " variance=" << variance;
                    }
                }
            }
        }
    }
}

}
