#include <algorithm>
#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <numbers>
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

[[nodiscard]] GraphDescription withDryMuted(GraphDescription description)
{
    for (auto& node : description.nodes)
    {
        if (node.id == "dry")
        {
            node.params["gainDb"] = -60.f;
        }
    }
    return description;
}

constexpr size_t kTrainPeriod = 2400;
constexpr size_t kTrainSamples = static_cast<size_t>(20 * kSampleRate);

// One impulse every kTrainPeriod samples on the left channel, silence on the right.
[[nodiscard]] std::vector<std::vector<float>> leftImpulseTrain()
{
    std::vector<float> left(kTrainSamples, 0.f);
    for (size_t at = 0; at < kTrainSamples; at += kTrainPeriod)
    {
        left[at] = 1.f;
    }
    return {left, std::vector<float>(kTrainSamples, 0.f)};
}

struct LagWindow
{
    size_t from;
    size_t to;
};

// Peak-to-peak spread, in samples, of where the strongest sample of each impulse's response lands
// inside the window. The first impulses are skipped so the modulation has started moving.
[[nodiscard]] long lagSwing(const std::vector<float>& output, const LagWindow window)
{
    constexpr size_t kSkippedImpulses = 2;
    long lowest = static_cast<long>(window.to);
    long highest = static_cast<long>(window.from);
    for (size_t start = kSkippedImpulses * kTrainPeriod; start + window.to < output.size(); start += kTrainPeriod)
    {
        size_t strongest = window.from;
        for (size_t i = window.from; i < window.to; ++i)
        {
            strongest = std::abs(output[start + i]) > std::abs(output[start + strongest]) ? i : strongest;
        }
        lowest = std::min(lowest, static_cast<long>(strongest));
        highest = std::max(highest, static_cast<long>(strongest));
    }
    return highest - lowest;
}

// Instantaneous frequency of each cycle from interpolated upward zero crossings.
[[nodiscard]] std::vector<double> cycleFrequencies(const std::vector<float>& signal, const size_t from)
{
    std::vector<double> crossings;
    for (size_t i = from; i + 1 < signal.size(); ++i)
    {
        if (signal[i] < 0.f && signal[i + 1] >= 0.f)
        {
            crossings.push_back(static_cast<double>(i) + static_cast<double>(-signal[i] / (signal[i + 1] - signal[i])));
        }
    }
    std::vector<double> frequencies;
    for (size_t i = 1; i < crossings.size(); ++i)
    {
        frequencies.push_back(kSampleRate / (crossings[i] - crossings[i - 1]));
    }
    return frequencies;
}

[[nodiscard]] double peakPitchDeviationCents(const std::vector<float>& signal, const size_t from)
{
    auto frequencies = cycleFrequencies(signal, from);
    auto sorted = frequencies;
    std::ranges::sort(sorted);
    const double median = sorted[sorted.size() / 2];
    double peak = 0.0;
    for (const double frequency : frequencies)
    {
        peak = std::max(peak, std::abs(1200.0 * std::log2(frequency / median)));
    }
    return peak;
}

// Share of the power within +-totalHalfWidthHz of the carrier that sits within +-carrierHalfWidthHz
// of it, from a Hann-windowed DFT taken every stepHz. 1.0 is a pure tone; modulation lowers it.
[[nodiscard]] double carrierPowerFraction(const std::vector<float>& signal, const size_t from, const size_t to,
                                          const double carrierHz, const double carrierHalfWidthHz,
                                          const double totalHalfWidthHz, const double stepHz)
{
    const size_t length = to - from;
    std::vector<double> windowed(length);
    for (size_t i = 0; i < length; ++i)
    {
        const double hann =
            0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(length - 1));
        windowed[i] = static_cast<double>(signal[from + i]) * hann;
    }
    double carrier = 0.0;
    double total = 0.0;
    for (double offset = -totalHalfWidthHz; offset <= totalHalfWidthHz; offset += stepHz)
    {
        const std::complex<double> rotation =
            std::polar(1.0, -2.0 * std::numbers::pi * (carrierHz + offset) / static_cast<double>(kSampleRate));
        std::complex<double> phasor{1.0, 0.0};
        std::complex<double> sum{0.0, 0.0};
        for (const double sample : windowed)
        {
            sum += sample * phasor;
            phasor *= rotation;
        }
        total += std::norm(sum);
        carrier += std::abs(offset) <= carrierHalfWidthHz ? std::norm(sum) : 0.0;
    }
    return carrier / total;
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
    auto wetGraph = compileDescription(withDryMuted(loadPreset("shared_transport_heads")));
    const auto output = OfflineRender::render(wetGraph, leftImpulse(8192), 8192, kStandardBlockSize);
    EXPECT_EQ(countArrivals(output[0], 0.03f, 100), 2u);
}

