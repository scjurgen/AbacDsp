#include <array>

#include "gtest/gtest.h"

#include "../GraphTestNodes.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/ControlNodes.h"

namespace AbacDsp::Graph::Nodes::Test
{
namespace
{

[[nodiscard]] NodeRegistry makeRegistry()
{
    NodeRegistry registry;
    registerControlNodes(registry);
    AbacDsp::Graph::Test::registerTestNodes(registry); // GainStub - an audio-domain sink to observe control output
    return registry;
}

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

[[nodiscard]] ControlEdge makeControlEdge(std::string fromNode, std::string fromPort, std::string toNode,
                                          std::string toParam)
{
    return ControlEdge{.fromNode = std::move(fromNode),
                       .fromPort = std::move(fromPort),
                       .toNode = std::move(toNode),
                       .toParam = std::move(toParam)};
}

} // namespace

// A control-node chain can only reach the outside world through a ControlEdge
// onto an audio node's parameter - graph boundary ports are always audio.mono.
TEST(ControlNodesIntegrationTest, MacroScaleOffsetChainDrivesGainStubParameter)
{
    auto registry = makeRegistry();

    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("m", "Macro"), makeNode("so", "ScaleOffset"), makeNode("g", "GainStub")};
    description.nodes[0].params["value"] = 0.5f;
    description.nodes[1].params["scale"] = 2.0f;
    description.nodes[1].params["offset"] = 1.0f;
    description.edges = {
        makeEdge("m", "out", "so", "in"),
        makeEdge("", "in", "g", "in"),
        makeEdge("g", "out", "", "out"),
    };
    description.controls = {makeControlEdge("so", "out", "g", "gain")};

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const float in = 3.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    result.graph->process(ins, outs, 1);

    EXPECT_FLOAT_EQ(out, 6.f); // gain = 0.5*2+1 = 2.0, output = 3.0*2.0
}

TEST(ControlNodesIntegrationTest, ClampBoundedByAMacroDrivesGainStubParameter)
{
    auto registry = makeRegistry();

    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("src", "Constant"), makeNode("mmax", "Macro"), makeNode("c", "Clamp"),
                         makeNode("g", "GainStub")};
    description.nodes[0].config["value"] = "5.0";
    description.nodes[1].params["value"] = 0.2f;
    description.edges = {
        makeEdge("src", "out", "c", "in"),
        makeEdge("", "in", "g", "in"),
        makeEdge("g", "out", "", "out"),
    };
    description.controls = {
        makeControlEdge("mmax", "out", "c", "max"),
        makeControlEdge("c", "out", "g", "gain"),
    };

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const float in = 1.f;
    float out = 0.f;
    std::array<const float*, 1> ins{&in};
    std::array<float*, 1> outs{&out};
    result.graph->process(ins, outs, 1);

    EXPECT_FLOAT_EQ(out, 0.2f); // src=5.0 clamped to max=0.2 (driven by mmax), gain=0.2, output = 1.0*0.2
}

}
