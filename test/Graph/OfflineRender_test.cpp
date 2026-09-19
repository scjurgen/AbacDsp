#include <cmath>
#include <numeric>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/FilterNodes.h"
#include "Graph/OfflineRender.h"

namespace AbacDsp::Graph::Test
{
namespace
{

constexpr float kSampleRate{48000.f};

[[nodiscard]] size_t countZeroCrossings(const std::vector<float>& signal, const size_t from, const size_t to)
{
    size_t crossings = 0;
    for (size_t i = from + 1; i < to; ++i)
    {
        crossings += (signal[i - 1] < 0.f) != (signal[i] < 0.f) ? 1 : 0;
    }
    return crossings;
}

// One OnePoleLP: stateful, so a wrong block hand-over would show as a mismatch.
[[nodiscard]] CompiledGraph makeLowPassGraph()
{
    NodeRegistry registry;
    Nodes::registerFilterNodes(registry);
    GraphDescription description;
    description.io = {{"in"}, {"out"}};
    description.nodes = {NodeInstance{.id = "lp", .type = "OnePoleLP", .params = {{"cutoffHz", 500.f}}}};
    description.edges = {Edge{.fromNode = "", .fromPort = "in", .toNode = "lp", .toPort = "in"},
                         Edge{.fromNode = "lp", .fromPort = "out", .toNode = "", .toPort = "out"}};
    auto result = GraphCompiler::compile(description, registry, 64, kSampleRate);
    EXPECT_TRUE(result.graph.has_value());
    return std::move(*result.graph);
}

} // namespace

TEST(OfflineRenderTest, ImpulseIsASingleSampleAtTheStart)
{
    const auto impulse = OfflineRender::stimulus({.kind = Stimulus::Impulse, .amplitude = 0.5f}, 100, kSampleRate);
    ASSERT_EQ(impulse.size(), 100u);
    EXPECT_EQ(impulse[0], 0.5f);
    EXPECT_EQ(std::accumulate(impulse.begin() + 1, impulse.end(), 0.f), 0.f);
    EXPECT_TRUE(OfflineRender::stimulus({.kind = Stimulus::Impulse}, 0, kSampleRate).empty());
}

TEST(OfflineRenderTest, SineHasTheRequestedFrequencyAndAmplitude)
{
    const auto tone = OfflineRender::stimulus({.kind = Stimulus::Sine, .amplitude = 0.25f, .frequencyHz = 1000.f},
                                              static_cast<size_t>(kSampleRate), kSampleRate);
    EXPECT_EQ(tone[0], 0.f);
    EXPECT_NEAR(tone[12], 0.25f, 1E-6f);
    EXPECT_NEAR(*std::ranges::max_element(tone), 0.25f, 1E-4f);
    EXPECT_NEAR(static_cast<double>(countZeroCrossings(tone, 0, tone.size())), 2000.0, 2.0);
}

TEST(OfflineRenderTest, SweepStartsAtTheLowFrequencyAndEndsAtTheHighOne)
{
    const auto sweep = OfflineRender::stimulus(
        {.kind = Stimulus::LogSweep, .amplitude = 1.f, .frequencyHz = 100.f, .endFrequencyHz = 10000.f},
        static_cast<size_t>(kSampleRate), kSampleRate);

    // 100 Hz rising to 100 * 100^0.05 = 126 Hz in the first 50 ms: 10 to 12.6 crossings.
    const auto early = countZeroCrossings(sweep, 0, 2400);
    EXPECT_GE(early, 9u);
    EXPECT_LE(early, 13u);

    // 9120 to 10000 Hz in the last 10 ms: 182 to 200 crossings.
    const auto late = countZeroCrossings(sweep, sweep.size() - 480, sweep.size());
    EXPECT_GE(late, 178u);
    EXPECT_LE(late, 202u);

    EXPECT_LE(*std::ranges::max_element(sweep), 1.f);
    EXPECT_GE(*std::ranges::min_element(sweep), -1.f);
}

TEST(OfflineRenderTest, NoiseIsRepeatableBoundedAndCentred)
{
    const StimulusSpec spec{.kind = Stimulus::WhiteNoise, .amplitude = 0.5f, .seed = 7};
    const auto first = OfflineRender::stimulus(spec, 48000, kSampleRate);
    const auto second = OfflineRender::stimulus(spec, 48000, kSampleRate);
    EXPECT_EQ(first, second);

    StimulusSpec other = spec;
    other.seed = 8;
    EXPECT_NE(first, OfflineRender::stimulus(other, 48000, kSampleRate));

    EXPECT_GE(*std::ranges::min_element(first), -0.5f);
    EXPECT_LT(*std::ranges::max_element(first), 0.5f);
    const double mean = std::accumulate(first.begin(), first.end(), 0.0) / static_cast<double>(first.size());
    EXPECT_NEAR(mean, 0.0, 0.01);
    double sumSquares = 0.0;
    for (const float sample : first)
    {
        sumSquares += static_cast<double>(sample) * sample;
    }
    EXPECT_NEAR(std::sqrt(sumSquares / static_cast<double>(first.size())), 0.5 / std::sqrt(3.0), 0.01);
}

TEST(OfflineRenderTest, OutputLengthMatchesAndBlockSizeDoesNotChangeTheResult)
{
    constexpr size_t kNumSamples{1000};
    const std::vector<std::vector<float>> input{
        OfflineRender::stimulus({.kind = Stimulus::WhiteNoise, .amplitude = 1.f}, kNumSamples, kSampleRate)};

    auto reference = makeLowPassGraph();
    const auto expected = OfflineRender::render(reference, input, kNumSamples, 64);
    ASSERT_EQ(expected.size(), 1u);
    ASSERT_EQ(expected[0].size(), kNumSamples);

    for (const size_t blockSize : {size_t{1}, size_t{7}, size_t{37}, size_t{64}})
    {
        auto graph = makeLowPassGraph();
        const auto rendered = OfflineRender::render(graph, input, kNumSamples, blockSize);
        ASSERT_EQ(rendered[0].size(), kNumSamples) << "block " << blockSize;
        for (size_t i = 0; i < kNumSamples; ++i)
        {
            ASSERT_NEAR(rendered[0][i], expected[0][i], 1E-6f) << "block " << blockSize << " sample " << i;
        }
    }
}

TEST(OfflineRenderTest, ZeroLengthRenderYieldsEmptyOutputs)
{
    auto graph = makeLowPassGraph();
    const auto rendered = OfflineRender::render(graph, {{}}, 0, 64);
    ASSERT_EQ(rendered.size(), 1u);
    EXPECT_TRUE(rendered[0].empty());
}

}
