#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/BandPass.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(BandPassNodeTest, BlocksDcAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    BandPass filter{kSampleRate};
    filter.setParameter(0, 1000.f);
    filter.setParameter(1, 1.0f);

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

}
