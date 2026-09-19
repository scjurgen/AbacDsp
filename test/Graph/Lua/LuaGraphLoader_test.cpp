#include <algorithm>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/Lua/LuaGraphLoader.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/StandardNodes.h"
#include "Graph/Nodes/TapeDelayNode.h"

namespace AbacDsp::Graph::Lua::Test
{
namespace
{

constexpr std::string_view kMinimalGraph = R"lua(
return {
  version = 1,
  name = "Minimal",
  io = { inputs = { "in" }, outputs = { "out" } },
  nodes = {
    { id = "n1", type = "Passthrough", params = { gain = 0.5 }, config = { mode = "fast" } },
  },
  edges = {
    { from = "in", to = "n1.in" },
    { from = "n1.out", to = "out" },
  },
  controls = {
    { from = "n1.out", to = "n1.gain", map = "linear", smoothingMs = 5 },
  },
  macros = {
    { id = "m1", label = "Mix", default = 0.25,
      targets = { { to = "n1.gain", map = "linear" } } },
  },
}
)lua";

// Chorus.md's "First graph: tape vibrato" example, verbatim.
constexpr std::string_view kTapeVibratoGraph = R"lua(
return {
  version = 1,
  name = "Tape Vibrato",

  io = {
    inputs  = { "inL", "inR" },
    outputs = { "outL", "outR" },
  },

  nodes = {
    {
      id = "tape",
      type = "TapeDelay",
      config = {
        channels = 2,
        writeRateHz = 4800,
        maxDelayMs = 30,
        interpolation = "cubic",
        modulationMode = "sharedStereo",
      },
      params = {
        baseDelayMs = 8.0,
        transportRatio = 1.0,

        wowDepth = 0.25,
        wowRate = 0.30,
        wowVariance = 0.10,
        wowDrift = 0.00,

        flutterDepth = 0.02,
        flutterRate = 4.0,
      },
    },
  },

  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "outL" },
    { from = "tape.outR", to = "outR" },
  },

  macros = {
    {
      id = "depth",
      label = "Depth",
      default = 0.35,
      targets = {
        { to = "tape.wowDepth", map = "vibratoWowDepth" },
        { to = "tape.flutterDepth", map = "vibratoFlutterDepth" },
      },
    },
    {
      id = "speed",
      label = "Speed",
      default = 0.35,
      targets = {
        { to = "tape.wowRate", map = "vibratoWowRate" },
        { to = "tape.flutterRate", map = "vibratoFlutterRate" },
      },
    },
    {
      id = "aggressivity",
      label = "OU Aggressivity",
      default = 0.15,
      targets = {
        { to = "tape.wowVariance", map = "vibratoWowVariance" },
        { to = "tape.wowDrift", map = "vibratoWowDrift" },
      },
    },
    {
      id = "character",
      label = "Character",
      default = 0.50,
      targets = {
        { to = "tape.transportRatio", map = "tapeToCleanTransportRatio" },
      },
    },
  },
}
)lua";

[[nodiscard]] bool hasSingleError(const LoadResult& result)
{
    return !result.description.has_value() && result.diagnostics.size() == 1 &&
           result.diagnostics.front().severity == DiagnosticSeverity::Error;
}

} // namespace

