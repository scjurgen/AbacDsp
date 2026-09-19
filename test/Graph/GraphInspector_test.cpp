#include <string>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/GraphInspector.h"
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

[[nodiscard]] Diagnostic makeDiagnostic(const DiagnosticSeverity severity, std::string message, std::string nodeId = "",
                                        std::string portName = "", std::string field = "", const int line = 0)
{
    return Diagnostic{severity, std::move(message), std::move(nodeId), std::move(portName), std::move(field), line};
}

[[nodiscard]] size_t countOccurrences(const std::string& text, const std::string& needle)
{
    size_t count = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1))
    {
        ++count;
    }
    return count;
}

[[nodiscard]] GraphDescription makeSmallGraph()
{
    GraphDescription description;
    description.name = "Small";
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("a", "PassThroughStub")};
    description.edges = {makeEdge("", "in", "a", "in"), makeEdge("a", "out", "", "out")};
    description.edges[0].gain = 0.5f;
    return description;
}

// in -> sum -> brk -> out, with brk fed back into sum. The edge closing the cycle into the
// breaker is sum.out -> brk.in.
[[nodiscard]] GraphDescription makeFeedbackGraph()
{
    GraphDescription description;
    description.name = "Feedback";
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("sum", "SumStub"), makeNode("brk", "CycleBreakerStub")};
    description.edges = {makeEdge("", "in", "sum", "in1"), makeEdge("sum", "out", "brk", "in"),
                         makeEdge("brk", "out", "sum", "in2"), makeEdge("brk", "out", "", "out")};
    return description;
}

} // namespace

TEST(GraphInspectorTest, SmallGraphDotIsExact)
{
    const std::string expected = "digraph \"Small\" {\n"
                                 "  rankdir=LR;\n"
                                 "  \"in:in\" [shape=plaintext, label=\"in\"];\n"
                                 "  \"out:out\" [shape=plaintext, label=\"out\"];\n"
                                 "  \"a\" [shape=box, label=\"a\\nPassThroughStub\"];\n"
                                 "  \"in:in\" -> \"a\" [label=\"in x0.5\"];\n"
                                 "  \"a\" -> \"out:out\" [label=\"out\"];\n"
                                 "}\n";
    EXPECT_EQ(GraphInspector::toDot(makeSmallGraph(), makeRegistry()), expected);
}

TEST(GraphInspectorTest, SmallGraphJsonIsExact)
{
    const std::string expected =
        "{\n"
        "  \"name\": \"Small\",\n"
        "  \"version\": 1,\n"
        "  \"io\": {\"inputs\": [\"in\"], \"outputs\": [\"out\"]},\n"
        "  \"nodes\": [\n"
        "    {\"id\": \"a\", \"type\": \"PassThroughStub\", \"breaksCycle\": false, \"params\": {}, \"config\": {}}\n"
        "  ],\n"
        "  \"edges\": [\n"
        "    {\"fromNode\": \"\", \"fromPort\": \"in\", \"toNode\": \"a\", \"toPort\": \"in\", \"gain\": 0.5, "
        "\"polarity\": 1, \"label\": \"\", \"feedback\": false},\n"
        "    {\"fromNode\": \"a\", \"fromPort\": \"out\", \"toNode\": \"\", \"toPort\": \"out\", \"gain\": 1, "
        "\"polarity\": 1, \"label\": \"\", \"feedback\": false}\n"
        "  ],\n"
        "  \"controls\": [],\n"
        "  \"macros\": [],\n"
        "  \"cycles\": []\n"
        "}\n";
    EXPECT_EQ(GraphInspector::toJson(makeSmallGraph(), makeRegistry()), expected);
}

TEST(GraphInspectorTest, IdsWithQuotesAndBackslashesAreEscaped)
{
    GraphDescription description;
    description.name = "say \"hi\"\\";
    EXPECT_NE(GraphInspector::toDot(description, makeRegistry()).find("digraph \"say \\\"hi\\\"\\\\\""),
              std::string::npos);
    EXPECT_NE(GraphInspector::toJson(description, makeRegistry()).find("\"name\": \"say \\\"hi\\\"\\\\\""),
              std::string::npos);
}

TEST(GraphInspectorTest, FeedbackEdgeIsMarkedInDotAndJsonAndTheBreakerIsDoubleBoxed)
{
    const auto description = makeFeedbackGraph();
    const auto registry = makeRegistry();

    const auto dot = GraphInspector::toDot(description, registry);
    EXPECT_NE(dot.find("\"sum\" -> \"brk\" [label=\"out -> in\", style=dashed, color=red];"), std::string::npos);
    EXPECT_NE(dot.find("\"brk\" [shape=box, peripheries=2, label="), std::string::npos);
    EXPECT_EQ(countOccurrences(dot, "style=dashed"), 1u);

    const auto json = GraphInspector::toJson(description, registry);
    EXPECT_EQ(countOccurrences(json, "\"feedback\": true"), 1u);
    EXPECT_NE(json.find("\"cycles\": [\n    {\"nodes\": [\"brk\", \"sum\"], \"breakers\": [\"brk\"]}\n  ]"),
              std::string::npos);
}

TEST(GraphInspectorTest, ExportsAreDeterministic)
{
    const auto description = makeFeedbackGraph();
    const auto registry = makeRegistry();
    EXPECT_EQ(GraphInspector::toDot(description, registry), GraphInspector::toDot(description, registry));
    EXPECT_EQ(GraphInspector::toJson(description, registry), GraphInspector::toJson(description, registry));
}

