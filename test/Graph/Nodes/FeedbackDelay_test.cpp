#include <array>

#include "gtest/gtest.h"

#include "../GraphTestNodes.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/FeedbackDelay.h"
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

TEST(FeedbackDelayNodeTest, PassesInputThroughUnchanged)
{
    FeedbackDelay node;
    const std::array<float, 3> in{1.f, -2.f, 3.f};
    std::array<float, 3> out{};
    std::array<const float*, 1> ins{in.data()};
    std::array<float*, 1> outs{out.data()};

    node.process(ins, outs, in.size());

    EXPECT_EQ(out, in);
}

// Mirrors CompiledGraphTest.CycleBreakerDelaysFeedbackByOneBlock, using the
// real registered node instead of the test-only stub.
TEST(FeedbackDelayNodeTest, BreaksACycleByOneBlockWhenCompiled)
{
    NodeRegistry registry;
    registerFilterNodes(registry);
    AbacDsp::Graph::Test::registerTestNodes(registry); // SumStub

    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("sum", "SumStub"), makeNode("fb", "FeedbackDelay")};
    description.edges = {
        makeEdge("", "in", "sum", "in1"),
        makeEdge("fb", "out", "sum", "in2"),
        makeEdge("sum", "out", "fb", "in"),
        makeEdge("fb", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    float output = 0.f;
    std::array<float*, 1> outs{&output};

    float in = 10.f;
    std::array<const float*, 1> ins{&in};
    result.graph->process(ins, outs, 1);
    EXPECT_FLOAT_EQ(output, 0.f);

    in = 0.f;
    result.graph->process(ins, outs, 1);
    EXPECT_FLOAT_EQ(output, 10.f);

    result.graph->process(ins, outs, 1);
    EXPECT_FLOAT_EQ(output, 10.f);
}

}
