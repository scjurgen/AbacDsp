#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Gain.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(GainNodeTest, DefaultGainIsUnity)
{
    Gain gain;
    const std::array<float, 3> inL{1.f, 2.f, 3.f};
    const std::array<float, 3> inR{-1.f, -2.f, -3.f};
    std::array<float, 3> outL{};
    std::array<float, 3> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    gain.process(ins, outs, inL.size());

    EXPECT_EQ(outL, inL);
    EXPECT_EQ(outR, inR);
}

TEST(GainNodeTest, SetParameterScalesBothChannelsInDb)
{
    Gain gain;
    gain.setParameter(0, -6.0f);
    const float expected = Convert::dbToGain(-6.0f);

    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{1.f};
    std::array<float, 1> outL{};
    std::array<float, 1> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    gain.process(ins, outs, 1);

    EXPECT_FLOAT_EQ(outL[0], expected);
    EXPECT_FLOAT_EQ(outR[0], expected);
}

}
