#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Curve.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(CurveNodeTest, DefaultExponentIsIdentity)
{
    Curve curve;
    const std::array<float, 3> in{0.f, 0.5f, 1.f};
    std::array<float, 3> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    curve.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], 0.f);
    EXPECT_FLOAT_EQ(out[1], 0.5f);
    EXPECT_FLOAT_EQ(out[2], 1.f);
}

TEST(CurveNodeTest, ExponentTwoSquaresAndClampsOutOfRangeInput)
{
    Curve curve;
    curve.setParameter(0, 2.0f);
    const std::array<float, 2> in{0.5f, 2.f}; // 2.f clamps to 1.f before pow()
    std::array<float, 2> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    curve.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], 0.25f);
    EXPECT_FLOAT_EQ(out[1], 1.f);
}

}
