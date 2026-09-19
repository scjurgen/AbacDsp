#include <array>
#include <span>
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

TEST(CompiledGraphTest, FeedbackSlotExposesTheFeedbackSourcesLiveContent)
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

    auto result = GraphCompiler::compile(description, makeRegistry(), 4, 48000.f);
    ASSERT_TRUE(result.graph.has_value());
    ASSERT_EQ(result.graph->feedbackSlotCount(), 1u);

    const std::vector<float> input(4, 10.f);
    std::vector<float> output(4, 0.f);
    std::array<const float*, 1> ins{input.data()};
    std::array<float*, 1> outs{output.data()};
    result.graph->process(ins, outs, 4);

    const std::span<const float> feedback = result.graph->feedbackSlotBuffer(0);
    EXPECT_FLOAT_EQ(feedback[0], 10.f);
}

TEST(CompiledGraphTest, FindNodeReturnsMatchingNodeOrNullptr)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("g", "GainStub")};
    description.edges = {
        makeEdge("", "in", "g", "in"),
        makeEdge("g", "out", "", "out"),
    };

    auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    Node* gainNode = result.graph->findNode("g");
    ASSERT_NE(gainNode, nullptr);
    gainNode->setParameter(0, 3.0f);

    const std::vector<float> input{1.f, 2.f};
    std::vector<float> output(input.size(), 0.f);
    std::array<const float*, 1> ins{input.data()};
    std::array<float*, 1> outs{output.data()};
    result.graph->process(ins, outs, input.size());

    EXPECT_FLOAT_EQ(output[0], 3.f);
    EXPECT_FLOAT_EQ(output[1], 6.f);
    EXPECT_EQ(result.graph->findNode("nosuchnode"), nullptr);
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

namespace
{

// Counts how often its parameter is set: a node that restarts a smoothing ramp on every set would
// never settle if a steady control value were pushed each block.
class CountingGainNode final : public Node
{
  public:
    explicit CountingGainNode(int& setCount) noexcept
        : m_setCount(setCount)
    {
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::copy_n(inputs[0], numSamples, outputs[0]);
    }

    void setParameter(const size_t paramIndex, const float) noexcept override
    {
        m_setCount += paramIndex == 0 ? 1 : 0;
    }

  private:
    int& m_setCount;
};

} // namespace

TEST(CompiledGraphTest, AControlEdgeSetsItsParameterOnlyWhenTheValueChanges)
{
    int setCount = 0;
    NodeRegistry registry = makeRegistry();
    registry.registerType(
        "CountingGain",
        NodeSchema{{audioInPort("in"), audioOutPort("out")},
                   {ParameterDescriptor{"gain", "linear", 0.0f, 4.0f, 1.0f, ParameterMapping::Linear, 0.0f, true}},
                   false},
        [&setCount](const NodeInstance&, float) { return std::make_unique<CountingGainNode>(setCount); });

    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("c", "ConstantStub"), makeNode("g", "CountingGain")};
    description.nodes[0].params["value"] = 0.5f;
    description.edges = {makeEdge("", "in", "g", "in"), makeEdge("g", "out", "", "out")};
    description.controls = {ControlEdge{.fromNode = "c", .fromPort = "out", .toNode = "g", .toParam = "gain"}};

    auto result = GraphCompiler::compile(description, registry, 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());
    CompiledGraph& graph = *result.graph;

    const std::vector<float> input(64, 1.f);
    std::vector<float> output(64, 0.f);
    std::array<const float*, 1> ins{input.data()};
    std::array<float*, 1> outs{output.data()};
    for (int block = 0; block < 10; ++block)
    {
        graph.process(ins, outs, 64);
    }
    EXPECT_EQ(setCount, 1);

    graph.findNode("c")->setParameter(0, 0.75f);
    graph.process(ins, outs, 64);
    graph.process(ins, outs, 64);
    EXPECT_EQ(setCount, 2);
}

}
