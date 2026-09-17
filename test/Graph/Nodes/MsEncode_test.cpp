#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/MsEncode.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MsEncodeNodeTest, LeftOnlyEncodesToEqualMidAndSide)
{
    MsEncode msEncode;
    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{0.f};
    std::array<float, 1> outM{};
    std::array<float, 1> outS{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outM.data(), outS.data()};

    msEncode.process(ins, outs, 1);

    EXPECT_FLOAT_EQ(outM[0], 0.5f);
    EXPECT_FLOAT_EQ(outS[0], 0.5f);
}

}