TEST(LuaGraphLoaderTest, MinimalGraphParsesToExactGraphDescription)
{
    const LoadResult result = LuaGraphLoader::loadFromString(kMinimalGraph);
    ASSERT_TRUE(result.description.has_value());
    const GraphDescription& description = *result.description;

    EXPECT_EQ(description.version, 1);
    EXPECT_EQ(description.name, "Minimal");
    ASSERT_EQ(description.io.inputs.size(), 1u);
    EXPECT_EQ(description.io.inputs[0], "in");
    ASSERT_EQ(description.io.outputs.size(), 1u);
    EXPECT_EQ(description.io.outputs[0], "out");

    ASSERT_EQ(description.nodes.size(), 1u);
    EXPECT_EQ(description.nodes[0].id, "n1");
    EXPECT_EQ(description.nodes[0].type, "Passthrough");
    ASSERT_TRUE(description.nodes[0].params.contains("gain"));
    EXPECT_FLOAT_EQ(description.nodes[0].params.at("gain"), 0.5f);
    ASSERT_TRUE(description.nodes[0].config.contains("mode"));
    EXPECT_EQ(description.nodes[0].config.at("mode"), "fast");

    ASSERT_EQ(description.edges.size(), 2u);
    EXPECT_EQ(description.edges[0].fromNode, "");
    EXPECT_EQ(description.edges[0].fromPort, "in");
    EXPECT_EQ(description.edges[0].toNode, "n1");
    EXPECT_EQ(description.edges[0].toPort, "in");
    EXPECT_EQ(description.edges[1].fromNode, "n1");
    EXPECT_EQ(description.edges[1].fromPort, "out");
    EXPECT_EQ(description.edges[1].toNode, "");
    EXPECT_EQ(description.edges[1].toPort, "out");

    ASSERT_EQ(description.controls.size(), 1u);
    EXPECT_EQ(description.controls[0].fromNode, "n1");
    EXPECT_EQ(description.controls[0].fromPort, "out");
    EXPECT_EQ(description.controls[0].toNode, "n1");
    EXPECT_EQ(description.controls[0].toParam, "gain");
    EXPECT_EQ(description.controls[0].mapName, "linear");
    EXPECT_FLOAT_EQ(description.controls[0].smoothingMs, 5.0f);

    ASSERT_EQ(description.macros.size(), 1u);
    EXPECT_EQ(description.macros[0].id, "m1");
    EXPECT_EQ(description.macros[0].label, "Mix");
    EXPECT_FLOAT_EQ(description.macros[0].defaultValue, 0.25f);
    ASSERT_EQ(description.macros[0].targets.size(), 1u);
    EXPECT_EQ(description.macros[0].targets[0].toNode, "n1");
    EXPECT_EQ(description.macros[0].targets[0].toParam, "gain");
    EXPECT_EQ(description.macros[0].targets[0].mapName, "linear");
}

TEST(LuaGraphLoaderTest, TapeVibratoGraphParsesValidatesAndCompiles)
{
    constexpr size_t kBlockSize = 64;
    constexpr float kSampleRate = 48000.f;

    const LoadResult result = LuaGraphLoader::loadFromString(kTapeVibratoGraph);
    ASSERT_TRUE(result.description.has_value());
    EXPECT_TRUE(result.diagnostics.empty());

    NodeRegistry registry;
    Nodes::registerStandardNodes(registry);
    Nodes::registerTapeDelayNode<kBlockSize>(registry);

    const auto diagnostics = GraphValidator::validate(*result.description, registry);
    const bool hasError = std::any_of(diagnostics.begin(), diagnostics.end(),
                                      [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Error; });
    EXPECT_FALSE(hasError);

    const auto compiled = GraphCompiler::compile(*result.description, registry, kBlockSize, kSampleRate);
    EXPECT_TRUE(compiled.graph.has_value());
}

TEST(LuaGraphLoaderTest, SyntaxErrorYieldsSingleErrorAndNoDescription)
{
    const LoadResult result = LuaGraphLoader::loadFromString("return { this is not valid lua");
    EXPECT_TRUE(hasSingleError(result));
}

TEST(LuaGraphLoaderTest, NodeMissingTypeYieldsSingleErrorAndNoDescription)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return { nodes = { { id = "n1" } } }
    )lua");
    EXPECT_TRUE(hasSingleError(result));
}

TEST(LuaGraphLoaderTest, UnsupportedVersionYieldsSingleErrorAndNoDescription)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return { version = 2, io = { inputs = {}, outputs = {} } }
    )lua");
    EXPECT_TRUE(hasSingleError(result));
}

TEST(LuaGraphLoaderTest, MacroTargetWithTableMapYieldsSingleErrorAndNoDescription)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return {
          nodes = { { id = "n1", type = "X" } },
          macros = {
            { id = "m1", targets = { { to = "n1.p", map = { type = "linear", min = 0, max = 1 } } } },
          },
        }
    )lua");
    EXPECT_TRUE(hasSingleError(result));
}

