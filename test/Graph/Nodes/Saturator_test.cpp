#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Graph/Nodes/Saturator.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(SaturatorNodeTest, SmallSignalAtZeroDriveIsNearlyUnchanged)
{
    Saturator saturator;
    const std::array<float, 1> in{0.1f};
    std::array<float, 1> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    saturator.process(ins, outs, 1);

    EXPECT_NEAR(out[0], in[0], 0.01f);
}

TEST(SaturatorNodeTest, HigherDriveCompressesALargeSignalMoreTowardTheLimit)
{
    Saturator low;
    Saturator high;
    high.setParameter(0, 10.f);

    const std::array<float, 1> in{0.7f};
    std::array<float, 1> outLow{};
    std::array<float, 1> outHigh{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outsLow{outLow.data()};
    std::array<float*, 1> outsHigh{outHigh.data()};

    low.process(ins, outsLow, 1);
    high.process(ins, outsHigh, 1);

    ASSERT_TRUE(std::isfinite(outLow[0]));
    ASSERT_TRUE(std::isfinite(outHigh[0]));
    EXPECT_GT(outHigh[0], outLow[0]); // more driven -> pushed harder toward the atanh limit
}

}
