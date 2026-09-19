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
#include "Graph/MacroBank.h"
#include "Graph/MacroLowering.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/ControlNodes.h"
#include "Graph/Nodes/FilterNodes.h"
#include "Graph/Nodes/MacroInput.h"
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

[[nodiscard]] double rmsOf(const std::vector<float>& signal, const size_t from, const size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        sum += static_cast<double>(signal[i]) * signal[i];
    }
    return std::sqrt(sum / static_cast<double>(to - from));
}

[[nodiscard]] double correlationOf(const std::vector<float>& a, const std::vector<float>& b, const size_t from)
{
    double cross = 0.0;
    double powerA = 0.0;
    double powerB = 0.0;
    for (size_t i = from; i < a.size(); ++i)
    {
        cross += static_cast<double>(a[i]) * b[i];
        powerA += static_cast<double>(a[i]) * a[i];
        powerB += static_cast<double>(b[i]) * b[i];
    }
    return cross / std::sqrt(powerA * powerB);
}

// Level of a sine through the wet path of a preset (dry muted), measured over the last half.
[[nodiscard]] double wetToneGainDb(const std::string_view preset, const float frequencyHz)
{
    constexpr size_t kSamples = static_cast<size_t>(3 * kSampleRate);
    const auto tone = OfflineRender::stimulus({.kind = Stimulus::Sine, .amplitude = 0.3f, .frequencyHz = frequencyHz},
                                              kSamples, kSampleRate);
    auto graph = compileDescription(withDryMuted(loadPreset(preset)));
    const auto output = OfflineRender::render(graph, {tone, tone}, kSamples, kStandardBlockSize);
    return 20.0 * std::log10(rmsOf(output[0], kSamples / 2, kSamples) / rmsOf(tone, kSamples / 2, kSamples));
}

[[nodiscard]] std::vector<std::vector<float>> monoNoise(const size_t numSamples)
{
    const auto noise =
        OfflineRender::stimulus({.kind = Stimulus::WhiteNoise, .amplitude = 0.5f, .seed = 3}, numSamples, kSampleRate);
    return {noise, noise};
}

constexpr size_t kKnobSlots = 8;

[[nodiscard]] NodeRegistry makeMacroRegistry(const MacroBank& bank)
{
    auto registry = makeRegistry<kStandardBlockSize>();
    Nodes::registerControlNodes(registry);
    Nodes::registerMacroInputNode(registry, bank);
    return registry;
}

[[nodiscard]] CompiledGraph compileWith(const GraphDescription& description, const NodeRegistry& registry)
{
    auto result = GraphCompiler::compile(description, registry, kStandardBlockSize, kSampleRate);
    EXPECT_TRUE(result.graph.has_value()) << description.name;
    return std::move(*result.graph);
}

