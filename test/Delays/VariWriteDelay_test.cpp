#include "Delays/VariSpeedTapeDelay.h"
#include "Filters/Sinc/sinc_4.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

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
}
