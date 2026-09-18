#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Graph/Nodes/ExpMap.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(ExpMapNodeTest, EndpointsMatchOutMinAndOutMax)
{
    ExpMap map;
    map.setParameter(2, 100.0f); // outMin
    map.setParameter(3, 400.0f); // outMax
    const std::array<float, 3> in{0.f, 0.5f, 1.f};
    std::array<float, 3> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    map.process(ins, outs, in.size());

    EXPECT_NEAR(out[0], 100.f, 1e-3f);
    EXPECT_NEAR(out[1], std::sqrt(100.f * 400.f), 1e-2f); // geometric mean at the midpoint
    EXPECT_NEAR(out[2], 400.f, 1e-2f);
}

}
