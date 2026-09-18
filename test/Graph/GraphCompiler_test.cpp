#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/Node.h"
#include "Graph/NodeRegistry.h"
#include "GraphTestNodes.h"

namespace AbacDsp::Graph::Test
{
namespace
{

[[nodiscard]] NodeRegistry makeRegistry()
{
    NodeRegistry registry;
    registerTestNodes(registry);
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

[[nodiscard]] GraphDescription makeChainDescription(const int length)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};

    for (int i = 0; i < length; ++i)
    {
        description.nodes.push_back(makeNode("n" + std::to_string(i), "PassThroughStub"));
    }

    description.edges.push_back(makeEdge("", "in", "n0", "in"));
    for (int i = 0; i + 1 < length; ++i)
    {
        description.edges.push_back(makeEdge("n" + std::to_string(i), "out", "n" + std::to_string(i + 1), "in"));
    }
    description.edges.push_back(makeEdge("n" + std::to_string(length - 1), "out", "", "out"));
    return description;
}

} // namespace

TEST(GraphCompilerTest, ValidChainCompiles)
{
    const auto description = makeChainDescription(1);
    const auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);

    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->nodeCount(), 1u);
}

TEST(GraphCompilerTest, ErrorDiagnosticsPreventCompilation)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p1", "PassThroughStub")};

    const auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);

    EXPECT_FALSE(result.graph.has_value());
    const bool hasDuplicateError = std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                               [](const Diagnostic& d)
                                               {
                                                   return d.severity == DiagnosticSeverity::Error &&
                                                          d.message.find("duplicate node id") != std::string::npos;
                                               });
    EXPECT_TRUE(hasDuplicateError);
}

TEST(GraphCompilerTest, BufferSlotCountDoesNotGrowWithChainLength)
{
    const auto shortChain = GraphCompiler::compile(makeChainDescription(3), makeRegistry(), 64, 48000.f);
    const auto longChain = GraphCompiler::compile(makeChainDescription(6), makeRegistry(), 64, 48000.f);

    ASSERT_TRUE(shortChain.graph.has_value());
    ASSERT_TRUE(longChain.graph.has_value());
    EXPECT_EQ(shortChain.graph->bufferSlotCount(), longChain.graph->bufferSlotCount());
}

TEST(GraphCompilerTest, CycleWithBreakerCompiles)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("sum", "SumStub"), makeNode("breaker", "CycleBreakerStub")};
    description.edges = {
        makeEdge("", "in", "sum", "in1"),
        makeEdge("breaker", "out", "sum", "in2"),
        makeEdge("sum", "out", "breaker", "in"),
        makeEdge("breaker", "out", "", "out"),
    };

    const auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);

    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.graph->nodeCount(), 2u);
}

TEST(GraphCompilerTest, ControlEdgeAppliesSourceValueToTargetParameterSameBlock)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("src", "ConstantStub"), makeNode("g", "GainStub")};
    description.nodes[0].params["value"] = 2.0f;
    description.edges = {
        makeEdge("", "in", "g", "in"),
        makeEdge("g", "out", "", "out"),
    };
    description.controls = {makeControlEdge("src", "out", "g", "gain")};

    auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const std::vector<float> input{1.f, 2.f, 3.f};
    std::vector<float> output(input.size(), 0.f);
    std::array<const float*, 1> ins{input.data()};
    std::array<float*, 1> outs{output.data()};

    result.graph->process(ins, outs, input.size());
    EXPECT_FLOAT_EQ(output[0], 2.f);
    EXPECT_FLOAT_EQ(output[1], 4.f);
    EXPECT_FLOAT_EQ(output[2], 6.f);

    // Live change, exactly how an external macro/UI value reaches a control source.
    Node* src = result.graph->findNode("src");
    ASSERT_NE(src, nullptr);
    src->setParameter(0, 3.0f);

    result.graph->process(ins, outs, input.size());
    EXPECT_FLOAT_EQ(output[0], 3.f);
    EXPECT_FLOAT_EQ(output[1], 6.f);
    EXPECT_FLOAT_EQ(output[2], 9.f);
}

}
