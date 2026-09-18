#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Compander.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(CompanderNodeTest, DefaultRatioOfOneLeavesSignalUnchanged)
{
    constexpr float kSampleRate = 48000.f;
    Compander compander{kSampleRate};

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 100; ++i)
    {
        compander.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 1.f, 1e-4f);
}

TEST(CompanderNodeTest, LoudSignalAboveThresholdIsAttenuatedAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    Compander compander{kSampleRate};
    compander.setParameter(0, -20.f); // thresholdDb
    compander.setParameter(1, 4.f);   // ratio
    compander.setParameter(2, 1.f);   // attackMs

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < static_cast<int>(kSampleRate * 0.05f); ++i)
    {
        compander.process(ins, outs, 1);
    }

    EXPECT_LT(out, 1.f);
}

}