[[nodiscard]] double rmsDifference(const std::vector<std::vector<float>>& a, const std::vector<std::vector<float>>& b)
{
    double sum = 0.0;
    size_t count = 0;
    for (size_t channel = 0; channel < a.size(); ++channel)
    {
        for (size_t i = 0; i < a[channel].size(); ++i)
        {
            const double difference = static_cast<double>(a[channel][i]) - b[channel][i];
            sum += difference * difference;
            ++count;
        }
    }
    return std::sqrt(sum / static_cast<double>(count));
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
    EXPECT_LT(peakAbs(output[0], 0, 440), 1E-3f);
    EXPECT_GT(peakAbs(output[0], 480, 650), 0.5f);
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
        {"tape_vibrato", {{450, 660}}, 30},
        {"classic_stereo_chorus", {{500, 800}}, 90},
        {"shared_transport_heads", {{450, 680}, {680, 900}}, 90},
        {"ensemble_tri_chorus", {{500, 690}, {690, 900}, {900, 1200}}, 90},
        {"bbd_inspired", {{380, 650}}, 90},
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

// Targets, measured on the first version: 15 kHz sits 15.9 dB below 1 kHz, the first echo is 20 percent
// of the direct arrival and each further echo is smaller.
TEST(ChorusPresetsTest, BbdPresetLimitsItsBandwidthAndEchoesWithDecay)
{
    EXPECT_LE(wetToneGainDb("bbd_inspired", 15000.f), wetToneGainDb("bbd_inspired", 1000.f) - 12.0);

    auto graph = compileDescription(withDryMuted(loadPreset("bbd_inspired")));
    const auto impulse = OfflineRender::render(graph, leftImpulse(4096), 4096, kStandardBlockSize);
    const float direct = peakAbs(impulse[0], 380, 650);
    const float first = peakAbs(impulse[0], 800, 1300);
    const float second = peakAbs(impulse[0], 1300, 1900);
    EXPECT_GE(first, 0.05f * direct);
    EXPECT_LE(first, 0.6f * direct);
    EXPECT_LT(second, first);
}

// Targets, measured on the first version: 0.8 cents of pitch movement (the vibrato has 21), a mono
// input decorrelated to 0.68 with the dry signal in and to 0.00 in the wet path alone, and a
// left-only input reaching the right output at 0.27.
TEST(ChorusPresetsTest, DimensionKeepsItsPitchSteadyAndDecorrelatesAMonoInput)
{
    constexpr size_t kToneSamples = static_cast<size_t>(12 * kSampleRate);
    const auto tone = OfflineRender::stimulus({.kind = Stimulus::Sine, .amplitude = 0.5f, .frequencyHz = 1000.f},
                                              kToneSamples, kSampleRate);
    auto toneGraph = compileDescription(withDryMuted(loadPreset("dimension")));
    const auto toneOut = OfflineRender::render(toneGraph, {tone, tone}, kToneSamples, kStandardBlockSize);
    EXPECT_LE(peakPitchDeviationCents(toneOut[0], static_cast<size_t>(2 * kSampleRate)), 3.0);

    constexpr size_t kNoiseSamples = static_cast<size_t>(6 * kSampleRate);
    auto full = compilePreset("dimension");
    const auto withDry = OfflineRender::render(full, monoNoise(kNoiseSamples), kNoiseSamples, kStandardBlockSize);
    EXPECT_LE(correlationOf(withDry[0], withDry[1], 48000), 0.8);

    auto wetGraph = compileDescription(withDryMuted(loadPreset("dimension")));
    const auto wetOnly = OfflineRender::render(wetGraph, monoNoise(kNoiseSamples), kNoiseSamples, kStandardBlockSize);
    EXPECT_LE(std::abs(correlationOf(wetOnly[0], wetOnly[1], 48000)), 0.1);

    auto impulseGraph = compileDescription(withDryMuted(loadPreset("dimension")));
    const auto impulse = OfflineRender::render(impulseGraph, leftImpulse(4096), 4096, kStandardBlockSize);
    EXPECT_GE(peakAbs(impulse[1], 400, 800), 0.1f);
}

// A dangerous configuration on purpose: the validator must say so, a loud burst must bloom into a
// ring that lasts seconds (measured: 0.33 rms at 1 s, 0.001 at 5 s), and nothing may leave +-1.
TEST(ChorusPresetsTest, ExperimentalFeedbackWarnsBloomsOnALoudBurstAndStaysBounded)
{
    const auto description = loadPreset("experimental_resonant_feedback");
    const auto diagnostics = GraphValidator::validate(description, makeRegistry<kStandardBlockSize>());
    const auto hasWarning = [&diagnostics](const std::string_view prefix)
    {
        return std::ranges::any_of(
            diagnostics, [prefix](const Diagnostic& d)
            { return d.severity == DiagnosticSeverity::Warning && std::string_view{d.message}.starts_with(prefix); });
    };
    EXPECT_TRUE(hasWarning("feedback cycle has no damping filter"));
    EXPECT_TRUE(hasWarning("resonant filter inside an undamped feedback cycle"));

    constexpr size_t kSamples = static_cast<size_t>(6 * kSampleRate);
    const auto noise =
        OfflineRender::stimulus({.kind = Stimulus::WhiteNoise, .amplitude = 0.7f, .seed = 5}, kSamples, kSampleRate);
    std::vector<float> burst(kSamples, 0.f);
    std::copy_n(noise.begin(), 24000, burst.begin());
    auto burstGraph = compileDescription(withDryMuted(description));
    const auto rung = OfflineRender::render(burstGraph, {burst, burst}, kSamples, kStandardBlockSize);
    EXPECT_GE(rmsOf(rung[0], 48000, 50400), 0.1);
    EXPECT_LE(rmsOf(rung[0], 5 * 48000, 5 * 48000 + 2400), 0.05);

    constexpr size_t kLoudSamples = static_cast<size_t>(10 * kSampleRate);
    const auto loud = OfflineRender::stimulus({.kind = Stimulus::WhiteNoise, .amplitude = 1.0f, .seed = 4},
                                              kLoudSamples, kSampleRate);
    auto loudGraph = compileDescription(description);
    const auto output = OfflineRender::render(loudGraph, {loud, loud}, kLoudSamples, kStandardBlockSize);
    for (const auto& channel : output)
    {
        EXPECT_LE(peakAbs(channel, 0, channel.size()), 1.0f + 1E-6f);
    }
}

TEST(ChorusPresetsTest, EveryPresetOffersMacrosThatLowerCleanlyAndFitTheKnobPool)
{
    for (const auto& preset : Presets::kChorusPresets)
    {
        SCOPED_TRACE(std::string{preset.name});
        MacroBank bank;
        const auto registry = makeMacroRegistry(bank);
        const auto description = loadPreset(preset.name);
        ASSERT_FALSE(description.macros.empty());
        ASSERT_LE(description.macros.size(), kKnobSlots);

        const auto lowered = MacroLowering::lower(description, registry, {}, 0, kKnobSlots);
        EXPECT_TRUE(lowered.diagnostics.empty()) << lowered.diagnostics.front().message;
        EXPECT_EQ(lowered.macros.size(), description.macros.size());
        std::set<std::string> ids;
        for (const auto& macro : lowered.macros)
        {
            EXPECT_TRUE(ids.insert(macro.id).second) << "duplicate " << macro.id;
            EXPECT_FALSE(macro.label.empty());
            EXPECT_LT(macro.displayMin, macro.displayMax) << macro.id;
            EXPECT_GE(macro.defaultValue, macro.displayMin) << macro.id;
            EXPECT_LE(macro.defaultValue, macro.displayMax) << macro.id;
        }
    }
}

// The macros are the only controls the app offers, so at their defaults they must give exactly the
// sound the script's own parameters describe.
TEST(ChorusPresetsTest, MacrosAtTheirDefaultsReproduceTheScriptsOwnParameters)
{
    const auto input = stereoNoise(48000);
    for (const auto& preset : Presets::kChorusPresets)
    {
        SCOPED_TRACE(std::string{preset.name});
        MacroBank bank;
        const auto registry = makeMacroRegistry(bank);
        const auto description = loadPreset(preset.name);
        const auto lowered = MacroLowering::lower(description, registry, {}, 0, kKnobSlots);
        for (const auto& macro : lowered.macros)
        {
            bank.set(macro.slot, macro.defaultNormalized);
        }
        auto withMacros = compileWith(lowered.description, registry);
        auto plain = compileWith(description, registry);
        const auto a = OfflineRender::render(withMacros, input, 48000, kStandardBlockSize);
        const auto b = OfflineRender::render(plain, input, 48000, kStandardBlockSize);
        EXPECT_LT(rmsDifference(a, b), 1E-3);
    }
}

// A knob that changes nothing is a defect, so every macro is moved from 0 to 1 on its own.
TEST(ChorusPresetsTest, EveryMacroChangesTheOutputWhenItIsMoved)
{
    const auto input = stereoNoise(24000);
    for (const auto& preset : Presets::kChorusPresets)
    {
        MacroBank probe;
        const auto probeRegistry = makeMacroRegistry(probe);
        const auto description = loadPreset(preset.name);
        const auto lowered = MacroLowering::lower(description, probeRegistry, {}, 0, kKnobSlots);
        for (const auto& macro : lowered.macros)
        {
            SCOPED_TRACE(std::string{preset.name} + " / " + macro.id);
            std::array<std::vector<std::vector<float>>, 2> rendered;
            for (size_t end = 0; end < 2; ++end)
            {
                MacroBank bank;
                const auto registry = makeMacroRegistry(bank);
                const auto again = MacroLowering::lower(description, registry, {}, 0, kKnobSlots);
                for (const auto& other : again.macros)
                {
                    bank.set(other.slot, other.defaultNormalized);
                }
                bank.set(macro.slot, static_cast<float>(end));
                auto graph = compileWith(again.description, registry);
                rendered[end] = OfflineRender::render(graph, input, 24000, kStandardBlockSize);
            }
            EXPECT_GT(rmsDifference(rendered[0], rendered[1]), 1E-4);
        }
    }
}

}
