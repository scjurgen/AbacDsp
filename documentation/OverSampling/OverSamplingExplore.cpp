// Verification plots for UpDownSampler wrapping WobbleDelay: swept-tone spectrograms across
// oversampling and undersampling ratios, delay time in host samples versus ratio, and the
// response to abrupt versus slow ratio changes. See README.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Analysis/Spectrogram.h"
#include "Delays/WobbleDelay.h"
#include "SamplerateConverter/UpDownSampler.h"

namespace
{
constexpr float kHostRate = 48000.f;
constexpr size_t kFrame = 16;
constexpr size_t kMaxBlock = 512;
constexpr size_t kBufferSize = 48000;
constexpr float kHostDelay = 500.f;

using Delay = AbacDsp::WobbleDelay<kBufferSize, kFrame>;
using Composed = AbacDsp::UpDownSampler<Delay, kFrame>;

// The delay is given in host samples, so the wrapped delay line gets it in internal samples.
void configure(Composed& composed, const float ratio, const bool modulated)
{
    composed.setRatio(ratio);
    auto& delay = composed.processor();
    delay.setDelay(kHostDelay * ratio, true);
    if (modulated)
    {
        delay.seed(7);
        delay.setWowRate(1.5f);
        delay.setWowDepth(0.8f);
        delay.setWowVariance(0.3f);
        delay.setFlutterRate(8.f);
        delay.setFlutterDepth(0.8f);
    }
}

[[nodiscard]] std::vector<float> makeSweep(const float startHz, const float endHz, const size_t numSamples)
{
    std::vector<float> tone(numSamples);
    float phase = 0.f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float frac = static_cast<float>(i) / static_cast<float>(numSamples - 1);
        tone[i] = 0.5f * std::sin(phase);
        phase += 2.f * std::numbers::pi_v<float> * (startHz + (endHz - startHz) * frac) / kHostRate;
    }
    return tone;
}

[[nodiscard]] std::vector<float> makeTone(const float hz, const size_t numSamples)
{
    std::vector<float> tone(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        tone[i] = 0.5f * std::sin(2.f * std::numbers::pi_v<float> * hz * static_cast<float>(i) / kHostRate);
    }
    return tone;
}

// Runs the input in host blocks, asking ratioAt(position) for the ratio before each block.
template <typename RatioAt>
[[nodiscard]] std::vector<float> render(Composed& composed, const std::vector<float>& input, RatioAt ratioAt)
{
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += kMaxBlock)
    {
        const auto count = std::min(kMaxBlock, input.size() - pos);
        composed.setRatio(ratioAt(pos));
        composed.processBlock(std::span<const float>{input}.subspan(pos, count),
                              std::span<float>{output}.subspan(pos, count));
    }
    return output;
}

