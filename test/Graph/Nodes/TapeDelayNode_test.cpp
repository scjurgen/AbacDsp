#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/Nodes/TapeDelayNode.h"

namespace AbacDsp::Graph::Nodes::Test
{
namespace
{

constexpr size_t kBlockSize = 64;
constexpr float kSampleRate = 48000.f;
using Node = TapeDelayNode<kBlockSize>;

[[nodiscard]] Node makeNode()
{
    return Node(kSampleRate, 8.0f, 250.0f);
}

struct Block
{
    std::array<float, kBlockSize> inL{};
    std::array<float, kBlockSize> inR{};
    std::array<float, kBlockSize> feedbackL{};
    std::array<float, kBlockSize> feedbackR{};
    std::array<float, kBlockSize> outL{};
    std::array<float, kBlockSize> outR{};
};

void processOneBlock(Node& node, Block& block)
{
    std::array<const float*, 4> ins{block.inL.data(), block.inR.data(), block.feedbackL.data(), block.feedbackR.data()};
    std::array<float*, 2> outs{block.outL.data(), block.outR.data()};
    node.process(ins, outs, kBlockSize);
}

// Random wow only: OU variance is what a seed changes.
[[nodiscard]] std::vector<float> renderRandomWow(Node& node, const size_t numBlocks)
{
    node.setParameter(1, 1.0f); // wowDepth
    node.setParameter(3, 1.0f); // wowVariance
    std::vector<float> left;
    for (size_t b = 0; b < numBlocks; ++b)
    {
        Block block;
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            const float phase = static_cast<float>(b * kBlockSize + i) * 440.f / kSampleRate;
            block.inL[i] = block.inR[i] = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * phase);
        }
        processOneBlock(node, block);
        left.insert(left.end(), block.outL.begin(), block.outL.end());
    }
    return left;
}

} // namespace

TEST(TapeDelayNodeTest, DefaultConstructionProducesFiniteOutput)
{
    Node node = makeNode();

    for (int b = 0; b < 50; ++b)
    {
        Block block;
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            const float phase = static_cast<float>(b * kBlockSize + i) * 440.f / kSampleRate;
            const float sample = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * phase);
            block.inL[i] = sample;
            block.inR[i] = sample;
        }
        processOneBlock(node, block);

        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(block.outL[i]));
            ASSERT_TRUE(std::isfinite(block.outR[i]));
        }
    }
}

TEST(TapeDelayNodeTest, WowDepthParameterChangesOutput)
{
    // wowVariance/wowDrift forced to 0 on both nodes: sigma=0 makes
    // OrnsteinUhlenbeckProcess deterministically zero regardless of seed, so
    // this isolates wowDepth's own effect from two independently-seeded RNGs.
    Node nodeDefault = makeNode();
    nodeDefault.setParameter(3, 0.0f);
    nodeDefault.setParameter(4, 0.0f);

    Node nodeHighDepth = makeNode();
    nodeHighDepth.setParameter(1, 1.0f);
    nodeHighDepth.setParameter(3, 0.0f);
    nodeHighDepth.setParameter(4, 0.0f);

    bool sawDifference = false;
    for (int b = 0; b < 30 && !sawDifference; ++b)
    {
        Block blockA;
        Block blockB;
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            const float phase = static_cast<float>(b * kBlockSize + i) * 440.f / kSampleRate;
            const float sample = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> * phase);
            blockA.inL[i] = blockA.inR[i] = sample;
            blockB.inL[i] = blockB.inR[i] = sample;
        }
        processOneBlock(nodeDefault, blockA);
        processOneBlock(nodeHighDepth, blockB);

        for (size_t i = 0; i < kBlockSize; ++i)
        {
            if (std::abs(blockA.outL[i] - blockB.outL[i]) > 1e-6f)
            {
                sawDifference = true;
                break;
            }
        }
    }
    EXPECT_TRUE(sawDifference);
}

TEST(TapeDelayNodeTest, FeedbackPortsReachTheWritePath)
{
    Node node = makeNode();
    node.setParameter(3, 0.0f); // wowVariance
    node.setParameter(4, 0.0f); // wowDrift

    bool sawNonzeroOutput = false;
    for (int b = 0; b < 30; ++b)
    {
        Block block;
        if (b == 0)
        {
            block.feedbackL.fill(1.0f);
            block.feedbackR.fill(1.0f);
        }
        processOneBlock(node, block);

        for (size_t i = 0; i < kBlockSize; ++i)
        {
            if (std::abs(block.outL[i]) > 1e-6f || std::abs(block.outR[i]) > 1e-6f)
            {
                sawNonzeroOutput = true;
            }
        }
    }
    EXPECT_TRUE(sawNonzeroOutput);
}

