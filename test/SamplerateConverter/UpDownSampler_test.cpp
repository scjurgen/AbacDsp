#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

#include "SamplerateConverter/UpDownSampler.h"

namespace AbacDsp::Test
{

namespace
{

constexpr size_t kMaxBlock{512};
constexpr size_t kFrame{16};
constexpr size_t kWarmup{4000};
constexpr float kToneCyclesPerSample{0.02f};
constexpr float kToneAmplitude{0.5f};
constexpr float kToneRms{kToneAmplitude * 0.70710678f};

class IdentityProcessor
{
  public:
    void processBlock(std::span<const float> source, std::span<float> target) noexcept
    {
        std::ranges::copy(source, target.begin());
    }
};

class GainProcessor
{
  public:
    explicit GainProcessor(const float gain)
        : m_gain(gain)
    {
    }

    void processBlock(std::span<const float> source, std::span<float> target) noexcept
    {
        std::ranges::transform(source, target.begin(), [this](const float in) { return in * m_gain; });
    }

  private:
    float m_gain;
};

class RecordingProcessor
{
  public:
    explicit RecordingProcessor(const size_t expectedSize)
        : m_expectedSize(expectedSize)
    {
    }

    void processBlock(std::span<const float> source, std::span<float> target) noexcept
    {
        ++calls;
        wrongSizeCalls += (source.size() != m_expectedSize || target.size() != m_expectedSize) ? 1 : 0;
        std::ranges::copy(source, target.begin());
    }

    size_t calls{0};
    size_t wrongSizeCalls{0};

  private:
    size_t m_expectedSize;
};

using Identity = UpDownSampler<IdentityProcessor, kFrame>;

[[nodiscard]] std::vector<float> makeSine(const size_t numSamples, const float cyclesPerSample = kToneCyclesPerSample)
{
    std::vector<float> signal(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        signal[i] =
            kToneAmplitude * std::sin(2.f * std::numbers::pi_v<float> * cyclesPerSample * static_cast<float>(i));
    }
    return signal;
}

template <typename Sut>
[[nodiscard]] std::vector<float> runThrough(Sut& sut, const std::vector<float>& input, const size_t blockSize,
                                            const std::function<float(size_t)>& ratioAtSample)
{
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += blockSize)
    {
        const auto count = std::min(blockSize, input.size() - pos);
        sut.setRatio(ratioAtSample(pos));
        sut.processBlock(std::span<const float>{input}.subspan(pos, count),
                         std::span<float>{output}.subspan(pos, count));
    }
    return output;
}

template <typename Sut>
[[nodiscard]] std::vector<float> runThrough(Sut& sut, const std::vector<float>& input, const size_t blockSize,
                                            const float ratio)
{
    return runThrough(sut, input, blockSize, [ratio](size_t) { return ratio; });
}

struct Alignment
{
    int lag;
    float rms;
};

// Finds the integer lag at which b[i + lag] best matches a[i] over [from, from + length).
[[nodiscard]] Alignment alignSignals(const std::vector<float>& a, const std::vector<float>& b, const int maxLag,
                                     const size_t from, const size_t length)
{
    Alignment best{0, std::numeric_limits<float>::max()};
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double sum{0.0};
        for (size_t i = from; i < from + length; ++i)
        {
            const auto diff = static_cast<double>(b[static_cast<size_t>(static_cast<int>(i) + lag)] - a[i]);
            sum += diff * diff;
        }
        const auto rms = static_cast<float>(std::sqrt(sum / static_cast<double>(length)));
        if (rms < best.rms)
        {
            best = {lag, rms};
        }
    }
    return best;
}

[[nodiscard]] float rmsOf(const std::vector<float>& signal, const size_t from, const size_t length)
{
    double sum{0.0};
    for (size_t i = from; i < from + length; ++i)
    {
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(length)));
}

[[nodiscard]] float maxStep(const std::vector<float>& signal, const size_t from, const size_t to)
{
    float step{0.f};
    for (size_t i = std::max<size_t>(from, 1); i < to; ++i)
    {
        step = std::max(step, std::abs(signal[i] - signal[i - 1]));
    }
    return step;
}

