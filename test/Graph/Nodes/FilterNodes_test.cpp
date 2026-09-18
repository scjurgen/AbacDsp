#include <array>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/FilterNodes.h"

namespace AbacDsp::Graph::Nodes::Test
{
namespace
{

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

TEST(FilterNodesIntegrationTest, BiquadLowPassIntoSaturatorCompilesAndRuns)
{
    NodeRegistry registry;
    registerFilterNodes(registry);

    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("lp", "Biquad"), makeNode("sat", "Saturator")};
    description.nodes[0].params["frequencyHz"] = 1000.0f;
    description.edges = {
        makeEdge("", "in", "lp", "in"),
        makeEdge("lp", "out", "sat", "in"),
        makeEdge("sat", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    for (int i = 0; i < 5000; ++i)
    {
        result.graph->process(ins, outs, 1);
    }

    EXPECT_NEAR(out, 1.f, 1e-2f);
}

TEST(FilterNodesIntegrationTest, CrossoverLR4RegisteredSchemaCompilesAndSplitsDcToLowOutput)
{
    NodeRegistry registry;
    registerFilterNodes(registry);

    GraphDescription description;
    description.io = {{"in"}, {"lowOut", "highOut"}};
    description.nodes = {makeNode("xo", "CrossoverLR4")};
    description.nodes[0].params["frequencyHz"] = 1000.0f;
    description.edges = {
        makeEdge("", "in", "xo", "in"),
        makeEdge("xo", "lowOut", "", "lowOut"),
        makeEdge("xo", "highOut", "", "highOut"),
    };

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const float in = 1.f;
    std::array<float, 2> out{};
    std::array<const float*, 1> ins{&in};
    std::array<float*, 2> outs{&out[0], &out[1]};
    for (int i = 0; i < 5000; ++i)
    {
        result.graph->process(ins, outs, 1);
    }

    EXPECT_NEAR(out[0], 1.f, 1e-2f);
    EXPECT_NEAR(out[1], 0.f, 1e-2f);
}

}
