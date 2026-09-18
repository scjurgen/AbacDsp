#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/OnePoleHp.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(OnePoleHpNodeTest, BlocksDcAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    OnePoleHp filter{kSampleRate};

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 5000; ++i)
    {
        filter.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 0.f, 1e-2f);
}

TEST(OnePoleHpNodeTest, FirstSampleOfAStepPassesThroughAlmostUnchanged)
{
    constexpr float kSampleRate = 48000.f;
    OnePoleHp filter{kSampleRate};
    filter.setParameter(0, 1000.f);

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    filter.process(ins, outs, 1);

    EXPECT_GT(out, 0.5f);
}

}
