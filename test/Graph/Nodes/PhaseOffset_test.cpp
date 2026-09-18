#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/PhaseOffset.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(PhaseOffsetNodeTest, WrapsPastOneBackToZero)
{
    PhaseOffset node;
    node.setParameter(0, 0.3f);
    const std::array<float, 1> in{0.8f};
    std::array<float, 1> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    node.process(ins, outs, in.size());

    EXPECT_NEAR(out[0], 0.1f, 1e-6f);
}

TEST(PhaseOffsetNodeTest, NegativeOffsetWrapsBelowZeroBackToOne)
{
    PhaseOffset node;
    node.setParameter(0, -0.3f);
    const std::array<float, 1> in{0.1f};
    std::array<float, 1> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    node.process(ins, outs, in.size());

    EXPECT_NEAR(out[0], 0.8f, 1e-6f);
}

}
