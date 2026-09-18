#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Graph/Nodes/ShelfEQ.h"

namespace AbacDsp::Graph::Nodes::Test
{
namespace
{

[[nodiscard]] float rmsOfSineThroughFilter(ShelfEQ& shelf, const float frequencyHz, const float sampleRate)
{
    constexpr int kNumSamples = 4000;
    float sumSquares = 0.f;
    constexpr int kMeasureFrom = 2000; // skip the filter's settling transient
    for (int i = 0; i < kNumSamples; ++i)
    {
        const float in = std::sin(2.f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / sampleRate);
        float out = 0.f;
        std::array<const float*, 1> ins{&in};
        std::array<float*, 1> outs{&out};
        shelf.process(ins, outs, 1);
        if (i >= kMeasureFrom)
        {
            sumSquares += out * out;
        }
    }
    return std::sqrt(sumSquares / static_cast<float>(kNumSamples - kMeasureFrom));
}

} // namespace

TEST(ShelfEQNodeTest, ZeroGainLeavesLowFrequencyContentUnchanged)
{
    constexpr float kSampleRate = 48000.f;
    ShelfEQ shelf{kSampleRate};

    const float rms = rmsOfSineThroughFilter(shelf, 100.f, kSampleRate);

    EXPECT_NEAR(rms, std::numbers::sqrt2_v<float> / 2.f, 0.02f); // unit-amplitude sine RMS
}

TEST(ShelfEQNodeTest, PositiveGainBoostsContentAboveTheShelfFrequency)
{
    constexpr float kSampleRate = 48000.f;
    ShelfEQ shelf{kSampleRate};
    shelf.setParameter(0, 2000.f);
    shelf.setParameter(1, 12.f);

    const float rmsBelowShelf = rmsOfSineThroughFilter(shelf, 100.f, kSampleRate);
    ShelfEQ shelfAgain{kSampleRate};
    shelfAgain.setParameter(0, 2000.f);
    shelfAgain.setParameter(1, 12.f);
    const float rmsAboveShelf = rmsOfSineThroughFilter(shelfAgain, 15000.f, kSampleRate);

    EXPECT_GT(rmsAboveShelf, rmsBelowShelf);
}

}
