#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "Delays/OrganicChorusTransport.h"
#include "Filters/Sinc/sinc_4.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"

namespace AbacDsp::Test
{

constexpr size_t TileSize{8};

struct DelayTestParams
{
    float ratio;
    float expectedPeakIndex;
    size_t expectedSupportWidth;
    std::vector<size_t> expectedDominantIndices;
};

class OrganicChorusTransportTestFixture : public ::testing::TestWithParam<DelayTestParams>
{
  protected:
    struct PeakAnalysis
    {
        size_t peakIndex{};
        float peakValue{};
        size_t supportBegin{};
        size_t supportEnd{};
        size_t supportWidth{};
        std::vector<size_t> dominantIndices;
        float energyWeightedCenter{};
        float energy{};
    };

    OrganicChorusTransport<10000, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};

    void SetUp() override
    {
        // Below the default (1000) so setReadHead()'s read-reconstruction-delay compensation
        // isn't clamped away - the test's own target distances land exactly as requested.
        sut.setReadHeadSafetyMargin(50.f);
        sut.setFlutterDepth(0.f);
        sut.setFlutterRate(1.f);
        sut.setRatio(1.f);
        sut.setReadHead(0, 48000, true);
        sut.setWowDepth(0.f);
        sut.setWowDrift(0.f);
        sut.setWowRate(1.f);
        sut.setWowVariance(0.f);
    }

    void settle(const size_t samples)
    {
        for (size_t i = 0; i <= samples / TileSize; ++i)
        {
            std::array<float, TileSize> s{};
            sut.feed(s);
        }
    }

    std::vector<float> feedAndCollect(const size_t numSamples)
    {
        const size_t numBlocks = 1 + (numSamples - 1) / TileSize;
        std::vector<float> out;
        out.reserve(numBlocks * TileSize);

        std::array<float, TileSize> data{};
        static_assert(data.size() >= 2);
        data[0] = 1.f;
        data[1] = 1.f;
        sut.feed(data);

        std::array<float, TileSize> result{};
        sut.readBlock(0, result);
        out.insert(out.end(), result.begin(), result.end());

        for (size_t i = 0; i < numBlocks - 1; ++i)
        {
            std::array<float, TileSize> empty{};
            sut.feed(empty);
            sut.readBlock(0, result);
            out.insert(out.end(), result.begin(), result.end());
        }

        return out;
    }

    static PeakAnalysis analysePeak(const std::vector<float>& data, const float supportThreshold = 0.00001f,
                                    const float dominantThreshold = 0.3f)
    {
        EXPECT_FALSE(data.empty());

        const auto peakIt =
            std::ranges::max_element(data, [](const float a, const float b) { return std::abs(a) < std::abs(b); });

        const auto peakIndex = static_cast<size_t>(std::distance(data.begin(), peakIt));
        const auto peakValue = std::abs(*peakIt);

        size_t supportBegin = peakIndex;
        while (supportBegin > 0 && std::abs(data[supportBegin - 1]) >= peakValue * supportThreshold)
        {
            --supportBegin;
        }

        size_t supportEnd = peakIndex;
        while (supportEnd + 1 < data.size() && std::abs(data[supportEnd + 1]) >= peakValue * supportThreshold)
        {
            ++supportEnd;
        }

        std::vector<size_t> dominantIndices;
        for (size_t i = supportBegin; i <= supportEnd; ++i)
        {
            if (std::abs(data[i]) >= peakValue * dominantThreshold)
            {
                dominantIndices.push_back(i);
            }
        }

        float weightedSum = 0.f;
        float integral = 0.f;
        for (size_t i = supportBegin; i <= supportEnd; ++i)
        {
            weightedSum += static_cast<float>(i) * data[i];
            integral += data[i];
        }

        PeakAnalysis result;
        result.peakIndex = peakIndex;
        result.peakValue = peakValue;
        result.supportBegin = supportBegin;
        result.supportEnd = supportEnd;
        result.supportWidth = supportEnd - supportBegin + 1;
        result.dominantIndices = std::move(dominantIndices);
        result.energyWeightedCenter = integral > 0.f ? weightedSum / integral : static_cast<float>(peakIndex);
        result.energy = integral;
        return result;
    }
};


