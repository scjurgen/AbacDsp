#include <algorithm>
#include <string_view>

#include "gtest/gtest.h"

#include "Graph/GraphDescription.h"
#include "Graph/GraphValidator.h"
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

[[nodiscard]] bool hasErrorContaining(const std::vector<Diagnostic>& diagnostics, const std::string_view needle)
{
    return std::any_of(
        diagnostics.begin(), diagnostics.end(), [needle](const Diagnostic& d)
        { return d.severity == DiagnosticSeverity::Error && d.message.find(needle) != std::string::npos; });
}

[[nodiscard]] int countSeverity(const std::vector<Diagnostic>& diagnostics, const DiagnosticSeverity severity)
{
    return static_cast<int>(std::count_if(diagnostics.begin(), diagnostics.end(),
                                          [severity](const Diagnostic& d) { return d.severity == severity; }));
}

} // namespace

TEST(GraphValidatorTest, ValidChainHasNoErrors)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("p1", "PassThroughStub")};
    description.edges = {
        makeEdge("", "in", "p1", "in"),
        makeEdge("p1", "out", "", "out"),
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_EQ(countSeverity(diagnostics, DiagnosticSeverity::Error), 0);
}

TEST(GraphValidatorTest, UnknownNodeTypeFails)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "NoSuchType")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "unknown node type"));
}

TEST(GraphValidatorTest, UnknownPortNameFails)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    description.edges = {makeEdge("p1", "doesNotExist", "p2", "in")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "unknown port"));
}

TEST(GraphValidatorTest, OutputToOutputEdgeFails)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    description.edges = {makeEdge("p1", "out", "p2", "out")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "must be an input port"));
}

TEST(GraphValidatorTest, InputToInputEdgeFails)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    description.edges = {makeEdge("p1", "in", "p2", "in")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "must be an output port"));
}

TEST(GraphValidatorTest, ImplicitFanInOnNonMultiConnectableInputFails)
{
    GraphDescription description;
    description.nodes = {
        makeNode("p1", "PassThroughStub"),
        makeNode("p2", "PassThroughStub"),
        makeNode("p3", "PassThroughStub"),
    };
    description.edges = {
        makeEdge("p1", "out", "p3", "in"),
        makeEdge("p2", "out", "p3", "in"),
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "fan-in"));
}

TEST(GraphValidatorTest, DuplicateNodeIdsFail)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p1", "PassThroughStub")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "duplicate node id"));
}

TEST(GraphValidatorTest, DirectCycleWithoutBreakerFails)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    description.edges = {
        makeEdge("p1", "out", "p2", "in"),
        makeEdge("p2", "out", "p1", "in"),
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "feedback cycle has no breaksCycle node"));
}

TEST(GraphValidatorTest, CycleWithBreakerPasses)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("breaker", "CycleBreakerStub")};
    description.edges = {
        makeEdge("p1", "out", "breaker", "in"),
        makeEdge("breaker", "out", "p1", "in"),
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_EQ(countSeverity(diagnostics, DiagnosticSeverity::Error), 0);
}

TEST(GraphValidatorTest, UnconnectedInputAndUnusedNodeAreWarningsNotErrors)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {
        makeNode("p1", "PassThroughStub"), makeNode("lonelyGain", "GainStub"), // output unused, input unconnected
        makeNode("untouched", "PassThroughStub")                               // no edges at all
    };
    description.edges = {
        makeEdge("", "in", "p1", "in"),
        makeEdge("p1", "out", "", "out"),
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_EQ(countSeverity(diagnostics, DiagnosticSeverity::Error), 0);
    EXPECT_GT(countSeverity(diagnostics, DiagnosticSeverity::Warning), 0);
}

}
