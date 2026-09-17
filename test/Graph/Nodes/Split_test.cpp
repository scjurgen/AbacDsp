#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Split.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(SplitNodeTest, DuplicatesToBothStereoOutputs)
{
    Split split;
    const std::array<float, 2> inL{1.f, 2.f};
    const std::array<float, 2> inR{3.f, 4.f};
    std::array<float, 2> out1L{};
    std::array<float, 2> out1R{};
    std::array<float, 2> out2L{};
    std::array<float, 2> out2R{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 4> outs{out1L.data(), out1R.data(), out2L.data(), out2R.data()};

    split.process(ins, outs, inL.size());

    EXPECT_EQ(out1L, inL);
    EXPECT_EQ(out1R, inR);
    EXPECT_EQ(out2L, inL);
    EXPECT_EQ(out2R, inR);
}

}
