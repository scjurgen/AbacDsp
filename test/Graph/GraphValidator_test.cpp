#include <algorithm>
#include <string_view>

#include "gtest/gtest.h"

#include "Graph/GraphDescription.h"
#include "Graph/GraphValidator.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/FilterNodes.h"
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

[[nodiscard]] bool hasErrorContaining(const std::vector<Diagnostic>& diagnostics, const std::string_view needle)
{
    return std::any_of(
        diagnostics.begin(), diagnostics.end(), [needle](const Diagnostic& d)
        { return d.severity == DiagnosticSeverity::Error && d.message.find(needle) != std::string::npos; });
}

[[nodiscard]] bool hasWarningContaining(const std::vector<Diagnostic>& diagnostics, const std::string_view needle)
{
    return std::any_of(
        diagnostics.begin(), diagnostics.end(), [needle](const Diagnostic& d)
        { return d.severity == DiagnosticSeverity::Warning && d.message.find(needle) != std::string::npos; });
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

TEST(GraphValidatorTest, ControlEdgeFromGraphBoundaryFails)
{
    GraphDescription description;
    description.io = {{"in"}, {}};
    description.nodes = {makeNode("g", "GainStub")};
    description.controls = {makeControlEdge("", "in", "g", "gain")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "control edge source must be a node"));
}

TEST(GraphValidatorTest, ControlEdgeFromUnknownNodeFails)
{
    GraphDescription description;
    description.nodes = {makeNode("g", "GainStub")};
    description.controls = {makeControlEdge("nosuch", "out", "g", "gain")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "unknown control source node"));
}

TEST(GraphValidatorTest, ControlEdgeFromInputPortFails)
{
    GraphDescription description;
    description.nodes = {makeNode("src", "PassThroughStub"), makeNode("g", "GainStub")};
    description.controls = {makeControlEdge("src", "in", "g", "gain")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "control edge source must be an output port"));
}

TEST(GraphValidatorTest, ControlEdgeToUnknownNodeFails)
{
    GraphDescription description;
    description.nodes = {makeNode("src", "ConstantStub")};
    description.controls = {makeControlEdge("src", "out", "nosuch", "gain")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "unknown control target node"));
}

TEST(GraphValidatorTest, ControlEdgeToUnknownParameterFails)
{
    GraphDescription description;
    description.nodes = {makeNode("src", "ConstantStub"), makeNode("g", "GainStub")};
    description.controls = {makeControlEdge("src", "out", "g", "noSuchParam")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "unknown control target parameter"));
}

TEST(GraphValidatorTest, ControlEdgeToNonAutomatableParameterFails)
{
    GraphDescription description;
    description.nodes = {makeNode("src", "ConstantStub"), makeNode("g", "NonAutomatableGainStub")};
    description.controls = {makeControlEdge("src", "out", "g", "gain")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "not automatable"));
}

TEST(GraphValidatorTest, ValidControlEdgePassesAndSourceIsNotFlaggedAsUnused)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("src", "ConstantStub"), makeNode("g", "GainStub")};
    description.edges = {makeEdge("", "in", "g", "in"), makeEdge("g", "out", "", "out")};
    description.controls = {makeControlEdge("src", "out", "g", "gain")};

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_EQ(countSeverity(diagnostics, DiagnosticSeverity::Error), 0);
    EXPECT_EQ(countSeverity(diagnostics, DiagnosticSeverity::Warning), 0);
}

TEST(GraphValidatorTest, FeedbackEdgeGainAboveLimitFails)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("breaker", "CycleBreakerStub")};
    description.edges = {
        makeEdge("p1", "out", "breaker", "in"),
        Edge{.fromNode = "breaker", .fromPort = "out", .toNode = "p1", .toPort = "in", .gain = 1.5f},
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasErrorContaining(diagnostics, "feedback edge gain exceeds"));
}

TEST(GraphValidatorTest, CyclicSccWithoutDampingNodeWarns)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("breaker", "CycleBreakerStub")};
    description.edges = {
        makeEdge("p1", "out", "breaker", "in"),
        makeEdge("breaker", "out", "p1", "in"),
    };

    const auto diagnostics = GraphValidator::validate(description, makeRegistry());
    EXPECT_TRUE(hasWarningContaining(diagnostics, "no damping filter"));
}

TEST(GraphValidatorTest, CyclicSccWithDampingNodeDoesNotWarnAboutDamping)
{
    NodeRegistry registry;
    registerTestNodes(registry);
    Nodes::registerFilterNodes(registry);

    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("breaker", "CycleBreakerStub"),
                         makeNode("lp", "OnePoleLP")};
    description.edges = {
        makeEdge("p1", "out", "breaker", "in"),
        makeEdge("breaker", "out", "lp", "in"),
        makeEdge("lp", "out", "p1", "in"),
    };

    const auto diagnostics = GraphValidator::validate(description, registry);
    EXPECT_FALSE(hasWarningContaining(diagnostics, "no damping filter"));
}

TEST(GraphValidatorTest, ResonantFilterInUndampedFeedbackCycleWarns)
{
    NodeRegistry registry;
    registerTestNodes(registry);
    Nodes::registerFilterNodes(registry);

    GraphDescription description;
    description.nodes = {makeNode("breaker", "CycleBreakerStub"), makeNode("bp", "BandPass")};
    description.nodes[1].params["Q"] = 5.0f;
    description.edges = {
        makeEdge("breaker", "out", "bp", "in"),
        makeEdge("bp", "out", "breaker", "in"),
    };

    const auto diagnostics = GraphValidator::validate(description, registry);
    EXPECT_TRUE(hasWarningContaining(diagnostics, "resonant filter"));
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

TEST(GraphValidatorTest, FindFeedbackComponentsReturnsNothingForAnAcyclicGraph)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    description.edges = {makeEdge("p1", "out", "p2", "in")};
    EXPECT_TRUE(GraphValidator::findFeedbackComponents(description, makeRegistry()).empty());
}

TEST(GraphValidatorTest, FindFeedbackComponentsReportsTheCycleAndItsBreaker)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("breaker", "CycleBreakerStub")};
    description.edges = {makeEdge("p1", "out", "breaker", "in"), makeEdge("breaker", "out", "p1", "in")};

    const auto components = GraphValidator::findFeedbackComponents(description, makeRegistry());
    ASSERT_EQ(components.size(), 1u);
    EXPECT_EQ(components[0].nodeIds, (std::vector<std::string>{"breaker", "p1"}));
    EXPECT_EQ(components[0].breakerIds, (std::vector<std::string>{"breaker"}));
}

TEST(GraphValidatorTest, FindFeedbackComponentsReportsAnUnbrokenCycleWithoutBreakers)
{
    GraphDescription description;
    description.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    description.edges = {makeEdge("p1", "out", "p2", "in"), makeEdge("p2", "out", "p1", "in")};

    const auto components = GraphValidator::findFeedbackComponents(description, makeRegistry());
    ASSERT_EQ(components.size(), 1u);
    EXPECT_TRUE(components[0].breakerIds.empty());
}

TEST(GraphValidatorTest, FindFeedbackComponentsCountsASelfLoop)
{
    GraphDescription description;
    description.nodes = {makeNode("breaker", "CycleBreakerStub")};
    description.edges = {makeEdge("breaker", "out", "breaker", "in")};
    EXPECT_EQ(GraphValidator::findFeedbackComponents(description, makeRegistry()).size(), 1u);
}

}
