#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/ScaleOffset.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(ScaleOffsetNodeTest, DefaultIsIdentity)
{
    ScaleOffset node;
    const std::array<float, 2> in{1.f, -3.f};
    std::array<float, 2> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    node.process(ins, outs, in.size());

    EXPECT_EQ(out, in);
}

TEST(ScaleOffsetNodeTest, ParametersScaleThenOffset)
{
    ScaleOffset node;
    node.setParameter(0, 2.0f);
    node.setParameter(1, 3.0f);
    const std::array<float, 2> in{1.f, -3.f};
    std::array<float, 2> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    node.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], 5.f);
    EXPECT_FLOAT_EQ(out[1], -3.f);
}

}