// SimpleSpectrogram's worker queue silently drops a frame fed while full - poll
// queueHasRoom() before every feed. Grid format: "rows cols sampleRate fftLength hop" header,
// then one row per line.
void writeSpectrogramGrid(std::ofstream& out, const std::vector<float>& audio, const unsigned fftLength)
{
    AbacDsp::SimpleSpectrogram spec;
    spec.setSampleRate(kHostRate);
    spec.setFftLength(fftLength);
    const size_t hop = spec.forwardLength();
    const size_t expectedFrames = audio.size() >= fftLength ? (audio.size() - fftLength) / hop + 1 : 0;
    spec.setSlices(expectedFrames + 8);

    for (size_t fed = 0; fed < audio.size();)
    {
        while (!spec.queueHasRoom())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const size_t chunk = std::min(hop, audio.size() - fed);
        spec.processBlock(audio.data() + fed, chunk);
        fed += chunk;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (spec.getImageSet().activeSlice < expectedFrames && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    const auto img = spec.getImageSet();
    out << img.activeSlice << " " << img.height << " " << img.sampleRate << " " << img.fftLength << " " << hop << "\n";
    for (size_t row = 0; row < img.activeSlice; ++row)
    {
        for (size_t bin = 0; bin < img.height; ++bin)
        {
            const float db = 20.f * std::log10(std::max(img.data[row * img.height + bin], 1e-9f));
            out << db << (bin + 1 < img.height ? ' ' : '\n');
        }
    }
}

void writeGridAndWav(const std::string& base, const std::vector<float>& audio)
{
    std::ofstream grid(base + ".txt");
    writeSpectrogramGrid(grid, audio, 4096);
    AudioUtility::SaveWav::saveMonoAs(base + ".wav", audio, kHostRate);
}

constexpr std::array<std::pair<float, const char*>, 5> kSweepRatios{
    {{0.99f, "0.99"}, {1.f, "1.0"}, {0.5f, "0.5"}, {2.f, "2.0"}, {4.f, "4.0"}}};

// Modulation on: wow and flutter must not raise the noise floor at any ratio.
void writeSweeps(const std::string& outDir)
{
    const auto input = makeSweep(50.f, 20000.f, static_cast<size_t>(4.f * kHostRate));
    for (const auto& [ratio, tag] : kSweepRatios)
    {
        Composed composed(kMaxBlock, kHostRate * ratio);
        configure(composed, ratio, true);
        const auto audio = render(composed, input, [ratio](size_t) { return ratio; });
        writeGridAndWav(outDir + "/os_sweep_" + tag, audio);
    }
}

[[nodiscard]] size_t peakIndex(const std::vector<float>& signal)
{
    const auto peak =
        std::ranges::max_element(signal, [](const float a, const float b) { return std::abs(a) < std::abs(b); });
    return static_cast<size_t>(std::distance(signal.begin(), peak));
}

// An impulse through the composition: arrival in host samples against what the delay alone
// (delay / ratio) would give. The difference is the converter latency at that ratio.
void writeDelayTime(std::ofstream& out)
{
    out << "@New plot: title=\"UpDownSampler<WobbleDelay>: impulse arrival in host samples vs. ratio\"\n";
    constexpr std::array<float, 11> ratios{0.125f, 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 3.f, 4.f, 8.f, 16.f};
    constexpr size_t kImpulseAt = 4000;
    constexpr float kInternalDelay = 800.f;

    std::vector<std::pair<float, float>> delayOnly;
    out << "#measured arrival\n";
    for (const float ratio : ratios)
    {
        Composed composed(kMaxBlock, kHostRate * ratio);
        composed.setRatio(ratio);
        composed.processor().setDelay(kInternalDelay, true);
        std::vector<float> input(60000, 0.f);
        input[kImpulseAt] = 1.f;
        const auto audio = render(composed, input, [ratio](size_t) { return ratio; });
        out << ratio << " " << (static_cast<float>(peakIndex(audio) - kImpulseAt)) << "\n";
        delayOnly.emplace_back(ratio, kInternalDelay / ratio);
    }
    out << "#delay / ratio only\n";
    for (const auto& [ratio, arrival] : delayOnly)
    {
        out << ratio << " " << arrival << "\n";
    }
}

// A steady 1kHz probe with the ratio changing: no modulation, so anything visible is the
// ratio change itself.
void writeRatioChanges(const std::string& outDir)
{
    const auto input = makeTone(1000.f, static_cast<size_t>(4.f * kHostRate));

    const auto abrupt = [](const size_t pos) { return pos < static_cast<size_t>(2.f * kHostRate) ? 0.5f : 2.f; };
    Composed abruptComposed(kMaxBlock, kHostRate);
    configure(abruptComposed, 0.5f, false);
    writeGridAndWav(outDir + "/os_jump_abrupt", render(abruptComposed, input, abrupt));

    const auto slow = [](const size_t pos)
    {
        const auto phase = static_cast<float>(pos) / kHostRate;
        return 1.f + 0.25f * std::sin(2.f * std::numbers::pi_v<float> * 0.5f * phase);
    };
    Composed slowComposed(kMaxBlock, kHostRate);
    configure(slowComposed, 1.f, false);
    writeGridAndWav(outDir + "/os_jump_slow", render(slowComposed, input, slow));
}
}

int main(int argc, char* argv[])
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    std::ofstream delayTimeOut(outDir + "/os_delaytime.txt");
    if (!delayTimeOut)
    {
        std::cerr << "OverSamplingExplore: ERROR - failed to open output files in " << outDir << std::endl;
        return 1;
    }

    writeSweeps(outDir);
    writeDelayTime(delayTimeOut);
    writeRatioChanges(outDir);

    std::cout << "OverSamplingExplore: wrote plot data, spectrogram grids and WAVs into " << outDir << std::endl;
}
