#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Map.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MapNodeTest, DefaultIsIdentity)
{
    Map map;
    const std::array<float, 3> in{0.f, 0.5f, 1.f};
    std::array<float, 3> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    map.process(ins, outs, in.size());

    EXPECT_EQ(out, in);
}

TEST(MapNodeTest, RemapsZeroToOneOntoCustomOutputRange)
{
    Map map;
    map.setParameter(2, 100.0f); // outMin
    map.setParameter(3, 200.0f); // outMax
    const std::array<float, 3> in{0.f, 0.5f, 1.f};
    std::array<float, 3> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    map.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], 100.f);
    EXPECT_FLOAT_EQ(out[1], 150.f);
    EXPECT_FLOAT_EQ(out[2], 200.f);
}

}
