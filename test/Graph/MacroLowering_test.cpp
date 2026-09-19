#include <array>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/MacroBank.h"
#include "Graph/MacroLowering.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/ControlNodes.h"
#include "Graph/Nodes/MacroInput.h"
#include "GraphTestNodes.h"

namespace AbacDsp::Graph::Test
{
namespace
{

constexpr size_t kBlock{16};

// in -> g (GainStub, gain 0..4) -> out, with one macro "m" aimed at g.gain.
[[nodiscard]] GraphDescription makeGraph(MacroTarget target)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {NodeInstance{.id = "g", .type = "GainStub"}};
    description.edges = {Edge{.fromNode = "", .fromPort = "in", .toNode = "g", .toPort = "in"},
                         Edge{.fromNode = "g", .fromPort = "out", .toNode = "", .toPort = "out"}};
    description.macros = {Macro{.id = "m", .label = "Amount", .defaultValue = 0.25f, .targets = {std::move(target)}}};
    return description;
}

class LoweringFixture : public ::testing::Test
{
  protected:
    LoweringFixture()
    {
        registerTestNodes(m_registry);
        Nodes::registerControlNodes(m_registry);
        Nodes::registerMacroInputNode(m_registry, m_bank);
    }

    [[nodiscard]] LoweredGraph lower(const GraphDescription& description)
    {
        return MacroLowering::lower(description, m_registry, std::array<std::string, 1>{"m"}, 1);
    }

    [[nodiscard]] CompiledGraph compile(const GraphDescription& description)
    {
        auto result = GraphCompiler::compile(description, m_registry, kBlock, 48000.f);
        EXPECT_TRUE(result.graph.has_value());
        return std::move(*result.graph);
    }

    // Level of the gain stage for a constant input of 1.
    [[nodiscard]] static float gainOf(CompiledGraph& graph)
    {
        std::array<float, kBlock> in{};
        in.fill(1.f);
        std::array<float, kBlock> out{};
        std::array<const float*, 1> inputs{in.data()};
        std::array<float*, 1> outputs{out.data()};
        graph.process(inputs, outputs, kBlock);
        return out[kBlock - 1];
    }

    MacroBank m_bank;
    NodeRegistry m_registry;
};

} // namespace

TEST_F(LoweringFixture, WithoutMinAndMaxTheMacroSweepsTheSchemaRange)
{
    const auto lowered = lower(makeGraph(MacroTarget{.toNode = "g", .toParam = "gain"}));
    ASSERT_EQ(lowered.macros.size(), 1u);
    EXPECT_TRUE(lowered.diagnostics.empty());
    auto graph = compile(lowered.description);

    m_bank.set(0, 0.f);
    EXPECT_FLOAT_EQ(gainOf(graph), 0.f);
    m_bank.set(0, 0.5f);
    EXPECT_FLOAT_EQ(gainOf(graph), 2.f);
    m_bank.set(0, 1.f);
    EXPECT_FLOAT_EQ(gainOf(graph), 4.f);
}

TEST_F(LoweringFixture, ExplicitMinAndMaxSetTheEndpoints)
{
    const auto lowered =
        lower(makeGraph(MacroTarget{.toNode = "g", .toParam = "gain", .minValue = 1.f, .maxValue = 3.f}));
    auto graph = compile(lowered.description);
    m_bank.set(0, 0.f);
    EXPECT_FLOAT_EQ(gainOf(graph), 1.f);
    m_bank.set(0, 0.5f);
    EXPECT_FLOAT_EQ(gainOf(graph), 2.f);
}

TEST_F(LoweringFixture, ExpCurveTravelsGeometricallyBetweenTheEndpoints)
{
    const auto lowered = lower(
        makeGraph(MacroTarget{.toNode = "g", .toParam = "gain", .minValue = 0.5f, .maxValue = 2.f, .curve = "exp"}));
    auto graph = compile(lowered.description);
    m_bank.set(0, 0.f);
    EXPECT_NEAR(gainOf(graph), 0.5f, 1E-5f);
    m_bank.set(0, 0.5f);
    EXPECT_NEAR(gainOf(graph), 1.f, 1E-5f);
    m_bank.set(0, 1.f);
    EXPECT_NEAR(gainOf(graph), 2.f, 1E-5f);
}

TEST_F(LoweringFixture, ExpCurveWithANonPositiveEndpointFallsBackToLinearWithAWarning)
{
    const auto lowered = lower(
        makeGraph(MacroTarget{.toNode = "g", .toParam = "gain", .minValue = 0.f, .maxValue = 2.f, .curve = "exp"}));
    ASSERT_EQ(lowered.macros.size(), 1u);
    ASSERT_EQ(lowered.diagnostics.size(), 1u);
    EXPECT_EQ(lowered.diagnostics[0].severity, DiagnosticSeverity::Warning);
    auto graph = compile(lowered.description);
    m_bank.set(0, 0.5f);
    EXPECT_FLOAT_EQ(gainOf(graph), 1.f);
}

