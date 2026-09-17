#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "../../examples/pathfinder/src/impl/PathfinderImpl.h"
#include "Audio/AudioBuffer.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/TapeDelayNode.h"

namespace AbacDsp::Graph::Test
{
namespace
{

constexpr size_t kBlockSize = 64;
constexpr float kSampleRate = 48000.f;

[[nodiscard]] NodeInstance makeNode(std::string id, std::string type)
{
    return NodeInstance{.id = std::move(id), .type = std::move(type)};
}

[[nodiscard]] Edge makeEdge(std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
{
    return Edge{.fromNode = std::move(fromNode),
                .fromPort = std::move(fromPort),
                .toNode = std::move(toNode),
                .toPort = std::move(toPort)};
}

} // namespace

// Phases 1-3 exit criteria: a hand-built graph reproduces PathfinderImpl.h's
// actual output bit-exact. Its macro constants are private, so the numbers
// below are copied from applyMacros(), not referenced by symbol.
TEST(PathfinderV1ComparisonTest, HandBuiltGraphMatchesPathfinderImplOutput)
{
    constexpr float kWowDepthAtZero = 0.15f;
    constexpr float kWowDepthAtOne = 0.45f;
    constexpr float kFlutterDepthAtZero = 0.1f;
    constexpr float kFlutterDepthAtOne = 0.5f;
    constexpr float kDepth = 0.35f;
    constexpr float kSpeedHz = 0.8f;
    constexpr float kFlutterRateFloorHz = 0.6f;
    const float wowDepth = kWowDepthAtZero + (kWowDepthAtOne - kWowDepthAtZero) * kDepth;
    const float flutterDepth = kFlutterDepthAtZero + (kFlutterDepthAtOne - kFlutterDepthAtZero) * kDepth;
    const float wowRate = kSpeedHz;
    const float flutterRate = std::max(kSpeedHz, kFlutterRateFloorHz);

    PathfinderImpl<kBlockSize> pathfinder(kSampleRate);
    // Forces wowVariance=wowDrift=0 on v1's side - see the plan's Wow.h note
    // on why a bit-exact comparison needs this regardless of correctness.
    pathfinder.setAggressivity(0.f);

    NodeRegistry registry;
    Nodes::registerTapeDelayNode<kBlockSize>(registry);

    GraphDescription description;
    description.io = {{"inL", "inR"}, {"outL", "outR"}};
    description.nodes = {makeNode("tape", "TapeDelay")};
    description.nodes[0].params["wowDepth"] = wowDepth;
    description.nodes[0].params["wowRate"] = wowRate;
    description.nodes[0].params["flutterDepth"] = flutterDepth;
    description.nodes[0].params["flutterRate"] = flutterRate;
    description.nodes[0].params["wowVariance"] = 0.0f;
    description.nodes[0].params["wowDrift"] = 0.0f;
    description.edges = {
        makeEdge("", "inL", "tape", "inL"),
        makeEdge("", "inR", "tape", "inR"),
        makeEdge("tape", "outL", "", "outL"),
        makeEdge("tape", "outR", "", "outR"),
    };

    auto result = GraphCompiler::compile(description, registry, kBlockSize, kSampleRate);
    ASSERT_TRUE(result.graph.has_value());

    for (int b = 0; b < 40; ++b)
    {
        AbacDsp::AudioBuffer<2, kBlockSize> in;
        AbacDsp::AudioBuffer<2, kBlockSize> out;
        std::array<float, kBlockSize> inL{};
        std::array<float, kBlockSize> inR{};
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            const float phase = static_cast<float>(b * kBlockSize + i) * 440.f / kSampleRate;
            const float sample = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * phase);
            in(i, 0) = sample;
            in(i, 1) = sample;
            inL[i] = sample;
            inR[i] = sample;
        }
        pathfinder.processBlock(in, out);

        std::array<float, kBlockSize> outL{};
        std::array<float, kBlockSize> outR{};
        std::array<const float*, 2> ins{inL.data(), inR.data()};
        std::array<float*, 2> outs{outL.data(), outR.data()};
        result.graph->process(ins, outs, kBlockSize);

        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_EQ(outL[i], out(i, 0)) << "block " << b << " sample " << i;
            ASSERT_EQ(outR[i], out(i, 1)) << "block " << b << " sample " << i;
        }
    }
}

}
