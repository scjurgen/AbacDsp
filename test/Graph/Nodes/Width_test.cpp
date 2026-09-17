#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Width.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(WidthNodeTest, DefaultIsUnchanged)
{
    Width width;
    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{0.2f};
    std::array<float, 1> outL{};
    std::array<float, 1> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    width.process(ins, outs, 1);

    EXPECT_NEAR(outL[0], inL[0], 1e-6f);
    EXPECT_NEAR(outR[0], inR[0], 1e-6f);
}

TEST(WidthNodeTest, ZeroCollapsesToMono)
{
    Width width;
    width.setParameter(0, 0.0f);

    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{0.2f};
    std::array<float, 1> outL{};
    std::array<float, 1> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    width.process(ins, outs, 1);

    EXPECT_FLOAT_EQ(outL[0], outR[0]);
    EXPECT_NEAR(outL[0], 0.6f, 1e-6f);
}

TEST(WidthNodeTest, TwoDoublesSideSignal)
{
    Width width;
    width.setParameter(0, 2.0f);

    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{0.f};
    std::array<float, 1> outL{};
    std::array<float, 1> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    width.process(ins, outs, 1);

    EXPECT_NEAR(outL[0], 1.5f, 1e-6f);
    EXPECT_NEAR(outR[0], -0.5f, 1e-6f);
}

}
