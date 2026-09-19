#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

constexpr float kSampleRate = 48000.f;
constexpr size_t kStandardBlockSize = 16;
constexpr size_t kLargeBlockSize = 64;
constexpr size_t kRenderSamples = static_cast<size_t>(5 * kSampleRate);
constexpr float kOutputBound = 4.f;

template <size_t BlockSize>
[[nodiscard]] NodeRegistry makeRegistry()
{
    NodeRegistry registry;
    Nodes::registerStandardNodes(registry);
    Nodes::registerFilterNodes(registry);
    Nodes::registerTapeDelayNode<BlockSize>(registry);
    return registry;
}

[[nodiscard]] std::string_view presetLua(const std::string_view name)
{
    for (const auto& preset : Presets::kChorusPresets)
    {
        if (preset.name == name)
        {
            return preset.lua;
        }
    }
    ADD_FAILURE() << "no preset named " << name;
    return {};
}

[[nodiscard]] GraphDescription loadPreset(const std::string_view name)
{
    const auto loaded = Lua::LuaGraphLoader::loadFromString(presetLua(name));
    EXPECT_TRUE(loaded.description.has_value()) << name;
    return loaded.description.value_or(GraphDescription{});
}

template <size_t BlockSize = kStandardBlockSize>
[[nodiscard]] CompiledGraph compileDescription(const GraphDescription& description)
{
    auto result = GraphCompiler::compile(description, makeRegistry<BlockSize>(), BlockSize, kSampleRate);
    EXPECT_TRUE(result.graph.has_value()) << description.name;
    return std::move(*result.graph);
}

template <size_t BlockSize = kStandardBlockSize>
[[nodiscard]] CompiledGraph compilePreset(const std::string_view name)
{
    return compileDescription<BlockSize>(loadPreset(name));
}

[[nodiscard]] bool hasError(const std::vector<Diagnostic>& diagnostics)
{
    return std::ranges::any_of(diagnostics,
                               [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Error; });
}

[[nodiscard]] std::vector<std::vector<float>> stereoNoise(const size_t numSamples)
{
    return {
        OfflineRender::stimulus({.kind = Stimulus::WhiteNoise, .amplitude = 0.5f, .seed = 1}, numSamples, kSampleRate),
        OfflineRender::stimulus({.kind = Stimulus::WhiteNoise, .amplitude = 0.5f, .seed = 2}, numSamples, kSampleRate)};
}

[[nodiscard]] std::vector<std::vector<float>> stereoSweep(const size_t numSamples)
{
    const StimulusSpec sweep{
        .kind = Stimulus::LogSweep, .amplitude = 0.5f, .frequencyHz = 20.f, .endFrequencyHz = 20000.f};
    return {OfflineRender::stimulus(sweep, numSamples, kSampleRate),
            OfflineRender::stimulus(sweep, numSamples, kSampleRate)};
}

// A unit impulse on the left channel only.
[[nodiscard]] std::vector<std::vector<float>> leftImpulse(const size_t numSamples)
{
    return {OfflineRender::stimulus({.kind = Stimulus::Impulse}, numSamples, kSampleRate),
            std::vector<float>(numSamples, 0.f)};
}

[[nodiscard]] float peakAbs(const std::vector<float>& signal, const size_t from, const size_t to)
{
    float peak = 0.f;
    for (size_t i = from; i < std::min(to, signal.size()); ++i)
    {
        peak = std::max(peak, std::abs(signal[i]));
    }
    return peak;
}

// Counts separate bursts above the threshold; samples closer than minGap belong to one burst.
[[nodiscard]] size_t countArrivals(const std::vector<float>& signal, const float threshold, const size_t minGap)
{
    size_t arrivals = 0;
    bool any = false;
    size_t lastAbove = 0;
    for (size_t i = 0; i < signal.size(); ++i)
    {
        if (std::abs(signal[i]) > threshold)
        {
            arrivals += (!any || i - lastAbove > minGap) ? 1 : 0;
            lastAbove = i;
            any = true;
        }
    }
    return arrivals;
}

} // namespace