TEST_F(LoweringFixture, UnknownNodeOrParameterSkipsTheTargetAndDropsAMacroWithNoTargetLeft)
{
    for (const auto& target :
         {MacroTarget{.toNode = "ghost", .toParam = "gain"}, MacroTarget{.toNode = "g", .toParam = "nope"}})
    {
        const auto lowered = lower(makeGraph(target));
        EXPECT_TRUE(lowered.macros.empty());
        ASSERT_EQ(lowered.diagnostics.size(), 2u);
        EXPECT_EQ(lowered.diagnostics[0].severity, DiagnosticSeverity::Warning);
        EXPECT_EQ(lowered.description.nodes.size(), 1u);
    }
}

TEST_F(LoweringFixture, ReservedIdsTakeTheirSlotAndOthersFillFromTheFirstFreeOne)
{
    auto description = makeGraph(MacroTarget{.toNode = "g", .toParam = "gain"});
    description.macros.push_back(Macro{.id = "extra", .targets = {MacroTarget{.toNode = "g", .toParam = "gain"}}});
    const auto lowered = MacroLowering::lower(description, m_registry, std::array<std::string, 3>{"a", "b", "m"}, 3);
    ASSERT_EQ(lowered.macros.size(), 2u);
    EXPECT_EQ(lowered.macros[0].slot, 2u);
    EXPECT_EQ(lowered.macros[1].slot, 3u);
    EXPECT_EQ(lowered.macros[0].label, "Amount");
    EXPECT_EQ(lowered.macros[1].label, "extra");
}

TEST_F(LoweringFixture, MoreMacrosThanFreeSlotsAreIgnoredWithAWarning)
{
    GraphDescription description = makeGraph(MacroTarget{.toNode = "g", .toParam = "gain"});
    description.macros.clear();
    for (size_t i = 0; i < MacroBank::kSlotCount + 2; ++i)
    {
        description.macros.push_back(
            Macro{.id = "k" + std::to_string(i), .targets = {MacroTarget{.toNode = "g", .toParam = "gain"}}});
    }
    const auto lowered = MacroLowering::lower(description, m_registry, {}, 0);
    EXPECT_EQ(lowered.macros.size(), MacroBank::kSlotCount);
    EXPECT_EQ(lowered.diagnostics.size(), 2u);
}

TEST_F(LoweringFixture, WithoutTheRequiredNodeTypesMacrosAreIgnoredWithOneWarning)
{
    NodeRegistry bare;
    registerTestNodes(bare);
    const auto lowered = MacroLowering::lower(makeGraph(MacroTarget{.toNode = "g", .toParam = "gain"}), bare,
                                              std::array<std::string, 1>{"m"}, 1);
    EXPECT_TRUE(lowered.macros.empty());
    ASSERT_EQ(lowered.diagnostics.size(), 1u);
    EXPECT_EQ(lowered.description.nodes.size(), 1u);
}

TEST_F(LoweringFixture, ADescriptionWithoutMacrosPassesThroughUnchanged)
{
    GraphDescription description = makeGraph(MacroTarget{.toNode = "g", .toParam = "gain"});
    description.macros.clear();
    const auto lowered = lower(description);
    EXPECT_EQ(lowered.description.nodes.size(), description.nodes.size());
    EXPECT_TRUE(lowered.diagnostics.empty());
}

TEST_F(LoweringFixture, TwoTargetsOfOneMacroMoveTogether)
{
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {NodeInstance{.id = "a", .type = "GainStub"}, NodeInstance{.id = "b", .type = "GainStub"}};
    description.edges = {Edge{.fromNode = "", .fromPort = "in", .toNode = "a", .toPort = "in"},
                         Edge{.fromNode = "a", .fromPort = "out", .toNode = "b", .toPort = "in"},
                         Edge{.fromNode = "b", .fromPort = "out", .toNode = "", .toPort = "out"}};
    description.macros = {
        Macro{.id = "m",
              .targets = {MacroTarget{.toNode = "a", .toParam = "gain", .minValue = 0.f, .maxValue = 2.f},
                          MacroTarget{.toNode = "b", .toParam = "gain", .minValue = 0.f, .maxValue = 2.f}}}};
    auto graph = compile(lower(description).description);
    m_bank.set(0, 0.5f);
    EXPECT_FLOAT_EQ(gainOf(graph), 1.f);
    m_bank.set(0, 1.f);
    EXPECT_FLOAT_EQ(gainOf(graph), 4.f);
}

}
