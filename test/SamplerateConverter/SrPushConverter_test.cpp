#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Filters/Sinc/sinc_4.h"
#include "SamplerateConverter/SrPushConverter.h"

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
constexpr size_t kStartupSkip = 100; // skip initial FIR transient
}

// ----- frame-count tests (ratio determines how many output frames are generated) -----

TEST(SrPushConverterTest, NeutralRatioFrameCountMono)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 0.5f);
    std::vector<float> out(kFrames + 500);

    auto generated = sut.fetchBlock(1.0f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames), kFrames * kCountTolerance);
}

TEST(SrPushConverterTest, UpsampleRatio2FrameCountMono)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 0.5f);
    std::vector<float> out(kFrames * 3);

    auto generated = sut.fetchBlock(2.0f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames * 2), kFrames * 2 * kCountTolerance);
}

TEST(SrPushConverterTest, UpsampleRatio4FrameCountMono)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 0.5f);
    std::vector<float> out(kFrames * 5);

    auto generated = sut.fetchBlock(4.0f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames * 4), kFrames * 4 * kCountTolerance);
}

TEST(SrPushConverterTest, DownsampleRatioHalfFrameCountMono)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 0.5f);
    std::vector<float> out(kFrames);

    auto generated = sut.fetchBlock(0.5f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames) / 2.0,
                static_cast<double>(kFrames) / 2.0 * kCountTolerance);
}

TEST(SrPushConverterTest, DownsampleRatioQuarterFrameCountMono)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 0.5f);
    std::vector<float> out(kFrames);

    auto generated = sut.fetchBlock(0.25f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames) / 4.0,
                static_cast<double>(kFrames) / 4.0 * kCountTolerance);
}

// ----- DC preservation tests (converters must not alter DC level) -----

TEST(SrPushConverterTest, DcPreservedNeutralRatio)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 1.0f);
    std::vector<float> out(kFrames + 500, 0.0f);

    auto generated = sut.fetchBlock(1.0f, in.data(), kFrames, out.data(), out.size());

    ASSERT_GT(generated, kStartupSkip);
    for (size_t i = kStartupSkip; i < generated; ++i)
    {
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at index " << i;
    }
}

TEST(SrPushConverterTest, DcPreservedUpsampleRatio2)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 1.0f);
    std::vector<float> out(kFrames * 3, 0.0f);

    auto generated = sut.fetchBlock(2.0f, in.data(), kFrames, out.data(), out.size());

    ASSERT_GT(generated, kStartupSkip);
    for (size_t i = kStartupSkip; i < generated; ++i)
    {
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at index " << i;
    }
}

TEST(SrPushConverterTest, DcPreservedDownsampleRatioHalf)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 1.0f);
    std::vector<float> out(kFrames, 0.0f);

    auto generated = sut.fetchBlock(0.5f, in.data(), kFrames, out.data(), out.size());

    ASSERT_GT(generated, kStartupSkip);
    for (size_t i = kStartupSkip; i < generated; ++i)
    {
        EXPECT_NEAR(out[i], 1.0f, kDcTolerance) << "at index " << i;
    }
}

// ----- stereo (MAXCHANNELS=2) -----

TEST(SrPushConverterTest, NeutralRatioFrameCountStereo)
{
    SrPushConverter<2> sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 0.5f); // interleaved L/R
    std::vector<float> out((kFrames + 500) * 2);

    auto generated = sut.fetchBlock(1.0f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames), kFrames * kCountTolerance);
}

TEST(SrPushConverterTest, UpsampleRatio2FrameCountStereo)
{
    SrPushConverter<2> sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out(kFrames * 3 * 2);

    auto generated = sut.fetchBlock(2.0f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames * 2), kFrames * 2 * kCountTolerance);
}

TEST(SrPushConverterTest, DownsampleRatioHalfFrameCountStereo)
{
    SrPushConverter<2> sut{makeSincFilter()};
    std::vector<float> in(kFrames * 2, 0.5f);
    std::vector<float> out(kFrames * 2);

    auto generated = sut.fetchBlock(0.5f, in.data(), kFrames, out.data(), out.size());

    EXPECT_NEAR(static_cast<double>(generated), static_cast<double>(kFrames) / 2.0,
                static_cast<double>(kFrames) / 2.0 * kCountTolerance);
}

// ----- reset clears all internal state -----

TEST(SrPushConverterTest, ResetRestoresInitialState)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 1.0f);
    std::vector<float> outAfterReset(kFrames + 500);

    // warm up, then reset
    std::vector<float> dummy(kFrames + 500);
    std::ignore = sut.fetchBlock(1.0f, in.data(), kFrames, dummy.data(), dummy.size());
    sut.reset();
    auto gen = sut.fetchBlock(1.0f, in.data(), kFrames, outAfterReset.data(), outAfterReset.size());

    SrPushConverter<1> fresh{makeSincFilter()};
    std::vector<float> outFresh(kFrames + 500);
    auto genFresh = fresh.fetchBlock(1.0f, in.data(), kFrames, outFresh.data(), outFresh.size());

    ASSERT_EQ(gen, genFresh);
    for (size_t i = 0; i < gen; ++i)
    {
        EXPECT_NEAR(outAfterReset[i], outFresh[i], 1e-5f) << "at index " << i;
    }
}

// ----- ratio glide (consecutive blocks with a changing ratio) -----

// A second fetchBlock at a different ratio exercises the interpolation that ramps
// the ratio across the block, plus the already-initialized last-ratio path.
TEST(SrPushConverterTest, RatioGlideAcrossConsecutiveBlocksMono)
{
    SrPushConverter<1> sut{makeSincFilter()};
    std::vector<float> in(kFrames, 0.5f);
    std::vector<float> out(kFrames * 3);

    std::ignore = sut.fetchBlock(1.0f, in.data(), kFrames, out.data(), out.size());          // sets last ratio
    const auto generated = sut.fetchBlock(2.0f, in.data(), kFrames, out.data(), out.size()); // glides 1 -> 2

    EXPECT_GT(generated, 0u);
    for (size_t i = kStartupSkip; i < generated; ++i)
    {
        EXPECT_TRUE(std::isfinite(out[i]));
    }
}

TEST(SrPushConverterTest, StreamingAlternatingRatiosStereo)
{
    SrPushConverter<2> sut{makeSincFilter()};
    constexpr size_t frames = 1024;
    std::vector<float> in(frames * 2, 0.3f); // interleaved stereo
    std::vector<float> out(frames * 6);

    for (int block = 0; block < 8; ++block)
    {
        const float ratio = (block % 2 == 0) ? 0.5f : 2.0f; // down then up: both scale paths + a glide each block
        const auto generated = sut.fetchBlock(ratio, in.data(), frames, out.data(), out.size());
        for (size_t i = 0; i < generated * 2; ++i)
        {
            EXPECT_TRUE(std::isfinite(out[i]));
        }
    }
}

}
