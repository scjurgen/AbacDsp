#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/MonoToStereo.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MonoToStereoNodeTest, DuplicatesToBothChannels)
{
    MonoToStereo monoToStereo;
    const std::array<float, 2> in{1.f, 2.f};
    std::array<float, 2> outL{};
    std::array<float, 2> outR{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    monoToStereo.process(ins, outs, in.size());

    EXPECT_EQ(outL, in);
    EXPECT_EQ(outR, in);
}

}
