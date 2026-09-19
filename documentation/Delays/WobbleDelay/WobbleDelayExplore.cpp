// Verification plots for WobbleDelay, the single-rate delay whose read head is modulated by
// Wow and Flutter: tracked playback pitch, the read/write distance under deep modulation
// (crossing clamp), a retune glide, and swept-tone spectrograms. See README.md.

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include "Analysis/ZeroCrossings.h"
#include "Delays/WobbleDelay.h"
#include "DelaysExploreCommon.h"

namespace
{
constexpr size_t kBufferSize = 48000;
using Delay = AbacDsp::WobbleDelay<kBufferSize, kTileSize>;

enum class Modulation
{
    None,
    Flutter,
    Wow,
    Both
};

void configureWow(Delay& delay)
{
    delay.seed(7);
    delay.setWowRate(1.5f);
    delay.setWowDepth(0.8f);
    delay.setWowVariance(0.3f);
    delay.setWowDrift(0.f);
}

void configureFlutter(Delay& delay)
{
    delay.setFlutterRate(8.f);
    delay.setFlutterDepth(0.8f);
}

void configure(Delay& delay, const Modulation modulation)
{
    if (modulation == Modulation::Wow || modulation == Modulation::Both)
    {
        configureWow(delay);
    }
    if (modulation == Modulation::Flutter || modulation == Modulation::Both)
    {
        configureFlutter(delay);
    }
}

[[nodiscard]] std::vector<float> makeTone(const float hz, const size_t numSamples)
{
    std::vector<float> tone(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        tone[i] = 0.5f * std::sin(2.f * std::numbers::pi_v<float> * hz * static_cast<float>(i) / kSampleRate);
    }
    return tone;
}

[[nodiscard]] std::vector<float> makeSweep(const float startHz, const float endHz, const size_t numSamples)
{
    std::vector<float> tone(numSamples);
    float phase = 0.f;
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float frac = static_cast<float>(i) / static_cast<float>(numSamples - 1);
        tone[i] = 0.5f * std::sin(phase);
        phase += 2.f * std::numbers::pi_v<float> * (startHz + (endHz - startHz) * frac) / kSampleRate;
    }
    return tone;
}

[[nodiscard]] std::vector<float> render(Delay& delay, const std::vector<float>& input)
{
    std::vector<float> output(input.size());
    for (size_t pos = 0; pos < input.size(); pos += kTileSize)
    {
        const auto count = std::min(kTileSize, input.size() - pos);
        delay.processBlock(std::span<const float>{input}.subspan(pos, count),
                           std::span<float>{output}.subspan(pos, count));
    }
    return output;
}

void writePitchWobble(std::ofstream& out)
{
    out << "@New plot: title=\"WobbleDelay: tracked playback pitch, wow+flutter on vs. off (220Hz probe)\"\n";
    constexpr float kProbeHz = 220.f;
    constexpr float kDelaySamples = 2000.f;
    const auto input = makeTone(kProbeHz, static_cast<size_t>(5.f * kSampleRate));

    for (const bool modulated : {false, true})
    {
        Delay delay(kSampleRate);
        delay.setDelay(kDelaySamples, true);
        configure(delay, modulated ? Modulation::Both : Modulation::None);
        const auto rendered = render(delay, input);

        out << "#" << (modulated ? "wow+flutter on" : "wow+flutter off") << "\n";
        for (size_t start = 0; start + kPitchWindow <= rendered.size(); start += kPitchHop)
        {
            const float period =
                AbacDsp::periodLengthByZeroCrossingAverage(rendered.data() + start, kPitchWindow, true);
            if (period > 0.f)
            {
                out << static_cast<float>(start) / kSampleRate << " " << (kSampleRate / period) << "\n";
            }
        }
    }
}