[[nodiscard]] bool allFinite(const std::vector<float>& signal)
{
    return std::ranges::all_of(signal, [](const float v) { return std::isfinite(v); });
}

}

TEST(UpDownSamplerTest, RatioBoundsAreAccepted)
{
    Identity sut(kMaxBlock);
    sut.setRatio(Identity::kMaxRatio);
    EXPECT_FLOAT_EQ(sut.ratio(), Identity::kMaxRatio);
    sut.setRatio(Identity::kMinRatio);
    EXPECT_FLOAT_EQ(sut.ratio(), Identity::kMinRatio);
}

TEST(UpDownSamplerTest, ForwardsConstructorArgumentsToProcessor)
{
    UpDownSampler<GainProcessor, kFrame> sut(kMaxBlock, 2.f);
    const std::vector<float> dc(8000, 0.25f);
    const auto output = runThrough(sut, dc, 64, 1.f);
    EXPECT_NEAR(output.back(), 0.5f, 0.01f);
}

TEST(UpDownSamplerTest, RatioOneReproducesInputDelayed)
{
    Identity sut(kMaxBlock);
    const auto input = makeSine(20000);
    const auto output = runThrough(sut, input, 64, 1.f);

    const auto alignment = alignSignals(input, output, 600, 2000, 10000);
    EXPECT_GT(alignment.lag, 0);
    EXPECT_LT(alignment.rms, 0.002f) << "lag " << alignment.lag;
}

TEST(UpDownSamplerTest, SourceMayAliasTarget)
{
    Identity separate(kMaxBlock);
    Identity inPlace(kMaxBlock);
    const auto input = makeSine(6000);
    const auto expected = runThrough(separate, input, 100, 2.f);

    auto buffer = input;
    inPlace.setRatio(2.f);
    for (size_t pos = 0; pos < buffer.size(); pos += 100)
    {
        const auto count = std::min<size_t>(100, buffer.size() - pos);
        inPlace.processBlock(std::span<const float>{buffer}.subspan(pos, count),
                             std::span<float>{buffer}.subspan(pos, count));
    }
    EXPECT_EQ(buffer, expected);
}

TEST(UpDownSamplerTest, ProcessorSeesExactlyFixedFrameSizeBlocks)
{
    UpDownSampler<RecordingProcessor, kFrame> sut(kMaxBlock, kFrame);
    static_cast<void>(runThrough(sut, makeSine(8000), 100, 2.f));
    EXPECT_GT(sut.processor().calls, 0u);
    EXPECT_EQ(sut.processor().wrongSizeCalls, 0u);
}

TEST(UpDownSamplerTest, FixedFrameSizeOneCallsProcessorPerSample)
{
    UpDownSampler<RecordingProcessor, 1> sut(kMaxBlock, 1);
    const auto output = runThrough(sut, std::vector<float>(8000, 1.f), 100, 2.f);
    EXPECT_GT(sut.processor().calls, 8000u);
    EXPECT_EQ(sut.processor().wrongSizeCalls, 0u);
    EXPECT_NEAR(output.back(), 1.f, 0.01f);
}

class UpDownSamplerRatioTest : public ::testing::TestWithParam<float>
{
};

TEST_P(UpDownSamplerRatioTest, DcPreservedWithoutUnderruns)
{
    Identity sut(kMaxBlock);
    const auto output = runThrough(sut, std::vector<float>(20000, 1.f), 64, GetParam());

    EXPECT_TRUE(allFinite(output));
    for (size_t i = kWarmup; i < output.size(); ++i)
    {
        ASSERT_NEAR(output[i], 1.f, 0.01f) << "at index " << i;
    }
    EXPECT_EQ(sut.underruns(), 0u);
}

TEST_P(UpDownSamplerRatioTest, ToneSurvivesAtItsOwnLevel)
{
    Identity sut(kMaxBlock);
    const auto output = runThrough(sut, makeSine(30000), 64, GetParam());
    EXPECT_NEAR(rmsOf(output, 10000, 16000), kToneRms, kToneRms * 0.02f);
}

