#include <array>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
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

} // namespace

TEST(CompiledGraphTest, PassThroughChainReproducesInput)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("a", "PassThroughStub"), makeNode("b", "PassThroughStub")};
    description.edges = {
        makeEdge("", "in", "a", "in"),
        makeEdge("a", "out", "b", "in"),
        makeEdge("b", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const std::vector<float> input{1.f, 2.f, 3.f, 4.f, 5.f};
    std::vector<float> output(input.size(), 0.f);
    std::array<const float*, 1> ins{input.data()};
    std::array<float*, 1> outs{output.data()};

    result.graph->process(ins, outs, input.size());

    EXPECT_EQ(output, input);
}

TEST(CompiledGraphTest, GainStubSetParameterScalesOutput)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("g", "GainStub")};
    description.nodes[0].params["gain"] = 2.0f;
    description.edges = {
        makeEdge("", "in", "g", "in"),
        makeEdge("g", "out", "", "out"),
    };

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
}

TEST(CompiledGraphTest, SumStubAddsBothInputs)
{
    GraphDescription description;
    description.io = {{"inA", "inB"}, {"out"}};
    description.nodes = {makeNode("sum", "SumStub")};
    description.edges = {
        makeEdge("", "inA", "sum", "in1"),
        makeEdge("", "inB", "sum", "in2"),
        makeEdge("sum", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const std::vector<float> inputA{3.f, 3.f};
    const std::vector<float> inputB{4.f, 5.f};
    std::vector<float> output(inputA.size(), 0.f);
    std::array<const float*, 2> ins{inputA.data(), inputB.data()};
    std::array<float*, 1> outs{output.data()};

    result.graph->process(ins, outs, inputA.size());

    EXPECT_FLOAT_EQ(output[0], 7.f);
    EXPECT_FLOAT_EQ(output[1], 8.f);
}

TEST(CompiledGraphTest, CycleBreakerDelaysFeedbackByOneBlock)
{
    // out(block n) == sum(block n-1): breaker reads sum's still-live previous
    // block value before sum overwrites it this block (see GraphCompiler.h).
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("sum", "SumStub"), makeNode("breaker", "CycleBreakerStub")};
    description.edges = {
        makeEdge("", "in", "sum", "in1"),
        makeEdge("breaker", "out", "sum", "in2"),
        makeEdge("sum", "out", "breaker", "in"),
        makeEdge("breaker", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, makeRegistry(), 1, 48000.f);
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

TEST(CompiledGraphTest, RepeatedProcessingIsStable)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("a", "PassThroughStub")};
    description.edges = {
        makeEdge("", "in", "a", "in"),
        makeEdge("a", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const std::vector<float> input(64, 0.5f);
    std::vector<float> output(input.size(), 0.f);
    std::array<const float*, 1> ins{input.data()};
    std::array<float*, 1> outs{output.data()};

    for (int i = 0; i < 200; ++i)
    {
        result.graph->process(ins, outs, input.size());
    }

    EXPECT_EQ(output, input);
}

}