TEST(GraphInspectorTest, DescribeCyclesNamesTheBreakerOrItsAbsence)
{
    EXPECT_EQ(GraphInspector::describeCycles(makeSmallGraph(), makeRegistry()), "no feedback cycles\n");
    EXPECT_EQ(GraphInspector::describeCycles(makeFeedbackGraph(), makeRegistry()),
              "cycle 1: brk, sum (breaker: brk)\n");

    GraphDescription unbroken;
    unbroken.nodes = {makeNode("p1", "PassThroughStub"), makeNode("p2", "PassThroughStub")};
    unbroken.edges = {makeEdge("p1", "out", "p2", "in"), makeEdge("p2", "out", "p1", "in")};
    EXPECT_EQ(GraphInspector::describeCycles(unbroken, makeRegistry()), "cycle 1: p1, p2 (NO breaker)\n");
}

TEST(GraphInspectorTest, DescribeMacrosListsEveryTargetWithItsMap)
{
    GraphDescription description;
    description.macros = {Macro{
        .id = "depth",
        .label = "Depth",
        .defaultValue = 0.35f,
        .targets = {MacroTarget{"tape", "wowDepth", "vibratoWowDepth"}, MacroTarget{"tape", "flutterDepth", ""}}}};
    EXPECT_EQ(GraphInspector::describeMacros(description), "macro depth (\"Depth\", default 0.35)\n"
                                                           "  -> tape.wowDepth via vibratoWowDepth\n"
                                                           "  -> tape.flutterDepth\n");
    EXPECT_EQ(GraphInspector::describeMacros(GraphDescription{}), "no macros\n");
}

TEST(GraphInspectorTest, MacrosAndControlsAppearInDotAndJson)
{
    GraphDescription description;
    description.nodes = {makeNode("g", "GainStub"), makeNode("c", "ConstantStub")};
    description.controls = {ControlEdge{.fromNode = "c", .fromPort = "out", .toNode = "g", .toParam = "gain"}};
    description.macros = {Macro{.id = "m", .label = "M", .targets = {MacroTarget{"g", "gain", "linear"}}}};

    const auto dot = GraphInspector::toDot(description, makeRegistry());
    EXPECT_NE(dot.find("\"c\" -> \"g\" [style=dotted, label=\"out -> gain\"];"), std::string::npos);
    EXPECT_NE(dot.find("\"macro:m\" -> \"g\" [style=dotted, label=\"gain (linear)\"];"), std::string::npos);

    const auto json = GraphInspector::toJson(description, makeRegistry());
    EXPECT_NE(json.find("{\"toNode\": \"g\", \"toParam\": \"gain\", \"map\": \"linear\"}"), std::string::npos);
}

TEST(GraphInspectorTest, LayoutShowsASlotReusedOnceItsFirstUserIsDone)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {makeNode("a", "PassThroughStub"), makeNode("b", "PassThroughStub"),
                         makeNode("c", "PassThroughStub")};
    description.edges = {makeEdge("", "in", "a", "in"), makeEdge("a", "out", "b", "in"),
                         makeEdge("b", "out", "c", "in"), makeEdge("c", "out", "", "out")};
    auto result = GraphCompiler::compile(description, makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    EXPECT_EQ(GraphInspector::describeLayout(*result.graph),
              "5 buffer slots (3 reserved for silence, scratch and graph inputs)\n"
              "schedule: a, b, c\n"
              "slot 3: a.out (steps 0-1) c.out (steps 2-3)\n"
              "slot 4: b.out (steps 1-2)\n");

    const auto json = GraphInspector::toJson(description, makeRegistry(), &*result.graph);
    EXPECT_NE(json.find("\"schedule\": [\"a\", \"b\", \"c\"]"), std::string::npos);
    EXPECT_NE(json.find("{\"node\": \"a\", \"port\": \"out\", \"slot\": 3, \"first\": 0, \"last\": 1, "
                        "\"feedback\": false}"),
              std::string::npos);
}

TEST(GraphInspectorTest, LayoutMarksAFeedbackPortWithItsOwnSlot)
{
    auto result = GraphCompiler::compile(makeFeedbackGraph(), makeRegistry(), 64, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const auto text = GraphInspector::describeLayout(*result.graph);
    EXPECT_NE(text.find("sum.out (feedback)"), std::string::npos);
    EXPECT_EQ(result.graph->feedbackSlotCount(), 1u);
}

TEST(GraphInspectorTest, DiagnosticFormatIncludesOnlyTheKnownParts)
{
    EXPECT_EQ(GraphInspector::formatDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, "boom")), "error: boom");
    EXPECT_EQ(GraphInspector::formatDiagnostic(
                  makeDiagnostic(DiagnosticSeverity::Warning, "unconnected input", "tape", "inL")),
              "warning: unconnected input (node 'tape', port 'inL')");
    EXPECT_EQ(GraphInspector::formatDiagnostic(makeDiagnostic(DiagnosticSeverity::Error, "node param must be a number",
                                                              "tape", "wowDepth", "params.wowDepth", 12)),
              "error: node param must be a number (node 'tape', port 'wowDepth', field 'params.wowDepth', line 12)");
}

TEST(GraphInspectorTest, FormatDiagnosticsPutsOneEntryPerLine)
{
    const std::vector<Diagnostic> diagnostics{makeDiagnostic(DiagnosticSeverity::Error, "a"),
                                              makeDiagnostic(DiagnosticSeverity::Warning, "b")};
    EXPECT_EQ(GraphInspector::formatDiagnostics(diagnostics), "error: a\nwarning: b\n");
}

}
