#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/StereoToMono.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(StereoToMonoNodeTest, DefaultIsEqualWeightedSum)
{
    StereoToMono stereoToMono;
    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{3.f};
    std::array<float, 1> out{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 1> outs{out.data()};

    stereoToMono.process(ins, outs, 1);

    EXPECT_FLOAT_EQ(out[0], 2.f);
}

TEST(StereoToMonoNodeTest, ParametersWeightChannels)
{
    StereoToMono stereoToMono;
    stereoToMono.setParameter(0, 1.0f); // gainL
    stereoToMono.setParameter(1, 0.0f); // gainR

    const std::array<float, 1> inL{5.f};
    const std::array<float, 1> inR{100.f};
    std::array<float, 1> out{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 1> outs{out.data()};

    stereoToMono.process(ins, outs, 1);

    EXPECT_FLOAT_EQ(out[0], 5.f);
}

}