TEST(LuaGraphLoaderTest, NodeConfigRoundTripsNumberStringAndBoolean)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return {
          nodes = {
            { id = "tape", type = "TapeDelay",
              config = { baseDelayMs = 12.5, interpolation = "cubic", enabled = true } },
          },
        }
    )lua");
    ASSERT_TRUE(result.description.has_value());
    ASSERT_EQ(result.description->nodes.size(), 1u);
    const NodeInstance& instance = result.description->nodes[0];

    EXPECT_FLOAT_EQ(Nodes::Detail::configOrDefault(instance, "baseDelayMs", -1.0f), 12.5f);
    ASSERT_TRUE(instance.config.contains("interpolation"));
    EXPECT_EQ(instance.config.at("interpolation"), "cubic");
    ASSERT_TRUE(instance.config.contains("enabled"));
    EXPECT_EQ(instance.config.at("enabled"), "true");
}

TEST(LuaGraphLoaderTest, ScriptRuntimeErrorReportsItsLine)
{
    const LoadResult result = LuaGraphLoader::loadFromString("local a = 1\nlocal b = 2\nerror(\"boom\")\n");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_EQ(result.diagnostics.front().line, 3);
}

TEST(LuaGraphLoaderTest, ScriptSyntaxErrorReportsItsLine)
{
    const LoadResult result = LuaGraphLoader::loadFromString("local a = 1\nlocal b = = 2\n");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_EQ(result.diagnostics.front().line, 2);
}

TEST(LuaGraphLoaderTest, NonNumericParamNamesTheNodeFieldAndLine)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
return {
  nodes = {
    { id = "tape", type = "TapeDelay",
      params = {
        wowRate = 0.3,
        wowDepth = "loud",
      },
    },
  },
}
)lua");
    ASSERT_TRUE(hasSingleError(result));
    const Diagnostic& diagnostic = result.diagnostics.front();
    EXPECT_EQ(diagnostic.nodeId, "tape");
    EXPECT_EQ(diagnostic.field, "params.wowDepth");
    EXPECT_EQ(diagnostic.line, 7);
}

TEST(LuaGraphLoaderTest, NonScalarConfigValueNamesTheNodeFieldAndLine)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
return {
  nodes = {
    { id = "first", type = "TapeDelay" },
    { id = "tape", type = "TapeDelay",
      config = {
        interpolation = { "cubic" },
      },
    },
  },
}
)lua");
    ASSERT_TRUE(hasSingleError(result));
    const Diagnostic& diagnostic = result.diagnostics.front();
    EXPECT_EQ(diagnostic.nodeId, "tape");
    EXPECT_EQ(diagnostic.field, "config.interpolation");
    EXPECT_EQ(diagnostic.line, 7);
}

TEST(LuaGraphLoaderTest, StructuralErrorNamesItsTableIndexAsTheField)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return { nodes = { { id = "n1" } } }
    )lua");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_EQ(result.diagnostics.front().field, "nodes[1]");
}

TEST(LuaGraphLoaderTest, ValidatorDiagnosticGetsTheLineOfItsNodePortReference)
{
    constexpr std::string_view script = R"lua(
return {
  io = { inputs = { "inL" }, outputs = { "outL" } },
  nodes = {
    { id = "tape", type = "TapeDelay" },
  },
  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "tape.bogus", to = "outL" },
  },
}
)lua";
    const LoadResult result = LuaGraphLoader::loadFromString(script);
    ASSERT_TRUE(result.description.has_value());

    NodeRegistry registry;
    Nodes::registerTapeDelayNode<64>(registry);
    auto diagnostics = GraphValidator::validate(*result.description, registry);
    LuaGraphLoader::annotateSourceLines(diagnostics, script);

    const auto unknownPort = std::find_if(diagnostics.begin(), diagnostics.end(),
                                          [](const Diagnostic& d) { return d.message == "unknown port"; });
    ASSERT_NE(unknownPort, diagnostics.end());
    EXPECT_EQ(unknownPort->line, 9);
}

