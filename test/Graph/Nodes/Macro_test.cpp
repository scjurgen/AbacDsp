#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Macro.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(MacroNodeTest, DefaultValueIsZero)
{
    Macro macro;
    std::array<float, 3> out{1.f, 1.f, 1.f};
    std::array<float*, 1> outs{out.data()};

    macro.process({}, outs, out.size());

    EXPECT_EQ(out, (std::array<float, 3>{0.f, 0.f, 0.f}));
}

TEST(MacroNodeTest, SetParameterFillsEveryOutputSample)
{
    Macro macro;
    macro.setParameter(0, 0.35f);
    std::array<float, 4> out{};
    std::array<float*, 1> outs{out.data()};

    macro.process({}, outs, out.size());

    for (const float sample : out)
    {
        EXPECT_FLOAT_EQ(sample, 0.35f);
    }
}

}
