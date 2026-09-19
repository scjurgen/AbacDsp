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
#include "Graph/OfflineRender.h"
#include "Graph/Presets/ChorusPresets.h"

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
constexpr std::string_view kPreserveStereoLowBand = Presets::kCrossoverPreserveStereo;

// Mono low band: same split, but the low band is folded to mono and
// duplicated back to both channels before recombining.
constexpr std::string_view kMonoLowBand = Presets::kCrossoverMonoLow;

// Wet-only low cut: no crossover node at all - full dry stays untouched, only
// the wet (post-TapeDelay) bus is high-passed before mixing back with dry.
constexpr std::string_view kWetOnlyLowCut = Presets::kCrossoverWetOnlyLowCut;

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

// Measured worst error over the test signals is 1.2e-7; the limit leaves about 8x headroom.
constexpr float kFloatRoundingTolerance{1E-6f};
// Measured change of the channel difference is 0.89 for the mono-low preset.
constexpr float kMinChannelChange{0.5f};

// Same ports and schema as TapeDelay, but unit gain and no latency: with the wet path a known
// identity, what is left to check is the crossover topology itself.
[[nodiscard]] PortDescriptor makePort(std::string name, const PortDirection direction)
{
    return PortDescriptor{.name = std::move(name), .direction = direction, .category = PortCategory::AudioMono};
}

class IdentityTapeDelayNode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::copy_n(inputs[0], numSamples, outputs[0]);
        std::copy_n(inputs[1], numSamples, outputs[1]);
    }
};

[[nodiscard]] NodeRegistry makeIdentityWetRegistry()
{
    NodeRegistry registry;
    Nodes::registerStandardNodes(registry);
    Nodes::registerFilterNodes(registry);
    registry.registerType(
        "TapeDelay",
        NodeSchema{{makePort("inL", PortDirection::Input), makePort("inR", PortDirection::Input),
                    makePort("feedbackL", PortDirection::Input), makePort("feedbackR", PortDirection::Input),
                    makePort("outL", PortDirection::Output), makePort("outR", PortDirection::Output)},
                   {},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<IdentityTapeDelayNode>(); });
    return registry;
}

[[nodiscard]] CompiledGraph compilePreset(const std::string_view lua)
{
    const auto loaded = Lua::LuaGraphLoader::loadFromString(lua);
    EXPECT_TRUE(loaded.description.has_value());
    auto compiled = GraphCompiler::compile(*loaded.description, makeIdentityWetRegistry(), kBlockSize, kSampleRate);
    EXPECT_TRUE(compiled.graph.has_value());
    return std::move(*compiled.graph);
}

[[nodiscard]] float maxAbsDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    float worst = 0.f;
    for (size_t i = 0; i < a.size(); ++i)
    {
        worst = std::max(worst, std::abs(a[i] - b[i]));
    }
    return worst;
}

// Different content per channel and per test signal, so a crossed or dropped channel shows up.
[[nodiscard]] std::vector<std::vector<float>> stereoStimulus(const Stimulus leftKind, const Stimulus rightKind,
                                                             const size_t numSamples)
{
    const StimulusSpec left{.kind = leftKind, .amplitude = 0.5f, .frequencyHz = 20.f, .endFrequencyHz = 20000.f};
    const StimulusSpec right{
        .kind = rightKind, .amplitude = 0.4f, .frequencyHz = 30.f, .endFrequencyHz = 15000.f, .seed = 3};
    return {OfflineRender::stimulus(left, numSamples, kSampleRate),
            OfflineRender::stimulus(right, numSamples, kSampleRate)};
}

// Signal pairs to check with: an impulse, two sweeps, and noise against a sweep.
constexpr std::array<std::pair<Stimulus, Stimulus>, 4> kStimulusPairs{
    std::pair{Stimulus::Impulse, Stimulus::Impulse}, std::pair{Stimulus::LogSweep, Stimulus::LogSweep},
    std::pair{Stimulus::WhiteNoise, Stimulus::LogSweep}, std::pair{Stimulus::Sine, Stimulus::WhiteNoise}};

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

// CrossoverLR4's high output is the exact complement of its low output, so with an identity wet
// path the preserve-stereo preset returns its input sample for sample, up to float rounding.
TEST(CrossoverPresetsTest, PreserveStereoNullsAgainstItsInputWithAnIdentityWetPath)
{
    constexpr size_t kNumSamples = 48000;
    for (const auto& [leftKind, rightKind] : kStimulusPairs)
    {
        auto graph = compilePreset(kPreserveStereoLowBand);
        const auto input = stereoStimulus(leftKind, rightKind, kNumSamples);
        const auto output = OfflineRender::render(graph, input, kNumSamples, kBlockSize);
        EXPECT_LT(maxAbsDifference(output[0], input[0]), kFloatRoundingTolerance);
        EXPECT_LT(maxAbsDifference(output[1], input[1]), kFloatRoundingTolerance);
    }
}

// Summing the two channels must give the same signal in, whichever low-band policy is used.
TEST(CrossoverPresetsTest, MonoFoldDownIsPreservedByBothStereoCrossoverPresets)
{
    constexpr size_t kNumSamples = 48000;
    for (const std::string_view lua : {kPreserveStereoLowBand, kMonoLowBand})
    {
        for (const auto& [leftKind, rightKind] : kStimulusPairs)
        {
            auto graph = compilePreset(lua);
            const auto input = stereoStimulus(leftKind, rightKind, kNumSamples);
            const auto output = OfflineRender::render(graph, input, kNumSamples, kBlockSize);

            std::vector<float> inputSum(kNumSamples);
            std::vector<float> outputSum(kNumSamples);
            for (size_t i = 0; i < kNumSamples; ++i)
            {
                inputSum[i] = input[0][i] + input[1][i];
                outputSum[i] = output[0][i] + output[1][i];
            }
            EXPECT_LT(maxAbsDifference(outputSum, inputSum), kFloatRoundingTolerance);
        }
    }
}

// Mono-low rewrites the low band into both channels, so unlike preserve-stereo it changes the
// channel difference for content with low-frequency stereo information.
TEST(CrossoverPresetsTest, MonoLowBandChangesTheChannelDifferenceThatPreserveStereoKeeps)
{
    constexpr size_t kNumSamples = 48000;
    const auto input = stereoStimulus(Stimulus::LogSweep, Stimulus::Sine, kNumSamples);
    auto preserve = compilePreset(kPreserveStereoLowBand);
    auto monoLow = compilePreset(kMonoLowBand);
    const auto preserved = OfflineRender::render(preserve, input, kNumSamples, kBlockSize);
    const auto folded = OfflineRender::render(monoLow, input, kNumSamples, kBlockSize);

    std::vector<float> inputDifference(kNumSamples);
    std::vector<float> preservedDifference(kNumSamples);
    std::vector<float> foldedDifference(kNumSamples);
    for (size_t i = 0; i < kNumSamples; ++i)
    {
        inputDifference[i] = input[0][i] - input[1][i];
        preservedDifference[i] = preserved[0][i] - preserved[1][i];
        foldedDifference[i] = folded[0][i] - folded[1][i];
    }
    EXPECT_LT(maxAbsDifference(preservedDifference, inputDifference), kFloatRoundingTolerance);
    EXPECT_GT(maxAbsDifference(foldedDifference, inputDifference), kMinChannelChange);
}

}
