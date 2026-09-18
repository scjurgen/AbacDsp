#include <algorithm>
#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Graph/Nodes/Lfo.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(LfoNodeTest, WaveformFromConfigParsesKnownNamesAndDefaultsToSine)
{
    EXPECT_EQ(lfoWaveformFromConfig("sine"), AbacDsp::LfoType::Sine);
    EXPECT_EQ(lfoWaveformFromConfig("triangle"), AbacDsp::LfoType::Triangle);
    EXPECT_EQ(lfoWaveformFromConfig("saw"), AbacDsp::LfoType::Saw);
    EXPECT_EQ(lfoWaveformFromConfig("square"), AbacDsp::LfoType::Square);
    EXPECT_EQ(lfoWaveformFromConfig("noise"), AbacDsp::LfoType::Noise);
    EXPECT_EQ(lfoWaveformFromConfig("unknown"), AbacDsp::LfoType::Sine);
}

TEST(LfoNodeTest, OutputStaysBoundedAndVariesOverABlock)
{
    constexpr float kSampleRate = 48000.f;
    Lfo lfo{kSampleRate, AbacDsp::LfoType::Sine};
    lfo.setParameter(0, 4.0f); // 4 Hz

    std::array<float, 512> out{};
    std::array<float*, 1> outs{out.data()};
    lfo.process({}, outs, out.size());

    for (const float sample : out)
    {
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 1.01f);
    }
    EXPECT_NE(*std::min_element(out.begin(), out.end()), *std::max_element(out.begin(), out.end()));
}

}
