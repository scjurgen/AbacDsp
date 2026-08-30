// Verification plots for src/includes/Modulation/: Wow, Flutter (LFO character: timelines,
// spectra, Poincare-style orbit plots), Tremolo (drive sine->square morph), RingModulator
// (sideband spectrum). PyConPlot.py output; see README.md.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <tuple>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Analysis/ZeroCrossings.h"
#include "Modulation/Flutter.h"
#include "Modulation/RingModulator.h"
#include "Modulation/Tremolo.h"
#include "Modulation/Wow.h"

namespace
{
constexpr float kSampleRate = 48000.f;

[[nodiscard]] std::vector<float> renderWow(const float rate, const float depth, const float drift,
                                           const size_t numSamples)
{
    AbacDsp::Wow wow(kSampleRate);
    wow.setRate(rate);
    wow.setDepth(depth);
    wow.setDrift(drift);
    wow.setVariance(0.5f);
    for (size_t i = 0; i < 500; ++i)
    {
        std::ignore = wow.step();
    }
    std::vector<float> out(numSamples);
    std::ranges::generate(out, [&wow] { return wow.step(); });
    return out;
}

[[nodiscard]] std::vector<float> renderFlutter(const float rate, const float depth, const size_t numSamples)
{
    AbacDsp::Flutter flutter(kSampleRate);
    flutter.setRate(rate);
    flutter.setDepth(depth);
    for (size_t i = 0; i < 24000; ++i) // clear the 0.1s rate/depth smoothing
    {
        std::ignore = flutter.step();
    }
    std::vector<float> out(numSamples);
    std::ranges::generate(out, [&flutter] { return flutter.step(); });
    return out;
}

// ---- Timelines ----

void writeTimelines(std::ofstream& out)
{
    constexpr auto numSamples = static_cast<size_t>(6.f * kSampleRate);
    constexpr size_t kHop = 48; // 1kHz effective: 20x oversampled for content under 50Hz

    out << "@New plot: title=\"Wow timeline: drift=0 vs. drift=0.3 (rate=0.5Hz, depth=1)\"\n";
    for (const float drift : {0.f, 0.3f})
    {
        const auto rendered = renderWow(0.5f, 1.f, drift, numSamples);
        out << "#drift=" << drift << "\n";
        for (size_t i = 0; i < rendered.size(); i += kHop)
        {
            out << (static_cast<float>(i) / kSampleRate) << " " << rendered[i] << "\n";
        }
    }

    out << "@New plot: title=\"Flutter timeline: two rate/depth settings\"\n";
    for (const auto& [rate, depth] : {std::pair{6.f, 30.f}, std::pair{20.f, 60.f}})
    {
        const auto rendered = renderFlutter(rate, depth, numSamples);
        out << "#rate=" << rate << "Hz depth=" << depth << "\n";
        for (size_t i = 0; i < rendered.size(); i += kHop)
        {
            out << (static_cast<float>(i) / kSampleRate) << " " << rendered[i] << "\n";
        }
    }
}

// ---- Spectra ----

constexpr size_t kSpectralDecimation = 48; // 1kHz effective rate
constexpr size_t kSpectralFftSize = 8192;  // ~0.12Hz resolution at the decimated rate

void writeDecimatedSpectrum(std::ofstream& out, const std::vector<float>& signal)
{
    std::vector<float> decimated(signal.size() / kSpectralDecimation);
    for (size_t i = 0; i < decimated.size(); ++i)
    {
        decimated[i] = signal[i * kSpectralDecimation];
    }
    AbacDsp::HannWindowMagnitudesFft fft(kSpectralFftSize);
    std::vector<float> magnitude(kSpectralFftSize / 2);
    const std::vector<float> window(decimated.begin(), decimated.begin() + kSpectralFftSize);
    fft.compute(window, magnitude);
    const float decimatedRate = kSampleRate / static_cast<float>(kSpectralDecimation);
    for (size_t bin = 1; bin < kSpectralFftSize / 2; ++bin)
    {
        const float hz = static_cast<float>(bin) * decimatedRate / static_cast<float>(kSpectralFftSize);
        const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
        out << hz << " " << db << "\n";
    }
}

void writeSpectra(std::ofstream& out)
{
    constexpr auto numSamples = static_cast<size_t>(10.f * kSampleRate);

    out << "@New plot: title=\"Wow spectrum (rate=2Hz, depth=1, drift=0)\" logx=true\n#Wow\n";
    writeDecimatedSpectrum(out, renderWow(2.f, 1.f, 0.f, numSamples));

    out << "@New plot: title=\"Flutter spectrum (rate=8Hz, depth=50)\" logx=true\n#Flutter\n";
    writeDecimatedSpectrum(out, renderFlutter(8.f, 50.f, numSamples));
}

// ---- Orbit (Poincare-style) plots ----

// x[n] vs. x[n+delta], delta a quarter of the signal's own measured average period - a closed
// loop means genuinely periodic motion, a diffuse cloud means it never repeats (Abel & Huang-
// style diagnostic, same idea as a phase-plane/Poincare section).
void writeOrbit(std::ofstream& out, const std::vector<float>& signal, const char* name)
{
    const float period = AbacDsp::periodLengthByZeroCrossingAverage(signal.data(), signal.size(), true);
    const auto delta = static_cast<size_t>(std::max(1.f, period / 4.f));
    out << "#" << name << " (period=" << period << " samples)\n";
    for (size_t i = 0; i + delta < signal.size(); ++i)
    {
        out << signal[i] << " " << signal[i + delta] << "\n";
    }
}

void writeOrbits(std::ofstream& out)
{
    constexpr auto numSamples = static_cast<size_t>(2.f * kSampleRate);
    constexpr size_t kHop = 8; // 6kHz effective: still >100x the signals' own content

    const auto decimate = [](const std::vector<float>& signal)
    {
        std::vector<float> out;
        out.reserve(signal.size() / kHop);
        for (size_t i = 0; i < signal.size(); i += kHop)
        {
            out.push_back(signal[i]);
        }
        return out;
    };

    out << "@New plot: title=\"Wow orbit (rate=0.5Hz, depth=1, drift=0.3): never closes\" "
           "aspect=equal\n";
    writeOrbit(out, decimate(renderWow(0.5f, 1.f, 0.3f, numSamples)), "Wow");

    out << "@New plot: title=\"Flutter orbit (rate=8Hz, depth=50): closes on itself\" "
           "aspect=equal\n";
    writeOrbit(out, decimate(renderFlutter(8.f, 50.f, numSamples)), "Flutter");
}

// ---- Tremolo drive morph + RingModulator sidebands ----

void writeTremoloRingMod(std::ofstream& out)
{
    out << "@New plot: title=\"Tremolo gain waveform vs. drive (rate=2Hz, depth=1)\"\n";
    constexpr auto tremoloSamples = static_cast<size_t>(0.5f * kSampleRate); // one full 2Hz cycle
    for (const float drive : {0.f, 0.3f, 0.6f, 1.f})
    {
        AbacDsp::Tremolo tremolo(kSampleRate);
        tremolo.setRate(2.f);
        tremolo.setDepth(1.f);
        tremolo.setDrive(drive);
        out << "#drive=" << drive << "\n";
        for (size_t i = 0; i < tremoloSamples; ++i)
        {
            const float t = static_cast<float>(i) / kSampleRate;
            out << t << " " << tremolo.step(1.f) << "\n";
        }
    }

    out << "@New plot: title=\"RingModulator sidebands (1000Hz probe tone)\" logx=true\n";
    constexpr size_t kRingFftSize = 16384;
    constexpr float kProbeHz = 1000.f;
    for (const float carrierHz : {300.f, 1500.f})
    {
        AbacDsp::RingModulator ring(kSampleRate);
        ring.setFrequency(carrierHz);
        std::vector<float> rendered(kRingFftSize);
        float phase = 0.f;
        const float phaseInc = 2.f * std::numbers::pi_v<float> * kProbeHz / kSampleRate;
        for (float& sample : rendered)
        {
            sample = ring.step(std::sin(phase));
            phase += phaseInc;
        }
        AbacDsp::HannWindowMagnitudesFft fft(kRingFftSize);
        std::vector<float> magnitude(kRingFftSize / 2);
        fft.compute(rendered, magnitude);
        out << "#carrier=" << carrierHz << "Hz\n";
        for (size_t bin = 1; bin < kRingFftSize / 2; ++bin)
        {
            const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kRingFftSize);
            const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
            out << hz << " " << db << "\n";
        }
    }
}
}

int main(int argc, char* argv[])
{
    const std::string timelinesPath = argc > 1 ? argv[1] : "md_timelines.txt";
    const std::string spectraPath = argc > 2 ? argv[2] : "md_spectra.txt";
    const std::string orbitPath = argc > 3 ? argv[3] : "md_orbit.txt";
    const std::string tremoloRingModPath = argc > 4 ? argv[4] : "md_tremolo_ringmod.txt";

    std::ofstream timelinesOut(timelinesPath);
    std::ofstream spectraOut(spectraPath);
    std::ofstream orbitOut(orbitPath);
    std::ofstream tremoloRingModOut(tremoloRingModPath);
    if (!timelinesOut || !spectraOut || !orbitOut || !tremoloRingModOut)
    {
        std::cerr << "ModulationExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeTimelines(timelinesOut);
    writeSpectra(spectraOut);
    writeOrbits(orbitOut);
    writeTremoloRingMod(tremoloRingModOut);

    std::cout << "ModulationExplore: wrote " << timelinesPath << ", " << spectraPath << ", " << orbitPath << ", and "
              << tremoloRingModPath << std::endl;
}
