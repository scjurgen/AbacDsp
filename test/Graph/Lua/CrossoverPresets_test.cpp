#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/Lua/LuaGraphLoader.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/FilterNodes.h"
#include "Graph/Nodes/StandardNodes.h"
#include "Graph/Nodes/TapeDelayNode.h"

namespace AbacDsp::Graph::Test
{
namespace
{

constexpr size_t kBlockSize = 64;
constexpr float kSampleRate = 48000.f;

[[nodiscard]] NodeRegistry makeRegistry()
{
    NodeRegistry registry;
    Nodes::registerStandardNodes(registry);
    Nodes::registerFilterNodes(registry);
    Nodes::registerTapeDelayNode<kBlockSize>(registry);
    return registry;
}

// Preserve stereo low band: low band bypasses TapeDelay entirely (dry), high
// band goes through it, Mixer recombines - the untouched low path is what
// makes this policy distinct from "mono low band" below.
constexpr std::string_view kPreserveStereoLowBand = R"lua(
return {
  version = 1,
  name = "Preserve Stereo Low Band",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "xoL", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "xoR", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "tape", type = "TapeDelay" },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "xoL.in" },
    { from = "inR", to = "xoR.in" },
    { from = "xoL.highOut", to = "tape.inL" },
    { from = "xoR.highOut", to = "tape.inR" },
    { from = "tape.outL", to = "mix.in1L" },
    { from = "tape.outR", to = "mix.in1R" },
    { from = "xoL.lowOut", to = "mix.in2L" },
    { from = "xoR.lowOut", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
)lua";

// Mono low band: same split, but the low band is folded to mono and
// duplicated back to both channels before recombining.
constexpr std::string_view kMonoLowBand = R"lua(
return {
  version = 1,
  name = "Mono Low Band",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "xoL", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "xoR", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "lowMono", type = "StereoToMono" },
    { id = "lowDup", type = "MonoToStereo" },
    { id = "tape", type = "TapeDelay" },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "xoL.in" },
    { from = "inR", to = "xoR.in" },
    { from = "xoL.lowOut", to = "lowMono.inL" },
    { from = "xoR.lowOut", to = "lowMono.inR" },
    { from = "lowMono.out", to = "lowDup.in" },
    { from = "xoL.highOut", to = "tape.inL" },
    { from = "xoR.highOut", to = "tape.inR" },
    { from = "tape.outL", to = "mix.in1L" },
    { from = "tape.outR", to = "mix.in1R" },
    { from = "lowDup.outL", to = "mix.in2L" },
    { from = "lowDup.outR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
)lua";

// Wet-only low cut: no crossover node at all - full dry stays untouched, only
// the wet (post-TapeDelay) bus is high-passed before mixing back with dry.
constexpr std::string_view kWetOnlyLowCut = R"lua(
return {
  version = 1,
  name = "Wet Only Low Cut",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "tape", type = "TapeDelay" },
    { id = "wetHpL", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "wetHpR", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "wetHpL.in" },
    { from = "tape.outR", to = "wetHpR.in" },
    { from = "wetHpL.out", to = "mix.in1L" },
    { from = "wetHpR.out", to = "mix.in1R" },
    { from = "inL", to = "mix.in2L" },
    { from = "inR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
)lua";

struct StereoRun
{
    std::vector<float> outL;
    std::vector<float> outR;
};

[[nodiscard]] StereoRun runStereo(CompiledGraph& graph, const std::vector<float>& inL, const std::vector<float>& inR)
{
    StereoRun run;
    run.outL.resize(inL.size());
    run.outR.resize(inL.size());
    for (size_t block = 0; block * kBlockSize < inL.size(); ++block)
    {
        const size_t offset = block * kBlockSize;
        std::array<const float*, 2> ins{inL.data() + offset, inR.data() + offset};
        std::array<float*, 2> outs{run.outL.data() + offset, run.outR.data() + offset};
        graph.process(ins, outs, kBlockSize);
    }
    return run;
}

[[nodiscard]] std::vector<float> sineTone(const float frequencyHz, const float amplitude, const size_t numSamples)
{
    std::vector<float> tone(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        tone[i] =
            amplitude * std::sin(2.f * std::numbers::pi_v<float> * frequencyHz * static_cast<float>(i) / kSampleRate);
    }
    return tone;
}

