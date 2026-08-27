#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "Delays/VariSpeedTapeDelay.h"
#include "Filters/Sinc/sinc_4.h"

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

class VariWriteDelayTestFixture : public ::testing::TestWithParam<DelayTestParams>
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

    VariSpeedTapeDelay<10000, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};

    void SetUp() override
    {
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


TEST_P(VariWriteDelayTestFixture, DelayedAround1000Samples)
{
    const auto& p = GetParam();

    sut.setReadHead(0, 1000, true);
    sut.setRatio(p.ratio);
    settle(100000);

    const auto out = feedAndCollect(4096);
    const auto peak = analysePeak(out);

    for (size_t i = 0; i < peak.peakIndex - peak.supportWidth; ++i)
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

INSTANTIATE_TEST_SUITE_P(VariWriteDelayTestSpeedVariants, VariWriteDelayTestFixture,
                         ::testing::Values(
                             //             ratio   peakIdx   energy  suppW  dominantIndices
                             DelayTestParams{0.25f, 4026.0f, 91, {4023, 4024, 4025, 4026, 4027, 4028, 4029, 4030}},
                             DelayTestParams{0.5f, 2010.35f, 45, {2009, 2010, 2011, 2012}},
                             DelayTestParams{0.9438743127f, 1060.4f, 24, {1060, 1061}},
                             DelayTestParams{1.f, 1001.5f, 22, {1001, 1002}},
                             DelayTestParams{1.05946309436f, 944.7f, 23, {944, 945}},
                             DelayTestParams{2.f, 506.7f, 21, {506, 507}},
                             DelayTestParams{4.f, 258.45f, 21, {258, 259}}),
                         [](const ::testing::TestParamInfo<DelayTestParams>& info) -> std::string
                         {
                             auto s = std::to_string(info.param.ratio);
                             std::ranges::replace(s, '.', '_');
                             return "ratio_" + s;
                         });

// extractLoop()/loadLoop() support tapelooper's saved-loop feature: pulling the currently
// looping audio out of the ring buffer, and installing previously saved audio back into it.
TEST(VariSpeedTapeDelayLoopIoTest, ExtractLoopRoundTripsThroughLoadLoopAcrossWraparound)
{
    constexpr size_t kSmallBufferSize = 64;
    constexpr size_t kLoopFrames = 30;
    VariSpeedTapeDelay<kSmallBufferSize, 1, 1, TileSize> src{48000.f, std::make_shared<SincFilter>(sinc4)};
    src.setRatio(1.f, true);
    src.setFlutterDepth(0.f);
    src.setWowDepth(0.f);

    // Feed well past kSmallBufferSize so the write head has wrapped more than once,
    // exercising extractLoop()'s wraparound arithmetic rather than a fresh, unwrapped buffer.
    for (int tile = 0; tile < 20; ++tile)
    {
        std::array<float, TileSize> block{};
        for (size_t s = 0; s < TileSize; ++s)
        {
            block[s] = static_cast<float>(tile * static_cast<int>(TileSize) + static_cast<int>(s));
        }
        src.feed(block);
    }

    std::vector<float> extracted(kLoopFrames);
    src.extractLoop(kLoopFrames, extracted);

    VariSpeedTapeDelay<kSmallBufferSize, 1, 1, TileSize> dst{48000.f, std::make_shared<SincFilter>(sinc4)};
    dst.loadLoop(extracted, kLoopFrames);
    std::vector<float> reExtracted(kLoopFrames);
    dst.extractLoop(kLoopFrames, reExtracted);

    EXPECT_EQ(extracted, reExtracted);
}

// The resampler has its own startup transient (see the fixture's settle() above), so
// writeHead() isn't exactly TileSize per feed() from a cold start - only once settled.
TEST(VariSpeedTapeDelayLoopIoTest, WriteHeadAdvancesByTileSizePerFeedOnceSettled)
{
    constexpr size_t kSmallBufferSize = 500;
    VariSpeedTapeDelay<kSmallBufferSize, 1, 1, TileSize> sut{48000.f, std::make_shared<SincFilter>(sinc4)};
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

// tapelooper's chunked save/load path calls installLoopChunk() across several blocks rather
// than loadLoop() in one shot - verify that produces the exact same result loadLoop() does.
TEST(VariSpeedTapeDelayLoopIoTest, ChunkedInstallLoopChunkMatchesSingleShotLoadLoop)
{
    constexpr size_t kSmallBufferSize = 64;
    constexpr size_t kLoopFrames = 30;
    std::vector<float> pattern(kLoopFrames);
    for (size_t i = 0; i < kLoopFrames; ++i)
    {
        pattern[i] = static_cast<float>(i) - 15.f;
    }

    VariSpeedTapeDelay<kSmallBufferSize, 1, 1, TileSize> viaLoadLoop{48000.f, std::make_shared<SincFilter>(sinc4)};
    viaLoadLoop.loadLoop(pattern, kLoopFrames);

    VariSpeedTapeDelay<kSmallBufferSize, 1, 1, TileSize> viaChunks{48000.f, std::make_shared<SincFilter>(sinc4)};
    constexpr size_t kFirstChunk = 12;
    const std::span<const float> patternSpan(pattern);
    viaChunks.installLoopChunk(0, patternSpan.subspan(0, kFirstChunk));
    viaChunks.installLoopChunk(kFirstChunk, patternSpan.subspan(kFirstChunk));
    viaChunks.finishLoopLoad(kLoopFrames);

    std::vector<float> expected(kLoopFrames);
    std::vector<float> actual(kLoopFrames);
    viaLoadLoop.extractLoop(kLoopFrames, expected);
    viaChunks.extractLoop(kLoopFrames, actual);
    EXPECT_EQ(expected, actual);
}

}
