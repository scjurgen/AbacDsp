#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Mixer.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MixerNodeTest, SumsBothStereoInputs)
{
    Mixer mixer;
    const std::array<float, 2> in1L{1.f, 2.f};
    const std::array<float, 2> in1R{3.f, 4.f};
    const std::array<float, 2> in2L{10.f, 20.f};
    const std::array<float, 2> in2R{30.f, 40.f};
    std::array<float, 2> outL{};
    std::array<float, 2> outR{};
    std::array<const float*, 4> ins{in1L.data(), in1R.data(), in2L.data(), in2R.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    mixer.process(ins, outs, in1L.size());

    EXPECT_FLOAT_EQ(outL[0], 11.f);
    EXPECT_FLOAT_EQ(outL[1], 22.f);
    EXPECT_FLOAT_EQ(outR[0], 33.f);
    EXPECT_FLOAT_EQ(outR[1], 44.f);
}

}