[[nodiscard]] float channelDifferenceRms(const StereoRun& run, const size_t from)
{
    float sumSquares = 0.f;
    for (size_t i = from; i < run.outL.size(); ++i)
    {
        const float diff = run.outL[i] - run.outR[i];
        sumSquares += diff * diff;
    }
    return std::sqrt(sumSquares / static_cast<float>(run.outL.size() - from));
}

} // namespace

TEST(CrossoverPresetsTest, AllThreePresetsCompileAndValidateWithoutErrors)
{
    const auto registry = makeRegistry();
    for (const std::string_view lua : {kPreserveStereoLowBand, kMonoLowBand, kWetOnlyLowCut})
    {
        const auto loaded = Lua::LuaGraphLoader::loadFromString(lua);
        ASSERT_TRUE(loaded.description.has_value());
        const auto compiled = GraphCompiler::compile(*loaded.description, registry, kBlockSize, kSampleRate);
        EXPECT_TRUE(compiled.graph.has_value());
    }
}

// Proof the two stereo-crossover policies are not aliases: "preserve stereo"
// keeps decorrelated L/R content far more different than "mono low band" does.
TEST(CrossoverPresetsTest, PreserveStereoKeepsMoreChannelSeparationThanMonoLowBand)
{
    const auto registry = makeRegistry();
    constexpr size_t kNumSamples = 12 * kBlockSize * 64;
    const auto toneL = sineTone(30.f, 0.5f, kNumSamples);
    const std::vector<float> silence(kNumSamples, 0.f);
    constexpr size_t kMeasureFrom = kNumSamples / 2;

    const auto preserveLoaded = Lua::LuaGraphLoader::loadFromString(kPreserveStereoLowBand);
    ASSERT_TRUE(preserveLoaded.description.has_value());
    auto preserveCompiled = GraphCompiler::compile(*preserveLoaded.description, registry, kBlockSize, kSampleRate);
    ASSERT_TRUE(preserveCompiled.graph.has_value());
    const float preserveDiff = channelDifferenceRms(runStereo(*preserveCompiled.graph, toneL, silence), kMeasureFrom);

    const auto monoLoaded = Lua::LuaGraphLoader::loadFromString(kMonoLowBand);
    ASSERT_TRUE(monoLoaded.description.has_value());
    auto monoCompiled = GraphCompiler::compile(*monoLoaded.description, registry, kBlockSize, kSampleRate);
    ASSERT_TRUE(monoCompiled.graph.has_value());
    const float monoDiff = channelDifferenceRms(runStereo(*monoCompiled.graph, toneL, silence), kMeasureFrom);

    EXPECT_GT(preserveDiff, monoDiff * 2.f);
}

TEST(CrossoverPresetsTest, AllThreePresetsProduceFiniteOutputForFullSpectrumContent)
{
    const auto registry = makeRegistry();
    constexpr size_t kNumSamples = 4 * kBlockSize * 64;
    const auto lowTone = sineTone(80.f, 0.3f, kNumSamples);
    const auto highTone = sineTone(4000.f, 0.3f, kNumSamples);
    std::vector<float> mixed(kNumSamples);
    for (size_t i = 0; i < kNumSamples; ++i)
    {
        mixed[i] = lowTone[i] + highTone[i];
    }

    for (const std::string_view lua : {kPreserveStereoLowBand, kMonoLowBand, kWetOnlyLowCut})
    {
        const auto loaded = Lua::LuaGraphLoader::loadFromString(lua);
        ASSERT_TRUE(loaded.description.has_value());
        auto compiled = GraphCompiler::compile(*loaded.description, registry, kBlockSize, kSampleRate);
        ASSERT_TRUE(compiled.graph.has_value());

        const auto run = runStereo(*compiled.graph, mixed, mixed);
        for (const float sample : run.outL)
        {
            ASSERT_TRUE(std::isfinite(sample));
        }
        for (const float sample : run.outR)
        {
            ASSERT_TRUE(std::isfinite(sample));
        }
    }
}

}