TEST(LuaGraphLoaderTest, DiagnosticWithoutAKnownNodeStaysAtLineZero)
{
    std::vector<Diagnostic> diagnostics{
        Diagnostic{DiagnosticSeverity::Error, "unknown graph source port", "", "x", "", 0},
        Diagnostic{DiagnosticSeverity::Error, "unknown node type", "ghost", "", "", 0}};
    LuaGraphLoader::annotateSourceLines(diagnostics, "return { nodes = { { id = \"real\", type = \"T\" } } }");
    EXPECT_EQ(diagnostics[0].line, 0);
    EXPECT_EQ(diagnostics[1].line, 0);
}

TEST(LuaGraphLoaderTest, IdLookupIgnoresAnotherKeyThatMerelyEndsInId)
{
    std::vector<Diagnostic> diagnostics{Diagnostic{DiagnosticSeverity::Error, "x", "tape", "", "", 0}};
    constexpr std::string_view script =
        "local grid = \"tape\"\nreturn { nodes = {\n{ id = \"tape\", type = \"T\" } } }";
    LuaGraphLoader::annotateSourceLines(diagnostics, script);
    EXPECT_EQ(diagnostics[0].line, 3);
}

TEST(LuaGraphLoaderTest, MacroTargetReadsMinMaxAndCurve)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return {
          macros = {
            { id = "m1", targets = {
                { to = "n1.gain", min = 0.1, max = 2.0, curve = "exp" },
                { to = "n1.pan" },
            } },
          },
        }
    )lua");
    ASSERT_TRUE(result.description.has_value());
    const auto& targets = result.description->macros.at(0).targets;
    ASSERT_EQ(targets.size(), 2u);
    ASSERT_TRUE(targets[0].minValue.has_value());
    EXPECT_FLOAT_EQ(*targets[0].minValue, 0.1f);
    EXPECT_FLOAT_EQ(*targets[0].maxValue, 2.0f);
    EXPECT_EQ(targets[0].curve, "exp");
    EXPECT_FALSE(targets[1].minValue.has_value());
    EXPECT_TRUE(targets[1].curve.empty());
}

TEST(LuaGraphLoaderTest, MacroTargetWithOnlyOneOfMinAndMaxIsAnError)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return { macros = { { id = "m1", targets = { { to = "n1.gain", min = 0.1 } } } } }
    )lua");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_EQ(result.diagnostics.front().nodeId, "n1");
    EXPECT_NE(result.diagnostics.front().message.find("both min and max"), std::string::npos);
}

TEST(LuaGraphLoaderTest, MacroTargetWithAnUnknownCurveIsAnError)
{
    const LoadResult result = LuaGraphLoader::loadFromString(R"lua(
        return { macros = { { id = "m1", targets = { { to = "n1.gain", curve = "wobbly" } } } } }
    )lua");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_NE(result.diagnostics.front().message.find("curve"), std::string::npos);
}

TEST(LuaGraphLoaderTest, ARunawayLoopIsStoppedByTheInstructionBudget)
{
    const LoadResult result = LuaGraphLoader::loadFromString("while true do end");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_NE(result.diagnostics.front().message.find("instruction budget"), std::string::npos);
}

TEST(LuaGraphLoaderTest, AMemoryBombIsStoppedByTheAllocationLimit)
{
    const LoadResult result = LuaGraphLoader::loadFromString("local big = string.rep('x', 2 ^ 27)\nreturn {}");
    ASSERT_TRUE(hasSingleError(result));
    EXPECT_NE(result.diagnostics.front().message.find("memory"), std::string::npos);
}

TEST(LuaGraphLoaderTest, TheBudgetResetsForEveryLoad)
{
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_TRUE(LuaGraphLoader::loadFromString("local n = 0 for i = 1, 100000 do n = n + i end return {}")
                        .description.has_value());
    }
}

}
