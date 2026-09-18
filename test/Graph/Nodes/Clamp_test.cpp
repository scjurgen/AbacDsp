#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Clamp.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(ClampNodeTest, DefaultRangeIsZeroToOne)
{
    Clamp clamp;
    const std::array<float, 3> in{-1.f, 0.5f, 2.f};
    std::array<float, 3> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    clamp.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], 0.f);
    EXPECT_FLOAT_EQ(out[1], 0.5f);
    EXPECT_FLOAT_EQ(out[2], 1.f);
}

TEST(ClampNodeTest, ParametersSetCustomRange)
{
    Clamp clamp;
    clamp.setParameter(0, -5.0f);
    clamp.setParameter(1, 5.0f);
    const std::array<float, 2> in{-10.f, 10.f};
    std::array<float, 2> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    clamp.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], -5.f);
    EXPECT_FLOAT_EQ(out[1], 5.f);
}

}
