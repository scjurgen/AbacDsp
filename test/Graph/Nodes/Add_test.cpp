#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Add.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(AddNodeTest, SumsBothInputsSampleWise)
{
    Add add;
    const std::array<float, 3> inA{1.f, 2.f, 3.f};
    const std::array<float, 3> inB{10.f, -2.f, 0.5f};
    std::array<float, 3> out{};
    std::array<const float*, 2> ins{inA.data(), inB.data()};
    std::array<float*, 1> outs{out.data()};

    add.process(ins, outs, inA.size());

    EXPECT_FLOAT_EQ(out[0], 11.f);
    EXPECT_FLOAT_EQ(out[1], 0.f);
    EXPECT_FLOAT_EQ(out[2], 3.5f);
}

}