TEST_P(OrganicChorusTransportTestFixture, DelayedAround1000Samples)
{
    const auto& p = GetParam();

    sut.setReadHead(0, 1000, true);
    sut.setRatio(p.ratio);
    settle(100000);

    const auto out = feedAndCollect(4096);
    const auto peak = analysePeak(out);

    // Saturating: peakIndex < supportWidth would otherwise underflow this size_t subtraction.
    const auto preSupportEnd = peak.peakIndex > peak.supportWidth ? peak.peakIndex - peak.supportWidth : 0;
    for (size_t i = 0; i < preSupportEnd; ++i)
    {
        EXPECT_EQ(out[i], 0.f) << "failed at " << i;
    }

    for (size_t i = peak.peakIndex + peak.supportWidth + 1; i < out.size(); ++i)
    {
        EXPECT_EQ(out[i], 0.f) << "failed at " << i;
    }
    EXPECT_NEAR(peak.energyWeightedCenter, p.expectedPeakIndex, 0.5f);
    EXPECT_NEAR(peak.energy, 2.f, 0.1f);
    EXPECT_EQ(peak.supportWidth, p.expectedSupportWidth);

    for (const auto idx : p.expectedDominantIndices)
    {
        EXPECT_THAT(peak.dominantIndices, ::testing::Contains(idx));
    }
}

// Re-measured for the SrPullConverter-based read reconstruction: peak positions land close
// to their old (Catmull-Rom-era) values given compensation headroom; support width is
// genuinely wider now - the sinc kernel's own footprint, not a bug.
INSTANTIATE_TEST_SUITE_P(OrganicChorusTransportSpeedVariants, OrganicChorusTransportTestFixture,
                         ::testing::Values(
                             //             ratio   peakIdx   suppW  dominantIndices
                             DelayTestParams{0.25f, 4023.71f, 120, {4020, 4021, 4022, 4023, 4024, 4025, 4026, 4027}},
                             DelayTestParams{0.5f, 2005.63f, 56, {2004, 2005, 2006, 2007}},
                             DelayTestParams{0.9438743127f, 1059.16f, 31, {1058, 1059, 1060}},
                             DelayTestParams{1.f, 999.63f, 36, {999, 1000}},
                             DelayTestParams{1.05946309436f, 937.4f, 35, {937, 938}},
                             DelayTestParams{2.f, 505.52f, 35, {505, 506}},
                             DelayTestParams{4.f, 258.45f, 34, {258, 259}}),
                         [](const ::testing::TestParamInfo<DelayTestParams>& info) -> std::string
                         {
                             auto s = std::to_string(info.param.ratio);
                             std::ranges::replace(s, '.', '_');
                             return "ratio_" + s;
                         });

// The resampler has its own startup transient (see the fixture's settle() above), so
// writeHead() isn't exactly TileSize per feed() from a cold start - only once settled.
TEST(OrganicChorusTransportWriteTest, WriteHeadAdvancesByTileSizePerFeedOnceSettled)
{
    constexpr size_t kSmallBufferSize = 500;
    OrganicChorusTransport<kSmallBufferSize, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};
    sut.setRatio(1.f, true);

    std::array<float, TileSize> block{};
    for (int i = 0; i < 50; ++i)
    {
        sut.feed(block);
    }
    const auto before = sut.writeHead();
    sut.feed(block);
    EXPECT_EQ(sut.writeHead(), (before + TileSize) % kSmallBufferSize);
}

// The whole point of this fork: Wow/Flutter must never reach feed(), so the write head's
// per-tile advance is identical whether they are silent or driven hard.
TEST(OrganicChorusTransportWriteTest, WowFlutterNeverAffectsWriteHeadAdvance)
{
    constexpr size_t kSmallBufferSize = 500;
    OrganicChorusTransport<kSmallBufferSize, 1, 1, TileSize> quiet{48000.f, std::make_shared<SincFilter>(sinc4)};
    OrganicChorusTransport<kSmallBufferSize, 1, 1, TileSize> driven{48000.f, std::make_shared<SincFilter>(sinc4)};
    quiet.setRatio(1.f, true);
    driven.setRatio(1.f, true);
    driven.setWowRate(0.5f);
    driven.setWowDepth(0.9f);
    driven.setWowVariance(0.3f);
    driven.setWowDrift(0.4f);
    driven.setFlutterRate(0.5f);
    driven.setFlutterDepth(0.5f);

    std::array<float, TileSize> block{};
    for (int i = 0; i < 200; ++i)
    {
        quiet.feed(block);
        driven.feed(block);
        quiet.readBlock(0, block);
        driven.readBlock(0, block);
        EXPECT_EQ(quiet.writeHead(), driven.writeHead()) << "diverged at feed " << i;
    }
}

