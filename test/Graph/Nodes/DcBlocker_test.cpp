#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Graph/Nodes/DcBlocker.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(DcBlockerNodeTest, BlocksDcAfterSettling)
{
    constexpr float kSampleRate = 48000.f;
    DcBlocker blocker{kSampleRate};

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 20000; ++i)
    {
        blocker.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 0.f, 1e-2f);
}

TEST(DcBlockerNodeTest, FirstSampleIsFinite)
{
    constexpr float kSampleRate = 48000.f;
    DcBlocker blocker{kSampleRate};

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    blocker.process(ins, outs, 1);

    EXPECT_TRUE(std::isfinite(out));
}

}
