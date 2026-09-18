#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/Slew.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(SlewNodeTest, StepInputDoesNotJumpInstantly)
{
    constexpr float kSampleRate = 48000.f;
    Slew slew{kSampleRate};

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};

    slew.process(ins, outs, 1);

    EXPECT_GT(out, 0.f);
    EXPECT_LT(out, 1.f);
}

TEST(SlewNodeTest, SettlesCloseToTargetAfterEnoughSamples)
{
    constexpr float kSampleRate = 48000.f;
    Slew slew{kSampleRate};
    slew.setParameter(0, 5.0f); // 5 ms

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};

    const auto numSamples = static_cast<size_t>(kSampleRate * 0.1f); // 100 ms, well beyond 5 ms
    for (size_t i = 0; i < numSamples; ++i)
    {
        slew.process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 1.f, 1e-3f);
}

}
