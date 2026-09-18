#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/OnePoleLp.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(OnePoleLpNodeTest, PassesDcAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    OnePoleLp filter{kSampleRate};

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 5000; ++i)
    {
        filter.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 1.f, 1e-3f);
}

TEST(OnePoleLpNodeTest, HigherCutoffSettlesFaster)
{
    constexpr float kSampleRate = 48000.f;
    OnePoleLp slow{kSampleRate};
    slow.setParameter(0, 100.f);
    OnePoleLp fast{kSampleRate};
    fast.setParameter(0, 5000.f);

    const float in = 1.f;
    float outSlow = 0.f;
    float outFast = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outsSlow{&outSlow};
    std::array<float*, 1> outsFast{&outFast};
    for (int i = 0; i < 50; ++i)
    {
        slow.process(ins, outsSlow, 1);
        fast.process(ins, outsFast, 1);
    }

    EXPECT_GT(outFast, outSlow);
}

}
