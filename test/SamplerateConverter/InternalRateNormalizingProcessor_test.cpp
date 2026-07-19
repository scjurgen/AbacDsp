#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

#include "SamplerateConverter/InternalRateNormalizingProcessor.h"

namespace AbacDsp::Test
{

namespace
{

template <typename T>
class MockAudioBuffer
{
  public:
    MockAudioBuffer(const size_t numChannels, const size_t numSamples)
        : m_numChannels(numChannels)
        , m_numSamples(numSamples)
        , m_data(numChannels * numSamples)
    {
    }

    [[nodiscard]] size_t getNumChannels() const noexcept
    {
        return m_numChannels;
    }

    [[nodiscard]] size_t getNumSamples() const noexcept
    {
        return m_numSamples;
    }

    T* getWritePointer(const size_t channel) noexcept
    {
        return &m_data[channel * m_numSamples];
    }

    [[nodiscard]] const T* getReadPointer(const size_t channel) const noexcept
    {
        return &m_data[channel * m_numSamples];
    }

  private:
    size_t m_numChannels;
    size_t m_numSamples;
    std::vector<T> m_data;
};

constexpr size_t kChannels{2};
constexpr size_t kFixedFrameSize{16};
constexpr size_t kHostBlockSize{512};
constexpr size_t kNumBlocks{100};
constexpr size_t kStartupSkip{4000};
constexpr float kDcTolerance{0.02f};
constexpr float kRmsTolerance{0.02f};

using Proc = InternalRateNormalizingProcessor<kChannels, kFixedFrameSize, MockAudioBuffer<float>>;

[[nodiscard]] Proc::ProcessFunction identityProcessFunc()
{
    return [](const Proc::InternalBuffer& in, Proc::InternalBuffer& out)
    {
        for (size_t frame = 0; frame < in.numFrames(); ++frame)
        {
            for (size_t channel = 0; channel < in.numChannels(); ++channel)
            {
                out(frame, channel) = in(frame, channel);
            }
        }
    };
}

// Drives numBlocks host-rate callbacks of kHostBlockSize samples through an
// InternalRateNormalizingProcessor built with the given process function, feeding channel 0 from
// generator(sampleIndex) and channel 1 with silence. Returns the concatenated channel-0 output.
[[nodiscard]] std::vector<float> runRoundTrip(const float hostRate, Proc::ProcessFunction processFunc,
                                              const std::function<float(size_t)>& generator)
{
    Proc sut(hostRate, std::move(processFunc));
    std::vector<float> output;
    output.reserve(kHostBlockSize * kNumBlocks);

    size_t sampleIndex = 0;
    for (size_t block = 0; block < kNumBlocks; ++block)
    {
        MockAudioBuffer<float> buffer(kChannels, kHostBlockSize);
        for (size_t i = 0; i < kHostBlockSize; ++i)
        {
            buffer.getWritePointer(0)[i] = generator(sampleIndex++);
            buffer.getWritePointer(1)[i] = 0.f;
        }
        sut.processBlock(buffer);
        for (size_t i = 0; i < kHostBlockSize; ++i)
        {
            output.push_back(buffer.getReadPointer(0)[i]);
        }
    }
    return output;
}

}

TEST(InternalRateNormalizingProcessorTest, PassthroughAtInternalRateIsBitExact)
{
    Proc sut(Proc::kInternalSampleRate, identityProcessFunc());

    MockAudioBuffer<float> buffer(kChannels, 64);
    for (size_t i = 0; i < 64; ++i)
    {
        buffer.getWritePointer(0)[i] = static_cast<float>(i + 1);
        buffer.getWritePointer(1)[i] = static_cast<float>(i + 1) * 2.f;
    }
    sut.processBlock(buffer);

    // FixedSizeProcessor delays by exactly kFixedFrameSize samples; no sinc filter runs at
    // hostRate == kInternalSampleRate, so the delayed samples must be bit-exact, not just close.
    for (size_t i = 0; i < 64 - kFixedFrameSize; ++i)
    {
        EXPECT_FLOAT_EQ(buffer.getReadPointer(0)[i + kFixedFrameSize], static_cast<float>(i + 1));
        EXPECT_FLOAT_EQ(buffer.getReadPointer(1)[i + kFixedFrameSize], static_cast<float>(i + 1) * 2.f);
    }
}

class InternalRateNormalizingProcessorRateTest : public ::testing::TestWithParam<float>
{
};

TEST_P(InternalRateNormalizingProcessorRateTest, DcPreserved)
{
    const auto hostRate = GetParam();
    const auto output = runRoundTrip(hostRate, identityProcessFunc(), [](size_t) { return 1.f; });
    ASSERT_GT(output.size(), kStartupSkip);
    for (size_t i = kStartupSkip; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.f, kDcTolerance) << "at index " << i;
    }
}

TEST_P(InternalRateNormalizingProcessorRateTest, SineRmsPreserved)
{
    const auto hostRate = GetParam();
    constexpr double freq = 440.0;
    const auto output =
        runRoundTrip(hostRate, identityProcessFunc(),
                     [hostRate](const size_t index)
                     {
                         return static_cast<float>(std::sin(2.0 * std::numbers::pi * freq * static_cast<double>(index) /
                                                            static_cast<double>(hostRate)));
                     });
    ASSERT_GT(output.size(), kStartupSkip);
    double sumSquares = 0.0;
    bool allFinite = true;
    for (size_t i = kStartupSkip; i < output.size(); ++i)
    {
        allFinite = allFinite && std::isfinite(output[i]);
        sumSquares += static_cast<double>(output[i]) * output[i];
    }
    EXPECT_TRUE(allFinite);
    const auto rms = std::sqrt(sumSquares / static_cast<double>(output.size() - kStartupSkip));
    EXPECT_NEAR(rms, 1.0 / std::sqrt(2.0), kRmsTolerance);
}

INSTANTIATE_TEST_SUITE_P(StandardRates, InternalRateNormalizingProcessorRateTest,
                         ::testing::Values(22050.f, 44100.f, 48000.f, 88200.f, 96000.f, 192000.f),
                         [](const ::testing::TestParamInfo<float>& info)
                         { return "Rate" + std::to_string(static_cast<int>(info.param)); });

TEST(InternalRateNormalizingProcessorTest, ReconstructingAtDifferentRatesStaysFinite)
{
    for (const float hostRate : {44100.f, 48000.f, 192000.f, 22050.f, 96000.f, 88200.f})
    {
        const auto output =
            runRoundTrip(hostRate, identityProcessFunc(), [](size_t index)
                         { return 0.5f * static_cast<float>(std::sin(0.1 * static_cast<double>(index))); });
        for (const auto v : output)
        {
            EXPECT_TRUE(std::isfinite(v)) << "hostRate " << hostRate;
        }
    }
}

}
