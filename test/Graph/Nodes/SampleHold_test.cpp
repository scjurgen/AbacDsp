#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/SampleHold.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(SampleHoldNodeTest, HoldsAtEachRisingEdgeWithinOneBlock)
{
    SampleHold node;
    // Two rising edges inside a single block: samples 1 and 4.
    const std::array<float, 6> in{10.f, 20.f, 20.f, 20.f, 30.f, 30.f};
    const std::array<float, 6> trigger{0.f, 1.f, 1.f, 0.f, 1.f, 1.f};
    std::array<float, 6> out{};
    std::array<const float*, 2> ins{in.data(), trigger.data()};
    std::array<float*, 1> outs{out.data()};

    node.process(ins, outs, in.size());

    EXPECT_FLOAT_EQ(out[0], 0.f);  // no edge yet
    EXPECT_FLOAT_EQ(out[1], 20.f); // rising edge, holds in[1]
    EXPECT_FLOAT_EQ(out[2], 20.f); // trigger still high, no new edge
    EXPECT_FLOAT_EQ(out[3], 20.f); // trigger low, held value unchanged
    EXPECT_FLOAT_EQ(out[4], 30.f); // second rising edge, holds in[4]
    EXPECT_FLOAT_EQ(out[5], 30.f);
}

TEST(SampleHoldNodeTest, StateSurvivesAcrossBlocks)
{
    SampleHold node;
    const std::array<float, 2> in1{5.f, 5.f};
    const std::array<float, 2> trigger1{1.f, 0.f};
    std::array<float, 2> out1{};
    std::array<const float*, 2> ins1{in1.data(), trigger1.data()};
    std::array<float*, 1> outs1{out1.data()};
    node.process(ins1, outs1, in1.size());
    EXPECT_FLOAT_EQ(out1[1], 5.f);

    const std::array<float, 1> in2{99.f};
    const std::array<float, 1> trigger2{0.f}; // no new edge
    std::array<float, 1> out2{};
    std::array<const float*, 2> ins2{in2.data(), trigger2.data()};
    std::array<float*, 1> outs2{out2.data()};
    node.process(ins2, outs2, in2.size());

    EXPECT_FLOAT_EQ(out2[0], 5.f);
}

}
