#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/SpectrumAnalyzer.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kNumBands{240};

[[nodiscard]] std::vector<float> sine(const float amplitude, const float frequency, const float seconds)
{
    std::vector<float> out(static_cast<size_t>(seconds * kSampleRate));
    for (size_t n = 0; n < out.size(); ++n)
    {
        out[n] =
            amplitude * std::sin(2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(n) / kSampleRate);
    }
    return out;
}

[[nodiscard]] size_t bandOf(const SpectrumAnalyzer& analyzer, const float frequency)
{
    const auto centers = analyzer.bandCenters();
    const auto nearest =
        std::ranges::min_element(centers, {}, [frequency](const float c) { return std::abs(std::log(c / frequency)); });
    return static_cast<size_t>(nearest - centers.begin());
}

[[nodiscard]] float maxBandDb(const SpectrumAnalyzer& analyzer)
{
    return std::ranges::max(analyzer.bandsDb());
}
}

TEST(SpectrumAnalyzerTest, BandCentersAreIncreasingAndCoverTheRange)
{
    const SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
    const auto centers = analyzer.bandCenters();
    ASSERT_EQ(centers.size(), kNumBands);
    EXPECT_TRUE(std::ranges::is_sorted(centers));
    EXPECT_GT(centers.front(), 20.f);
    EXPECT_LT(centers.front(), 21.f);
    EXPECT_LT(centers.back(), 20000.f);
    EXPECT_GT(centers.back(), 19000.f);
}

TEST(SpectrumAnalyzerTest, FftSizeIsRoundedAndClamped)
{
    SpectrumAnalyzer analyzer(kSampleRate, kNumBands, 8192);
    EXPECT_EQ(analyzer.fftSize(), 8192u);
    analyzer.setFftSize(3000);
    EXPECT_EQ(analyzer.fftSize(), 4096u);
    analyzer.setFftSize(16);
    EXPECT_EQ(analyzer.fftSize(), SpectrumAnalyzer::kMinFftSize);
    analyzer.setFftSize(65536);
    EXPECT_EQ(analyzer.fftSize(), 8192u);
}

TEST(SpectrumAnalyzerTest, FrameRateIsIndependentOfFftSize)
{
    for (const size_t size : {2048u, 16384u})
    {
        SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
        analyzer.setFftSize(size);
        EXPECT_EQ(analyzer.process(std::vector<float>(static_cast<size_t>(kSampleRate), 0.f)), 60u);
    }
}

TEST(SpectrumAnalyzerTest, SineReadsItsAmplitudeInItsBand)
{
    for (const size_t size : {2048u, 4096u, 8192u, 16384u})
    {
        for (const float frequency : {55.f, 440.f, 1000.f, 1234.5f, 5000.f, 12345.f})
        {
            if (frequency < 2.f * kSampleRate / static_cast<float>(size))
            {
                continue;
            }
            SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
            analyzer.setFftSize(size);
            analyzer.setFallDbPerSecond(std::numeric_limits<float>::infinity());
            std::ignore = analyzer.process(sine(0.5f, frequency, 0.5f));
            const float level = analyzer.bandsDb()[bandOf(analyzer, frequency)];
            EXPECT_NEAR(level, -6.0206f, 0.5f) << "fft " << size << " f " << frequency;
            EXPECT_NEAR(maxBandDb(analyzer), -6.0206f, 0.35f) << "fft " << size << " f " << frequency;
        }
    }
}

TEST(SpectrumAnalyzerTest, NoBandOvershootsNextToAHannNull)
{
    for (const size_t bin : {3u, 4u, 7u})
    {
        SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
        analyzer.setFftSize(2048);
        analyzer.setFallDbPerSecond(std::numeric_limits<float>::infinity());
        std::ignore = analyzer.process(sine(0.5f, static_cast<float>(bin) * kSampleRate / 2048.f, 0.5f));
        EXPECT_LT(maxBandDb(analyzer), -6.0206f + 0.05f) << "bin " << bin;
    }
}

TEST(SpectrumAnalyzerTest, BinCentredSineIsExact)
{
    SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
    analyzer.setFftSize(4096);
    analyzer.setFallDbPerSecond(std::numeric_limits<float>::infinity());
    const float frequency = 100.f * kSampleRate / 4096.f;
    std::ignore = analyzer.process(sine(0.25f, frequency, 0.5f));
    EXPECT_NEAR(analyzer.bandsDb()[bandOf(analyzer, frequency)], -12.0412f, 0.05f);
}

TEST(SpectrumAnalyzerTest, LevelFallsAtTheConfiguredRate)
{
    SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
    analyzer.setFftSize(2048);
    analyzer.setFallDbPerSecond(24.f);
    std::ignore = analyzer.process(sine(1.f, 1000.f, 0.5f));
    const size_t band = bandOf(analyzer, 1000.f);

    std::ignore = analyzer.process(std::vector<float>(static_cast<size_t>(0.1f * kSampleRate), 0.f));
    const float before = analyzer.bandsDb()[band];
    std::ignore = analyzer.process(std::vector<float>(static_cast<size_t>(0.5f * kSampleRate), 0.f));
    EXPECT_NEAR(before - analyzer.bandsDb()[band], 12.f, 0.01f);
}

TEST(SpectrumAnalyzerTest, InstantFallDropsToFloorOnceWindowIsClear)
{
    SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
    analyzer.setFftSize(2048);
    analyzer.setFallDbPerSecond(std::numeric_limits<float>::infinity());
    std::ignore = analyzer.process(sine(1.f, 1000.f, 0.5f));
    std::ignore = analyzer.process(std::vector<float>(4096, 0.f));
    EXPECT_FLOAT_EQ(maxBandDb(analyzer), SpectrumAnalyzer::kFloorDb);
}

TEST(SpectrumAnalyzerTest, PeakHoldKeepsMaximumUntilReset)
{
    SpectrumAnalyzer analyzer(kSampleRate, kNumBands);
    analyzer.setFftSize(2048);
    analyzer.setFallDbPerSecond(std::numeric_limits<float>::infinity());
    std::ignore = analyzer.process(sine(0.5f, 1000.f, 0.5f));
    std::ignore = analyzer.process(std::vector<float>(4096, 0.f));
    const size_t band = bandOf(analyzer, 1000.f);
    EXPECT_NEAR(analyzer.peakHoldDb()[band], -6.0206f, 0.5f);

    analyzer.resetPeakHold();
    EXPECT_FLOAT_EQ(analyzer.peakHoldDb()[band], SpectrumAnalyzer::kFloorDb);
}

}
