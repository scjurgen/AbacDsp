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
#include "Parameters/OctaveGlide.h"
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
    composed.setRatio(ratio, true);
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
[[nodiscard]] std::vector<float> render(Composed& composed, const std::vector<float>& input, RatioAt ratioAt,
                                        const size_t blockSize = kMaxBlock)
{
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += blockSize)
    {
        const auto count = std::min(blockSize, input.size() - pos);
        composed.setRatio(ratioAt(pos), true);
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
        composed.setRatio(ratio, true);
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

constexpr float kToneHz = 1000.f;
constexpr float kChangeAtSeconds = 1.5f;

// Glide rates in octaves per second for the study below; 0 is an immediate jump.
constexpr std::array<float, 4> kGlideRates{0.f, 24.f, 6.f, 1.5f};

// Renders the probe while the ratio goes from `from` to `to` after kChangeAtSeconds, either
// at once or glided by OctaveGlide the way OrganicChorusVoice and PathfinderImpl do it.
[[nodiscard]] std::vector<float> renderRatioChange(const float from, const float to, const float octavesPerSecond,
                                                   const size_t blockSize, size_t& underruns)
{
    Composed composed(kMaxBlock, kHostRate);
    configure(composed, from, false);
    const bool immediate = octavesPerSecond <= 0.f;
    AbacDsp::OctaveGlide glide(kHostRate, from, immediate ? 1.f : octavesPerSecond, immediate ? 1.f : octavesPerSecond);
    const auto changeAt = static_cast<size_t>(kChangeAtSeconds * kHostRate);
    bool changed = false;
    const auto ratioAt = [&](const size_t pos)
    {
        if (!changed && pos >= changeAt)
        {
            glide.setTarget(to, immediate);
            changed = true;
        }
        return glide.getValue(blockSize);
    };
    const auto audio = render(composed, makeTone(kToneHz, static_cast<size_t>(4.f * kHostRate)), ratioAt, blockSize);
    underruns = composed.underruns();
    return audio;
}

// Same change, done by the sampler's own glide (or a forced jump) instead of a driven ratio.
[[nodiscard]] std::vector<float> renderClassGlide(const float from, const float to, const size_t blockSize,
                                                  size_t& underruns)
{
    Composed composed(kMaxBlock, kHostRate);
    configure(composed, from, false);
    const auto input = makeTone(kToneHz, static_cast<size_t>(4.f * kHostRate));
    const auto changeAt = static_cast<size_t>(kChangeAtSeconds * kHostRate);
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += blockSize)
    {
        if (pos == changeAt)
        {
            composed.setRatio(to);
        }
        const auto count = std::min(blockSize, input.size() - pos);
        composed.processBlock(std::span<const float>{input}.subspan(pos, count),
                              std::span<float>{output}.subspan(pos, count));
    }
    underruns = composed.underruns();
    return output;
}

// Worst 256-sample window of the second difference, against the steady tone: 1.0 is clean.
// Repeated samples add broadband energy without any large sample-to-sample step.
[[nodiscard]] float broadbandRatio(const std::vector<float>& audio)
{
    constexpr size_t kWindow = 256;
    const auto omega = 2.f * std::numbers::pi_v<float> * kToneHz / kHostRate;
    const auto steady = 0.5f * omega * omega / std::numbers::sqrt2_v<float>;
    float worst = 0.f;
    for (auto start = static_cast<size_t>(kChangeAtSeconds * kHostRate) - 4000; start + kWindow < audio.size();
         start += kWindow / 2)
    {
        double sum = 0.0;
        for (size_t i = start; i < start + kWindow; ++i)
        {
            const auto d2 = static_cast<double>(audio[i]) - 2.0 * audio[i - 1] + audio[i - 2];
            sum += d2 * d2;
        }
        worst = std::max(worst, static_cast<float>(std::sqrt(sum / kWindow)) / steady);
    }
    return worst;
}

void writeGlideStudy(const std::string& outDir)
{
    std::ofstream metrics(outDir + "/os_glide_metrics.txt");
    metrics << "from to octaves_per_second(-1=sampler glide) broadband_ratio underruns\n";
    const std::array<std::pair<float, float>, 5> changes{
        {{0.25f, 1.f}, {0.5f, 2.f}, {1.f, 0.5f}, {1.f, 0.25f}, {1.f, 0.0625f}}};
    for (const auto& [from, to] : changes)
    {
        for (const float rate : kGlideRates)
        {
            size_t underruns = 0;
            const auto audio = renderRatioChange(from, to, rate, kFrame, underruns);
            metrics << from << " " << to << " " << rate << " " << broadbandRatio(audio) << " " << underruns << "\n";
        }
        size_t underruns = 0;
        const auto audio = renderClassGlide(from, to, kFrame, underruns);
        metrics << from << " " << to << " -1 " << broadbandRatio(audio) << " " << underruns << "\n";
    }

    size_t unused = 0;
    writeGridAndWav(outDir + "/os_glide_down_immediate", renderRatioChange(1.f, 0.25f, 0.f, kFrame, unused));
    writeGridAndWav(outDir + "/os_glide_down_glide", renderClassGlide(1.f, 0.25f, kFrame, unused));
    writeGridAndWav(outDir + "/os_glide_up_immediate", renderRatioChange(0.25f, 1.f, 0.f, kFrame, unused));
    writeGridAndWav(outDir + "/os_glide_up_glide", renderClassGlide(0.25f, 1.f, kFrame, unused));
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
    writeGlideStudy(outDir);

    std::cout << "OverSamplingExplore: wrote plot data, spectrogram grids and WAVs into " << outDir << std::endl;
}
