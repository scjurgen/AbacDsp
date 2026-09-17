#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/MsDecode.h"
#include "Graph/Nodes/MsEncode.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MsDecodeNodeTest, InvertsMsEncode)
{
    MsEncode msEncode;
    MsDecode msDecode;

    const std::array<float, 2> inL{1.f, -0.3f};
    const std::array<float, 2> inR{0.f, 0.7f};
    std::array<float, 2> mid{};
    std::array<float, 2> side{};
    std::array<const float*, 2> encodeIns{inL.data(), inR.data()};
    std::array<float*, 2> encodeOuts{mid.data(), side.data()};
    msEncode.process(encodeIns, encodeOuts, inL.size());

    std::array<float, 2> outL{};
    std::array<float, 2> outR{};
    std::array<const float*, 2> decodeIns{mid.data(), side.data()};
    std::array<float*, 2> decodeOuts{outL.data(), outR.data()};
    msDecode.process(decodeIns, decodeOuts, inL.size());

    EXPECT_NEAR(outL[0], inL[0], 1e-6f);
    EXPECT_NEAR(outR[0], inR[0], 1e-6f);
    EXPECT_NEAR(outL[1], inL[1], 1e-6f);
    EXPECT_NEAR(outR[1], inR[1], 1e-6f);
}

}