TEST(ChorusPresetsTest, EveryPresetLoadsValidatesAndCompilesAtBothBlockSizes)
{
    for (const auto& preset : Presets::kChorusPresets)
    {
        SCOPED_TRACE(std::string{preset.name});
        const auto loaded = Lua::LuaGraphLoader::loadFromString(preset.lua);
        ASSERT_TRUE(loaded.description.has_value());
        EXPECT_FALSE(hasError(loaded.diagnostics));
        EXPECT_FALSE(hasError(GraphValidator::validate(*loaded.description, makeRegistry<kStandardBlockSize>())));

        auto small = GraphCompiler::compile(*loaded.description, makeRegistry<kStandardBlockSize>(), kStandardBlockSize,
                                            kSampleRate);
        auto large =
            GraphCompiler::compile(*loaded.description, makeRegistry<kLargeBlockSize>(), kLargeBlockSize, kSampleRate);
        EXPECT_TRUE(small.graph.has_value());
        EXPECT_TRUE(large.graph.has_value());
    }
}

// The runtime ignores unknown parameters and config keys silently, so a typo or a key copied
// from a design note would do nothing. Every key a preset sets must be one a node reads.
TEST(ChorusPresetsTest, EveryPresetKeyIsOneTheNodeActuallyReads)
{
    const std::map<std::string, std::set<std::string>> readConfigKeys{
        {"TapeDelay", {"baseDelayMs", "safetyMarginSamples", "seed"}},
        {"Biquad", {"mode"}},
    };
    const auto registry = makeRegistry<kStandardBlockSize>();
    for (const auto& preset : Presets::kChorusPresets)
    {
        SCOPED_TRACE(std::string{preset.name});
        const auto loaded = Lua::LuaGraphLoader::loadFromString(preset.lua);
        ASSERT_TRUE(loaded.description.has_value());
        for (const auto& node : loaded.description->nodes)
        {
            const auto* schema = registry.findSchema(node.type);
            ASSERT_NE(schema, nullptr) << node.id;
            for (const auto& [key, value] : node.params)
            {
                EXPECT_GE(schema->findParameterIndex(key), 0) << node.id << "." << key;
            }
            const auto known = readConfigKeys.find(node.type);
            for (const auto& [key, value] : node.config)
            {
                ASSERT_NE(known, readConfigKeys.end()) << node.id << " has config but " << node.type << " reads none";
                EXPECT_TRUE(known->second.contains(key)) << node.id << "." << key;
            }
        }
    }
}

TEST(ChorusPresetsTest, EveryPresetStaysFiniteAndBoundedOnNoiseAndSweeps)
{
    for (const auto& preset : Presets::kChorusPresets)
    {
        SCOPED_TRACE(std::string{preset.name});
        for (const auto& input : {stereoNoise(kRenderSamples), stereoSweep(kRenderSamples)})
        {
            auto graph = compilePreset(preset.name);
            const auto output = OfflineRender::render(graph, input, kRenderSamples, kStandardBlockSize);
            for (const auto& channel : output)
            {
                for (const float sample : channel)
                {
                    ASSERT_TRUE(std::isfinite(sample));
                    ASSERT_LT(std::abs(sample), kOutputBound);
                }
            }
        }
    }
}

TEST(ChorusPresetsTest, EveryPresetIsDeterministic)
{
    const auto input = stereoNoise(48000);
    for (const auto& preset : Presets::kChorusPresets)
    {
        SCOPED_TRACE(std::string{preset.name});
        auto first = compilePreset(preset.name);
        auto second = compilePreset(preset.name);
        EXPECT_EQ(OfflineRender::render(first, input, 48000, kStandardBlockSize),
                  OfflineRender::render(second, input, 48000, kStandardBlockSize));
    }
}