// Deep wow and flutter around a base delay barely above the margin, so the clamp engages.
void writeDelayStability(std::ofstream& out)
{
    out << "@New plot: title=\"WobbleDelay: read/write distance under deep modulation, safety margin 30\"\n";
    constexpr float kMargin = 30.f;
    constexpr size_t kHop = 96;
    constexpr size_t kNumSamples = 20 * static_cast<size_t>(kSampleRate);

    Delay delay(kSampleRate);
    delay.setSafetyMargin(kMargin);
    delay.setDelay(60.f, true);
    delay.seed(11);
    delay.setWowRate(1.f);
    delay.setWowDepth(1.f);
    delay.setWowVariance(0.5f);
    delay.setFlutterRate(8.f);
    delay.setFlutterDepth(3.f);

    out << "#distance (samples)\n";
    std::vector<std::pair<float, float>> margin;
    for (size_t i = 0; i < kNumSamples; ++i)
    {
        static_cast<void>(delay.step(0.f));
        if (i % kHop == 0)
        {
            const float time = static_cast<float>(i) / kSampleRate;
            out << time << " " << delay.currentDelay() << "\n";
            margin.emplace_back(time, kMargin);
        }
    }
    out << "#safety margin\n";
    for (const auto& [time, value] : margin)
    {
        out << time << " " << value << "\n";
    }
}

// Delay distance while retuning 200 -> 6000 and back down to 1000, on silence.
void writeRetuneCurve(std::ofstream& out)
{
    out << "@New plot: title=\"WobbleDelay: retune glide, 200 -> 6000 -> 1000 samples\"\n";
    constexpr size_t kHop = 32;
    Delay delay(kSampleRate);
    delay.setDelay(200.f, true);

    out << "#distance (samples)\n";
    const auto record = [&](const size_t numSamples, const size_t startIndex)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            static_cast<void>(delay.step(0.f));
            if (i % kHop == 0)
            {
                out << static_cast<float>(startIndex + i) / kSampleRate << " " << delay.currentDelay() << "\n";
            }
        }
    };
    constexpr size_t kSegment = 20000;
    record(kSegment / 4, 0);
    delay.setDelay(6000.f);
    record(kSegment, kSegment / 4);
    delay.setDelay(1000.f);
    record(kSegment, kSegment / 4 + kSegment);
}

void writeSweep(const std::string& outDir, const std::string& name, Delay& delay)
{
    const auto input = makeSweep(50.f, 20000.f, static_cast<size_t>(4.f * kSampleRate));
    const auto audio = render(delay, input);
    std::ofstream grid(outDir + "/wd_sweep_" + name + ".txt");
    writeSpectrogramGrid(grid, audio, 4096);
    AudioUtility::SaveWav::saveMonoAs(outDir + "/wd_sweep_" + name + ".wav", audio, kSampleRate);
}

void writeSweeps(const std::string& outDir)
{
    const std::array<std::pair<const char*, Modulation>, 4> cases{{{"plain", Modulation::None},
                                                                   {"flutter", Modulation::Flutter},
                                                                   {"wow", Modulation::Wow},
                                                                   {"both", Modulation::Both}}};
    for (const auto& [name, modulation] : cases)
    {
        Delay delay(kSampleRate);
        delay.setDelay(500.f, true);
        configure(delay, modulation);
        writeSweep(outDir, name, delay);
    }
    Delay retune(kSampleRate);
    retune.setDelay(200.f, true);
    retune.setDelay(6000.f);
    writeSweep(outDir, "retune", retune);
}
}

int main(int argc, char* argv[])
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    std::ofstream pitchOut(outDir + "/wd_pitchwobble.txt");
    std::ofstream stabilityOut(outDir + "/wd_delaystability.txt");
    std::ofstream retuneOut(outDir + "/wd_retune.txt");
    if (!pitchOut || !stabilityOut || !retuneOut)
    {
        std::cerr << "WobbleDelayExplore: ERROR - failed to open output files in " << outDir << std::endl;
        return 1;
    }

    writePitchWobble(pitchOut);
    writeDelayStability(stabilityOut);
    writeRetuneCurve(retuneOut);
    writeSweeps(outDir);

    std::cout << "WobbleDelayExplore: wrote plot data, spectrogram grids and WAVs into " << outDir << std::endl;
}
