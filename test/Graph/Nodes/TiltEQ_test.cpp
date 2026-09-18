#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Graph/Nodes/TiltEQ.h"

namespace AbacDsp::Graph::Nodes::Test
{
namespace
{

[[nodiscard]] float rmsOfSineThroughFilter(TiltEQ& tilt, const float frequencyHz, const float sampleRate)
{
    constexpr int kNumSamples = 4000;
    constexpr int kMeasureFrom = 2000; // skip the filter's settling transient
    float sumSquares = 0.f;
    for (int i = 0; i < kNumSamples; ++i)
    {
        const float in = std::sin(2.f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / sampleRate);
        float out = 0.f;
        std::array<const float*, 1> ins{&in};
        std::array<float*, 1> outs{&out};
        tilt.process(ins, outs, 1);
        if (i >= kMeasureFrom)
        {
            sumSquares += out * out;
        }
    }
    return std::sqrt(sumSquares / static_cast<float>(kNumSamples - kMeasureFrom));
}

} // namespace

TEST(TiltEQNodeTest, ZeroTiltIsFlat)
{
    constexpr float kSampleRate = 48000.f;
    TiltEQ tilt{kSampleRate};

    const float rms = rmsOfSineThroughFilter(tilt, 100.f, kSampleRate);

    EXPECT_NEAR(rms, std::numbers::sqrt2_v<float> / 2.f, 0.02f); // unit-amplitude sine RMS
}

TEST(TiltEQNodeTest, PositiveTiltCutsLowBoostsHigh)
{
    constexpr float kSampleRate = 48000.f;
    TiltEQ low{kSampleRate};
    low.setParameter(0, 12.f);
    const float rmsLow = rmsOfSineThroughFilter(low, 100.f, kSampleRate);

    TiltEQ high{kSampleRate};
    high.setParameter(0, 12.f);
    const float rmsHigh = rmsOfSineThroughFilter(high, 15000.f, kSampleRate);

    EXPECT_LT(rmsLow, std::numbers::sqrt2_v<float> / 2.f);
    EXPECT_GT(rmsHigh, std::numbers::sqrt2_v<float> / 2.f);
}

}