// The header is generated from the .lua files; editing one without regenerating must fail here.
TEST(ChorusPresetsTest, HeaderMatchesTheLuaSourceFiles)
{
    std::map<std::string, std::string> onDisk;
    for (const auto& entry : std::filesystem::directory_iterator(GRAPH_PRESET_LUA_DIR))
    {
        if (entry.path().extension() != ".lua")
        {
            continue;
        }
        std::string stem = entry.path().stem().string();
        const size_t underscore = stem.find('_');
        if (underscore != std::string::npos && std::isdigit(static_cast<unsigned char>(stem.front())) != 0)
        {
            stem = stem.substr(underscore + 1);
        }
        std::ifstream file(entry.path());
        std::ostringstream contents;
        contents << file.rdbuf();
        onDisk[stem] = contents.str();
    }

    EXPECT_EQ(onDisk.size(), Presets::kChorusPresets.size()) << "run dev-scripts/dev-generate-presets.sh";
    for (const auto& preset : Presets::kChorusPresets)
    {
        const auto found = onDisk.find(std::string{preset.name});
        ASSERT_NE(found, onDisk.end()) << preset.name;
        EXPECT_EQ(found->second, std::string{preset.lua}) << preset.name << ": run dev-scripts/dev-generate-presets.sh";
    }
}

TEST(ChorusPresetsTest, TapeVibratoIsFullyWetAndItsImpulseArrivesAtTheBaseDelayPlusSamplerLatency)
{
    auto graph = compilePreset("tape_vibrato");
    const auto output = OfflineRender::render(graph, leftImpulse(8192), 8192, kStandardBlockSize);
    EXPECT_LT(peakAbs(output[0], 0, 400), 1E-3f);
    EXPECT_GT(peakAbs(output[0], 400, 520), 0.5f);
}

TEST(ChorusPresetsTest, ClassicChorusKeepsTheDryImpulseAddsAWetOneAndStaysDualMono)
{
    auto graph = compilePreset("classic_stereo_chorus");
    const auto output = OfflineRender::render(graph, leftImpulse(8192), 8192, kStandardBlockSize);
    EXPECT_GT(std::abs(output[0][0]), 0.9f);
    EXPECT_GT(peakAbs(output[0], 240, 2000), 0.05f);
    EXPECT_LT(peakAbs(output[1], 0, 8192), 1E-6f);
}

TEST(ChorusPresetsTest, SharedTransportHeadsGiveTwoWetArrivals)
{
    auto wetOnly = loadPreset("shared_transport_heads");
    for (auto& node : wetOnly.nodes)
    {
        if (node.id == "dry")
        {
            node.params["gainDb"] = -60.f;
        }
    }
    auto wetGraph = compileDescription(wetOnly);
    const auto output = OfflineRender::render(wetGraph, leftImpulse(8192), 8192, kStandardBlockSize);
    EXPECT_EQ(countArrivals(output[0], 0.03f, 100), 2u);
}

TEST(ChorusPresetsTest, EnsembleGivesThreeWetArrivalsAndKeepsItsLevelNearTheInput)
{
    auto wetOnly = loadPreset("ensemble_tri_chorus");
    for (auto& node : wetOnly.nodes)
    {
        if (node.id == "dry")
        {
            node.params["gainDb"] = -60.f;
        }
    }
    auto graph = compileDescription(wetOnly);
    const auto impulse = OfflineRender::render(graph, leftImpulse(8192), 8192, kStandardBlockSize);
    EXPECT_EQ(countArrivals(impulse[0], 0.03f, 100), 3u);

    auto noiseGraph = compileDescription(wetOnly);
    const auto input = stereoNoise(kRenderSamples);
    const auto output = OfflineRender::render(noiseGraph, input, kRenderSamples, kStandardBlockSize);
    const auto rms = [](const std::vector<float>& signal)
    {
        double sum = 0.0;
        for (const float sample : signal)
        {
            sum += static_cast<double>(sample) * sample;
        }
        return std::sqrt(sum / static_cast<double>(signal.size()));
    };
    const double ratioDb = 20.0 * std::log10(rms(output[0]) / rms(input[0]));
    EXPECT_GT(ratioDb, -6.0);
    EXPECT_LT(ratioDb, 3.0);
}

}