// Mirrors OrganicChorusVoiceSafetyMarginMeasurement in OrganicChorusVoice_test.cpp: the
// write/read distance should sit on target with Wow/Flutter off, and wander once they're
// driven - proving the read head, not the write clock, now carries the modulation.
TEST(OrganicChorusTransportWriteTest, ReadHeadWobblesWithWowFlutterWhileWriteStaysClean)
{
    constexpr size_t kBufferSize = 20000;
    constexpr float kTargetDistance{kBufferSize / 2.f};

    const auto measurePeakDrift = [](OrganicChorusTransport<kBufferSize, 1, 1, TileSize>& sut)
    {
        sut.setReadHeadSafetyMargin(8.f);
        sut.setReadHead(0, kTargetDistance, true);
        sut.setReadHeadCorrectionThreshold(0, kTargetDistance);

        float peak = 0.f;
        const std::array<float, TileSize> silence{};
        for (size_t b = 0; b < 5000; ++b)
        {
            sut.feed(silence);
            std::array<float, TileSize> discard{};
            sut.readBlock(0, discard);

            auto delta = static_cast<double>(sut.writeHead()) - sut.readHead(0);
            while (delta < 0.0)
            {
                delta += kBufferSize;
            }
            while (delta >= kBufferSize)
            {
                delta -= kBufferSize;
            }
            peak = std::max(peak, static_cast<float>(std::abs(delta - kTargetDistance)));
        }
        return peak;
    };

    // Even at ratio 1 with Wow/Flutter off, a small bounded residual remains - empirically ~69
    // samples here (the read reconstruction's own chunked generation is coarser-grained than
    // the old per-sample Catmull-Rom was); the bound below has headroom either way.
    OrganicChorusTransport<kBufferSize, 1, 1, TileSize> quiet{48000.f, std::make_shared<SincFilter>(sinc4)};
    quiet.setWowDepth(0.f);
    quiet.setFlutterDepth(0.f);
    EXPECT_LT(measurePeakDrift(quiet), 100.f);

    OrganicChorusTransport<kBufferSize, 1, 1, TileSize> driven{48000.f, std::make_shared<SincFilter>(sinc4)};
    driven.setWowRate(0.4f);
    driven.setWowDepth(0.7f);
    driven.setWowVariance(0.3f);
    driven.setWowDrift(0.3f);
    driven.setFlutterRate(0.4f);
    driven.setFlutterDepth(0.3f);
    EXPECT_GT(measurePeakDrift(driven), 100.f);
}

// setReadHead(..., true) resolves to m_idealWritePosition - clampedDistance, wrapped into
// [0, BufferSize) - so on a freshly constructed instance (write position still 0) the
// resulting read position directly reveals what the clamp margin actually did.
TEST(OrganicChorusTransportSafetyMarginTest, DefaultMarginMatchesLegacyFixedConstant)
{
    constexpr size_t kBufferSize = 20000;
    OrganicChorusTransport<kBufferSize, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};
    sut.setReadHead(0, 500.f, true);
    EXPECT_NEAR(sut.readHead(0), kBufferSize - 1000.0, 1E-9);
}

// setReadHead() now compensates for the read reconstruction's own added delay (see
// m_readReconstructionDelay: kernel half-width/increment + kLowRateChunkFrames, 25.765625 for
// sinc4/chunk16) - a caller's requested distance is where the signal actually arrives, not
// where the read head literally sits, so the two now differ by that constant.
constexpr double kReadReconstructionDelay = 25.765625;