TEST(ChorusPresetsTest, EnsembleGivesThreeWetArrivalsAndKeepsItsLevelNearTheInput)
{
    const auto wetOnly = withDryMuted(loadPreset("ensemble_tri_chorus"));
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

// Modulation that is too small to hear also passes every structural check, so the swing itself is
// asserted: 30 samples (0.6 ms) for the vibrato, 90 samples (1.9 ms) for the chorus family.
TEST(ChorusPresetsTest, ModulatedPresetsSwingTheirDelayByAnAudibleAmount)
{
    struct Case
    {
        std::string_view preset;
        std::vector<LagWindow> windows;
        long minimum;
    };
    const std::vector<Case> cases{
        {"tape_vibrato", {{400, 560}}, 30},
        {"classic_stereo_chorus", {{500, 800}}, 90},
        {"shared_transport_heads", {{450, 680}, {680, 900}}, 90},
        {"ensemble_tri_chorus", {{500, 690}, {690, 900}, {900, 1200}}, 90},
    };
    constexpr long kMaximumSwing = 250;
    for (const auto& testCase : cases)
    {
        SCOPED_TRACE(std::string{testCase.preset});
        auto graph = compileDescription(withDryMuted(loadPreset(testCase.preset)));
        const auto output = OfflineRender::render(graph, leftImpulseTrain(), kTrainSamples, kStandardBlockSize);
        for (const auto& window : testCase.windows)
        {
            const long swing = lagSwing(output[0], window);
            EXPECT_GE(swing, testCase.minimum) << "window " << window.from << " to " << window.to;
            EXPECT_LE(swing, kMaximumSwing) << "window " << window.from << " to " << window.to;
        }
    }
}

// The vibrato is fully wet, so a 1 kHz tone through it shows the effect directly: its pitch moves
// by tens of cents and the carrier hands part of its power to sidebands.
TEST(ChorusPresetsTest, TapeVibratoMovesThePitchAndSpreadsTheCarrierIntoSidebands)
{
    constexpr size_t kSamples = static_cast<size_t>(12 * kSampleRate);
    const auto tone = OfflineRender::stimulus({.kind = Stimulus::Sine, .amplitude = 0.5f, .frequencyHz = 1000.f},
                                              kSamples, kSampleRate);
    auto graph = compilePreset("tape_vibrato");
    const auto output = OfflineRender::render(graph, {tone, tone}, kSamples, kStandardBlockSize);

    const double cents = peakPitchDeviationCents(output[0], static_cast<size_t>(2 * kSampleRate));
    EXPECT_GE(cents, 12.0);
    EXPECT_LE(cents, 40.0);

    const double carrierFraction = carrierPowerFraction(output[0], static_cast<size_t>(2 * kSampleRate),
                                                        static_cast<size_t>(10 * kSampleRate), 1000.0, 1.0, 60.0, 0.5);
    EXPECT_LE(carrierFraction, 0.6);
}

// The check itself must be able to fail: an unmodulated tone keeps its carrier and its pitch.
TEST(ChorusPresetsTest, CarrierAndPitchChecksSeeNothingInAnUnmodulatedTone)
{
    constexpr size_t kSamples = static_cast<size_t>(12 * kSampleRate);
    const auto tone = OfflineRender::stimulus({.kind = Stimulus::Sine, .amplitude = 0.5f, .frequencyHz = 1000.f},
                                              kSamples, kSampleRate);
    EXPECT_LT(peakPitchDeviationCents(tone, static_cast<size_t>(2 * kSampleRate)), 1.0);
    EXPECT_GT(carrierPowerFraction(tone, static_cast<size_t>(2 * kSampleRate), static_cast<size_t>(10 * kSampleRate),
                                   1000.0, 1.0, 60.0, 0.5),
              0.95);
}

}
