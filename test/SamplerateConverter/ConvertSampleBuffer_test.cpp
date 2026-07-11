#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "SamplerateConverter/ConvertSampleBuffer.h"

namespace AbacDsp::Test
{

// ConvertSampleBuffer::convert<N> is designed for stereo (N=2):
//   - pushes in.size()/2 input frames
//   - resizes out to generated_frames * 2 stereo samples
//
// For mono (N=1) the helper has a known limitation (divides by 2 regardless),
// so these tests use N=2 exclusively.

namespace
{
constexpr size_t kFrames = 4096;
constexpr double kCountTolerance = 0.02; // 2 %
constexpr float kDcTolerance = 0.01f;
constexpr size_t kStartupSkipSamples = 200; // skip 100 frames × 2 channels
}

TEST(ConvertSampleBufferTest, NeutralRatioSampleCount)
{
    std::vector<float> in(kFrames * 2, 0.5f); // kFrames stereo frames
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(1.0f, in, out);

    // out holds generated_frames * 2 samples
    EXPECT_NEAR(static_cast<double>(out.size()), static_cast<double>(in.size()), in.size() * kCountTolerance);
}

TEST(ConvertSampleBufferTest, UpsampleRatio2SampleCount)
{
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(2.0f, in, out);

    EXPECT_NEAR(static_cast<double>(out.size()), static_cast<double>(in.size() * 2), in.size() * 2 * kCountTolerance);
}

TEST(ConvertSampleBufferTest, UpsampleRatio4SampleCount)
{
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(4.0f, in, out);

    EXPECT_NEAR(static_cast<double>(out.size()), static_cast<double>(in.size() * 4), in.size() * 4 * kCountTolerance);
}

TEST(ConvertSampleBufferTest, DownsampleRatioHalfSampleCount)
{
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(0.5f, in, out);

    EXPECT_NEAR(static_cast<double>(out.size()), static_cast<double>(in.size() / 2), in.size() / 2 * kCountTolerance);
}

TEST(ConvertSampleBufferTest, DownsampleRatioQuarterSampleCount)
{
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(0.25f, in, out);

    EXPECT_NEAR(static_cast<double>(out.size()), static_cast<double>(in.size() / 4), in.size() / 4 * kCountTolerance);
}

TEST(ConvertSampleBufferTest, DcPreservedNeutralRatio)
{
    std::vector<float> in(kFrames * 2, 1.0f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(1.0f, in, out);

    ASSERT_GT(out.size(), kStartupSkipSamples);
    for (size_t i = kStartupSkipSamples; i < out.size(); ++i)
    {
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at sample " << i;
    }
}

TEST(ConvertSampleBufferTest, DcPreservedUpsampleRatio2)
{
    std::vector<float> in(kFrames * 2, 1.0f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(2.0f, in, out);

    ASSERT_GT(out.size(), kStartupSkipSamples);
    for (size_t i = kStartupSkipSamples; i < out.size(); ++i)
    {
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at sample " << i;
    }
}

TEST(ConvertSampleBufferTest, DcPreservedDownsampleRatioHalf)
{
    std::vector<float> in(kFrames * 2, 1.0f);
    std::vector<float> out;

    ConvertSampleBuffer::convert<2>(0.5f, in, out);

    ASSERT_GT(out.size(), kStartupSkipSamples);
    for (size_t i = kStartupSkipSamples; i < out.size(); ++i)
    {
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at sample " << i;
    }
}

}