TEST(OrganicChorusTransportSafetyMarginTest, SmallerMarginPermitsShortDelay)
{
    constexpr size_t kBufferSize = 20000;
    OrganicChorusTransport<kBufferSize, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};
    sut.setReadHeadSafetyMargin(50.f);
    sut.setReadHead(0, 500.f, true);
    EXPECT_NEAR(sut.readHead(0), kBufferSize - 500.0 + kReadReconstructionDelay, 1E-9);
}

TEST(OrganicChorusTransportSafetyMarginTest, MarginIsClampedToInterpolatorFloorAndBufferCeiling)
{
    constexpr size_t kBufferSize = 20000;
    OrganicChorusTransport<kBufferSize, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};

    // Floor is now max(2, kReadReconstructionDelay) - the kernel's own structural minimum,
    // not the old fixed 8 (sized for Catmull-Rom's much narrower footprint).
    sut.setReadHeadSafetyMargin(2.f);
    sut.setReadHead(0, 3.f, true);
    EXPECT_NEAR(sut.readHead(0), kBufferSize - kReadReconstructionDelay, 1E-9);

    sut.setReadHeadSafetyMargin(999999.f);
    sut.setReadHead(0, 9000.f, true);
    EXPECT_NEAR(sut.readHead(0), kBufferSize - kBufferSize / 2.0, 1E-9);
}

// readBlock() used to track m_ratio.getLastValue(), which omits setExternalRatioPerturbation()
// - under sustained drift that caused a hard catch-up glide once the real write rate diverged
// far enough. Fixed by tracking the exact feed() ratio; this pins the regression.
TEST(OrganicChorusTransportDriftTrackingTest, ReadHeadVelocityStaysBoundedUnderSustainedDrift)
{
    constexpr size_t kBufferSize = 8192;
    constexpr float sigma = 0.2f * 0.06f; // Drift=0.2 on Tri Ensemble's speedDriftMaxSigma

    OrganicChorusTransport<kBufferSize, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};
    sut.setRatio(1.f, true);
    sut.setReadHeadSafetyMargin(280.f);
    sut.setReadHeadCorrectionThreshold(0, 220.f);
    sut.setReadHead(0, 672.f, true);
    sut.setWowRate(3.3f);
    sut.setWowDepth(0.45f);
    sut.setFlutterRate(3.3f);
    sut.setFlutterDepth(0.225f);

    OrnsteinUhlenbeckProcess speedDrift(48000.f / static_cast<float>(TileSize));
    speedDrift.setSigma(sigma);

    // Per-tile velocity is no longer a meaningful signal: readHead() now advances in bursts (a
    // whole low-rate chunk generated every few tiles, see kLowRateChunkFrames), not smoothly
    // every tile. Averaging over a window spanning several chunks smooths that by-design
    // burstiness out while staying sensitive to a genuine sustained lurch.
    constexpr int kVelocityWindowTiles = 50;
    double windowStartReadHead = sut.readHead(0);
    float worstDeviation = 0.f;
    constexpr int numBlocks = static_cast<int>(60.0 * 48000.0) / static_cast<int>(TileSize);
    for (int b = 0; b < numBlocks; ++b)
    {
        sut.setExternalRatioPerturbation(speedDrift.step() - sigma);
        std::array<float, TileSize> silence{};
        sut.feed(silence);
        std::array<float, TileSize> discard{};
        sut.readBlock(0, discard);

        if ((b + 1) % kVelocityWindowTiles != 0)
        {
            continue;
        }
        const double readHead = sut.readHead(0);
        auto delta = readHead - windowStartReadHead;
        if (delta < -static_cast<double>(kBufferSize) / 2.0)
        {
            delta += static_cast<double>(kBufferSize);
        }
        if (delta > static_cast<double>(kBufferSize) / 2.0)
        {
            delta -= static_cast<double>(kBufferSize);
        }
        windowStartReadHead = readHead;

        const auto velocity = static_cast<float>(delta) / static_cast<float>(kVelocityWindowTiles * TileSize);
        worstDeviation = std::max(worstDeviation, std::abs(velocity - 1.f));
    }

    // Measured ~1.2%; a correction-glide lurch (the bug) drops velocity to ~8-13% of
    // nominal - 5% has headroom on the fix, none on the bug.
    EXPECT_LT(worstDeviation, 0.05f);
}

}
