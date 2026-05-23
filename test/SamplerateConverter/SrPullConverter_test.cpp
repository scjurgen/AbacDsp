#include "SamplerateConverter/SrPullConverter.h"
#include "Filters/Sinc/sinc_4.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <vector>

namespace AbacDsp::Test
{

namespace
{
auto makeSincFilter()
{
    return std::make_shared<SincFilter>(sinc4);
}

constexpr size_t kFrames = 4096;
constexpr double kCountTolerance = 0.02; // 2 %
constexpr float kDcTolerance = 0.01f;
constexpr size_t kStartupSkip = 100;

// ProcessCallback matches SrPullConverter::ProcessCallback (private alias).
using PullCb = std::function<long(float**, size_t)>;

// Serves all data in one shot, then signals end-of-input on the next call.
// numChannels: how many interleaved channels are in data (used to compute frame count).
PullCb makeCallback(const std::vector<float>& data, size_t numChannels = 1)
{
    return PullCb{[&data, numChannels, served = false](float** ptr, size_t) mutable -> long
    {
        if (served)
        {
            *ptr = nullptr;
            return 0;
        }
        served = true;
        *ptr = const_cast<float*>(data.data());
        return static_cast<long>(data.size() / numChannels);
    }};
}
} // namespace

// ----- frame-count tests -----
// For the pull converter, numSamples is the requested OUTPUT frame count.
// ratio > 1: upsample – N output frames consume ~N/ratio input frames.
// ratio < 1: downsample – N output frames consume ~N/ratio (> N) input frames.

TEST(SrPullConverterTest, NeutralRatioFrameCount)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 0.5f); // extra headroom
    std::vector<float> out(kFrames + 500);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(1.0f, out.data(), kFrames, 1, cb);

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames), kFrames * kCountTolerance);
}

TEST(SrPullConverterTest, UpsampleRatio2FrameCount)
{
    SrPullConverter sut{makeSincFilter()};
    // 2x upsample: kFrames*2 output needs ~kFrames input; provide 2x that to be safe
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out(kFrames * 2 + 500);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(2.0f, out.data(), kFrames * 2, 1, cb);

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames * 2), kFrames * 2 * kCountTolerance);
}

TEST(SrPullConverterTest, UpsampleRatio4FrameCount)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out(kFrames * 4 + 500);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(4.0f, out.data(), kFrames * 4, 1, cb);

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames * 4), kFrames * 4 * kCountTolerance);
}

TEST(SrPullConverterTest, DownsampleRatioHalfFrameCount)
{
    SrPullConverter sut{makeSincFilter()};
    // 0.5x downsample: kFrames output needs ~2*kFrames input; provide 3x that
    std::vector<float> in(kFrames * 6, 0.5f);
    std::vector<float> out(kFrames + 500);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(0.5f, out.data(), kFrames, 1, cb);

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames), kFrames * kCountTolerance);
}

// ----- DC preservation tests -----

TEST(SrPullConverterTest, DcPreservedNeutralRatio)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 1.0f);
    std::vector<float> out(kFrames + 500, 0.0f);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(1.0f, out.data(), kFrames, 1, cb);

    ASSERT_GT(generated, kStartupSkip);
    for (size_t i = kStartupSkip; i < generated; ++i)
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at index " << i;
}

TEST(SrPullConverterTest, DcPreservedUpsampleRatio2)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 1.0f);
    std::vector<float> out(kFrames * 2 + 500, 0.0f);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(2.0f, out.data(), kFrames * 2, 1, cb);

    ASSERT_GT(generated, kStartupSkip);
    for (size_t i = kStartupSkip; i < generated; ++i)
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at index " << i;
}

TEST(SrPullConverterTest, DcPreservedDownsampleRatioHalf)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 6, 1.0f);
    std::vector<float> out(kFrames + 500, 0.0f);

    auto cb = makeCallback(in);
    auto generated = sut.fetchBlock(0.5f, out.data(), kFrames, 1, cb);

    ASSERT_GT(generated, kStartupSkip);
    for (size_t i = kStartupSkip; i < generated; ++i)
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at index " << i;
}

// ----- stereo path -----

TEST(SrPullConverterTest, NeutralRatioFrameCountStereo)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2 * 2, 0.5f); // 2x headroom, 2 channels
    std::vector<float> out((kFrames + 500) * 2);

    auto cb = makeCallback(in, 2);
    auto generated = sut.fetchBlock(1.0f, out.data(), kFrames, 2, cb);

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames), kFrames * kCountTolerance);
}

TEST(SrPullConverterTest, UpsampleRatio2FrameCountStereo)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2 * 2, 0.5f);
    std::vector<float> out((kFrames * 2 + 500) * 2);

    auto cb = makeCallback(in, 2);
    auto generated = sut.fetchBlock(2.0f, out.data(), kFrames * 2, 2, cb);

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames * 2), kFrames * 2 * kCountTolerance);
}

// ----- reset restores initial state -----

TEST(SrPullConverterTest, ResetRestoresInitialState)
{
    SrPullConverter sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 1.0f);

    // warm up, reset, then process again
    std::vector<float> dummy(kFrames + 500);
    auto cb0 = makeCallback(in);
    sut.fetchBlock(1.0f, dummy.data(), kFrames, 1, cb0);
    sut.reset();

    std::vector<float> outAfterReset(kFrames + 500);
    auto cb1 = makeCallback(in);
    auto gen = sut.fetchBlock(1.0f, outAfterReset.data(), kFrames, 1, cb1);

    SrPullConverter fresh{makeSincFilter()};
    std::vector<float> outFresh(kFrames + 500);
    auto cb2 = makeCallback(in);
    auto genFresh = fresh.fetchBlock(1.0f, outFresh.data(), kFrames, 1, cb2);

    ASSERT_EQ(gen, genFresh);
    for (size_t i = 0; i < gen; ++i)
        EXPECT_NEAR(outAfterReset[i], outFresh[i], 1e-5f) << "at index " << i;
}

} // namespace AbacDsp::Test
