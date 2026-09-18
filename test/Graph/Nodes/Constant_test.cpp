#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Constant.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(ConstantNodeTest, FillsOutputWithConstructedValue)
{
    Constant constant{-2.5f};
    std::array<float, 3> out{};
    std::array<float*, 1> outs{out.data()};

    constant.process({}, outs, out.size());

    for (const float sample : out)
    {
        EXPECT_FLOAT_EQ(sample, -2.5f);
    }
}

TEST(ConstantNodeTest, SetParameterHasNoEffect)
{
    Constant constant{1.0f};
    constant.setParameter(0, 99.0f);
    std::array<float, 1> out{};
    std::array<float*, 1> outs{out.data()};

    constant.process({}, outs, out.size());

    EXPECT_FLOAT_EQ(out[0], 1.0f);
}

}
