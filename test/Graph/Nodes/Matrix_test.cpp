#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Matrix.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MatrixNodeTest, DefaultIsIdentity)
{
    Matrix matrix;
    const std::array<float, 2> inL{1.f, 2.f};
    const std::array<float, 2> inR{3.f, 4.f};
    std::array<float, 2> outL{};
    std::array<float, 2> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    matrix.process(ins, outs, inL.size());

    EXPECT_EQ(outL, inL);
    EXPECT_EQ(outR, inR);
}

TEST(MatrixNodeTest, ParametersSwapChannels)
{
    Matrix matrix;
    matrix.setParameter(0, 0.0f); // gainLL
    matrix.setParameter(1, 1.0f); // gainLR
    matrix.setParameter(2, 1.0f); // gainRL
    matrix.setParameter(3, 0.0f); // gainRR

    const std::array<float, 1> inL{1.f};
    const std::array<float, 1> inR{2.f};
    std::array<float, 1> outL{};
    std::array<float, 1> outR{};
    std::array<const float*, 2> ins{inL.data(), inR.data()};
    std::array<float*, 2> outs{outL.data(), outR.data()};

    matrix.process(ins, outs, 1);

    EXPECT_FLOAT_EQ(outL[0], 2.f);
    EXPECT_FLOAT_EQ(outR[0], 1.f);
}

}
