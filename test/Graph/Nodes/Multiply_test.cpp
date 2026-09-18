#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Multiply.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MultiplyNodeTest, MultipliesBothInputsSampleWise)
{
    Multiply multiply;
    const std::array<float, 3> inA{1.f, 2.f, 3.f};
    const std::array<float, 3> inB{10.f, -2.f, 0.5f};
    std::array<float, 3> out{};
    std::array<const float*, 2> ins{inA.data(), inB.data()};
    std::array<float*, 1> outs{out.data()};

    multiply.process(ins, outs, inA.size());

    EXPECT_FLOAT_EQ(out[0], 10.f);
    EXPECT_FLOAT_EQ(out[1], -4.f);
    EXPECT_FLOAT_EQ(out[2], 1.5f);
}

}
