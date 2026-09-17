#include <array>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/StandardNodes.h"

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

TEST(StandardNodesIntegrationTest, StereoToMonoGainMonoToStereoChainCompilesAndRuns)
{
    NodeRegistry registry;
    registerStandardNodes(registry);

    GraphDescription description;
    description.io = {{"inL", "inR"}, {"outL", "outR"}};
    description.nodes = {
        makeNode("s2m", "StereoToMono"),
        makeNode("g", "Gain"),
        makeNode("m2s", "MonoToStereo"),
    };
    description.edges = {
        makeEdge("", "inL", "s2m", "inL"),   makeEdge("", "inR", "s2m", "inR"),  makeEdge("s2m", "out", "g", "inL"),
        makeEdge("s2m", "out", "g", "inR"),  makeEdge("g", "outL", "m2s", "in"), makeEdge("m2s", "outL", "", "outL"),
        makeEdge("m2s", "outR", "", "outR"),
    };

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const float inL = 3.f;
    const float inR = 1.f;
    float outL = 0.f;
    float outR = 0.f;
    std::array<const float*, 2> ins{&inL, &inR};
    std::array<float*, 2> outs{&outL, &outR};

    result.graph->process(ins, outs, 1);

    EXPECT_FLOAT_EQ(outL, 2.f);
    EXPECT_FLOAT_EQ(outR, 2.f);
}

TEST(StandardNodesIntegrationTest, MsEncodeThenMsDecodeReconstructsInput)
{
    NodeRegistry registry;
    registerStandardNodes(registry);

    GraphDescription description;
    description.io = {{"inL", "inR"}, {"outL", "outR"}};
    description.nodes = {
        makeNode("enc", "MS_Encode"),
        makeNode("dec", "MS_Decode"),
    };
    description.edges = {
        makeEdge("", "inL", "enc", "inL"),     makeEdge("", "inR", "enc", "inR"),
        makeEdge("enc", "outM", "dec", "inM"), makeEdge("enc", "outS", "dec", "inS"),
        makeEdge("dec", "outL", "", "outL"),   makeEdge("dec", "outR", "", "outR"),
    };

    auto result = GraphCompiler::compile(description, registry, 1, 48000.f);
    ASSERT_TRUE(result.graph.has_value());

    const float inL = 1.f;
    const float inR = 0.2f;
    float outL = 0.f;
    float outR = 0.f;
    std::array<const float*, 2> ins{&inL, &inR};
    std::array<float*, 2> outs{&outL, &outR};

    result.graph->process(ins, outs, 1);

    EXPECT_NEAR(outL, inL, 1e-6f);
    EXPECT_NEAR(outR, inR, 1e-6f);
}

}
