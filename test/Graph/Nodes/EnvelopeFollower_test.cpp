#include <array>

#include "gtest/gtest.h"

#include "Graph/Nodes/EnvelopeFollower.h"

namespace AbacDsp::Graph::Nodes::Test
{

TEST(EnvelopeFollowerNodeTest, RisesTowardStepInputThenFallsToSilence)
{
    constexpr float kSampleRate = 48000.f;
    EnvelopeFollower follower{kSampleRate};
    follower.setParameter(0, 1.0f);  // fast attack
    follower.setParameter(1, 10.0f); // release

    const auto numSamples = static_cast<size_t>(kSampleRate * 0.02f); // 20 ms
    std::array<float, 1> loud{1.f};
    std::array<float, 1> out{};
    std::array<const float*, 1> ins{loud.data()};
    std::array<float*, 1> outs{out.data()};
    for (size_t i = 0; i < numSamples; ++i)
    {
        follower.process(ins, outs, 1);
    }
    const float risen = out[0];
    EXPECT_GT(risen, 0.5f);

    std::array<float, 1> silence{0.f};
    ins[0] = silence.data();
    for (size_t i = 0; i < numSamples; ++i)
    {
        follower.process(ins, outs, 1);
    }
    EXPECT_LT(out[0], risen);
}

}