TEST(TapeDelayNodeTest, SilentInputAndFeedbackStaySilent)
{
    Node node = makeNode();

    for (int b = 0; b < 10; ++b)
    {
        Block block;
        processOneBlock(node, block);

        for (size_t i = 0; i < kBlockSize; ++i)
        {
            EXPECT_FLOAT_EQ(block.outL[i], 0.0f);
            EXPECT_FLOAT_EQ(block.outR[i], 0.0f);
        }
    }
}

TEST(TapeDelayNodeTest, SameSeedGivesIdenticalRandomWowAndDifferentSeedsDiverge)
{
    constexpr size_t kBlocks = 400;
    Node first(kSampleRate, 8.0f, 250.0f, 7);
    Node second(kSampleRate, 8.0f, 250.0f, 7);
    Node other(kSampleRate, 8.0f, 250.0f, 8);
    const auto renderedFirst = renderRandomWow(first, kBlocks);
    EXPECT_EQ(renderedFirst, renderRandomWow(second, kBlocks));
    EXPECT_NE(renderedFirst, renderRandomWow(other, kBlocks));
}

TEST(TapeDelayNodeTest, DefaultSeedIsOne)
{
    constexpr size_t kBlocks = 400;
    Node implicitSeed = makeNode();
    Node explicitSeed(kSampleRate, 8.0f, 250.0f, 1);
    EXPECT_EQ(renderRandomWow(implicitSeed, kBlocks), renderRandomWow(explicitSeed, kBlocks));
}

TEST(TapeDelayNodeTest, RegisteredNodeReadsTheSeedFromItsConfig)
{
    constexpr size_t kBlocks = 400;
    NodeRegistry registry;
    registerTapeDelayNode<kBlockSize>(registry);

    NodeInstance seeded{.id = "t", .type = "TapeDelay", .config = {{"seed", "7"}}};
    NodeInstance plain{.id = "t", .type = "TapeDelay"};
    auto fromConfig = registry.create("TapeDelay", seeded, kSampleRate);
    auto fromDefault = registry.create("TapeDelay", plain, kSampleRate);
    Node direct(kSampleRate, 8.0f, 250.0f, 7);

    auto& configured = static_cast<Node&>(*fromConfig);
    auto& defaulted = static_cast<Node&>(*fromDefault);
    const auto expected = renderRandomWow(direct, kBlocks);
    EXPECT_EQ(renderRandomWow(configured, kBlocks), expected);
    EXPECT_NE(renderRandomWow(defaulted, kBlocks), expected);
}

TEST(TapeDelayNodeTest, SchemaDefaultsDescribeAnUnmodulatedNode)
{
    NodeRegistry registry;
    registerTapeDelayNode<kBlockSize>(registry);
    const auto* schema = registry.findSchema("TapeDelay");
    ASSERT_NE(schema, nullptr);
    for (const std::string id : {"wowDepth", "wowVariance", "wowDrift", "flutterDepth"})
    {
        const int index = schema->findParameterIndex(id);
        ASSERT_GE(index, 0) << id;
        EXPECT_EQ(schema->parameters[static_cast<size_t>(index)].defaultValue, 0.f) << id;
    }
}

TEST(TapeDelayNodeTest, WithNoParametersSetTheSeedHasNothingToChange)
{
    constexpr size_t kBlocks = 200;
    Node first(kSampleRate, 8.0f, 250.0f, 3);
    Node second(kSampleRate, 8.0f, 250.0f, 4);
    std::vector<float> a;
    std::vector<float> b;
    for (size_t block = 0; block < kBlocks; ++block)
    {
        Block one;
        Block two;
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            const float phase = static_cast<float>(block * kBlockSize + i) * 440.f / kSampleRate;
            one.inL[i] = one.inR[i] = two.inL[i] = two.inR[i] =
                0.5f * std::sin(2.0f * std::numbers::pi_v<float> * phase);
        }
        processOneBlock(first, one);
        processOneBlock(second, two);
        a.insert(a.end(), one.outL.begin(), one.outL.end());
        b.insert(b.end(), two.outL.begin(), two.outL.end());
    }
    EXPECT_EQ(a, b);
}

}