TEST_P(UpDownSamplerRatioTest, OutputDoesNotDependOnBlockSize)
{
    Identity reference(kMaxBlock);
    const auto input = makeSine(24000);
    const auto expected = runThrough(reference, input, 64, GetParam());

    for (const size_t blockSize : {size_t{1}, size_t{7}, size_t{100}, size_t{1500}})
    {
        Identity sut(kMaxBlock);
        const auto output = runThrough(sut, input, blockSize, GetParam());
        const auto alignment = alignSignals(expected, output, 700, 6000, 12000);
        EXPECT_LT(alignment.rms, 1e-4f) << "blockSize " << blockSize << " lag " << alignment.lag;
    }
}

INSTANTIATE_TEST_SUITE_P(Ratios, UpDownSamplerRatioTest, ::testing::Values(0.0625f, 0.25f, 0.5f, 1.f, 2.f, 4.f, 16.f));

TEST(UpDownSamplerTest, AbruptRatioJumpsStayBounded)
{
    Identity sut(kMaxBlock);
    constexpr size_t kSegment{12000};
    constexpr size_t kBurstWindow{1500};
    const auto input = makeSine(3 * kSegment);
    const auto output = runThrough(sut, input, 64, [](const size_t pos) { return pos / kSegment == 1 ? 2.f : 0.5f; });

    EXPECT_TRUE(allFinite(output));
    EXPECT_LT(*std::ranges::max_element(output), kToneAmplitude * 1.5f);
    EXPECT_GT(*std::ranges::min_element(output), -kToneAmplitude * 1.5f);
    EXPECT_LT(maxStep(output, kWarmup, output.size()), kToneAmplitude);

    const auto steadyStep = kToneAmplitude * 2.f * std::numbers::pi_v<float> * kToneCyclesPerSample;
    EXPECT_LT(maxStep(output, kWarmup, kSegment), steadyStep * 1.1f);
    EXPECT_LT(maxStep(output, kSegment + kBurstWindow, 2 * kSegment), steadyStep * 1.1f);
    EXPECT_LT(maxStep(output, 2 * kSegment + kBurstWindow, output.size()), steadyStep * 1.1f);
    for (size_t segment = 0; segment < 3; ++segment)
    {
        EXPECT_NEAR(rmsOf(output, (segment + 1) * kSegment - 4000, 4000), kToneRms, kToneRms * 0.03f)
            << "segment " << segment;
    }
}

TEST(UpDownSamplerTest, SlowSweepThroughOneHasNoDiscontinuity)
{
    Identity sut(kMaxBlock);
    constexpr size_t kLength{40000};
    const auto input = makeSine(kLength);
    const auto ratioAt = [](const size_t pos)
    {
        const auto phase = static_cast<float>(pos) / static_cast<float>(kLength);
        return 1.f + 0.2f * std::sin(2.f * std::numbers::pi_v<float> * 2.f * phase);
    };
    const auto output = runThrough(sut, input, 64, ratioAt);

    EXPECT_TRUE(allFinite(output));
    const auto steadyStep = kToneAmplitude * 2.f * std::numbers::pi_v<float> * kToneCyclesPerSample;
    EXPECT_LT(maxStep(output, kWarmup, output.size()), steadyStep * 1.5f);
    EXPECT_EQ(sut.underruns(), 0u);
}

TEST(UpDownSamplerTest, ExtremeRatioJumpsDoNotBreakTheStream)
{
    Identity sut(kMaxBlock);
    const auto input = makeSine(60000);
    const auto output = runThrough(sut, input, 128,
                                   [](const size_t pos)
                                   {
                                       const auto ratios = std::to_array<float>({16.f, 0.0625f, 1.f, 16.f, 0.0625f});
                                       return ratios[(pos / 12000) % ratios.size()];
                                   });
    EXPECT_TRUE(allFinite(output));
    EXPECT_LT(*std::ranges::max_element(output), kToneAmplitude * 2.f);
}

}
