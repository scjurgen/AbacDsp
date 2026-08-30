// Verification plots for the FDN tanks examples actually use (src/includes/Reverbs/):
// FdnTankGlide, FdnTankSpicedBase, FdnTankBlockDelayWalshSIMD. Late-tail spectral flatness,
// RT60 vs. frequency band, and Schroeder energy-decay curves. PyConPlot.py output; README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Filters/Biquad.h"
#include "Reverbs/FdnTankBlockDelayWalshSIMD.h"
#include "Reverbs/FdnTankGlide.h"
#include "Reverbs/FdnTankSpicedBase.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kMaxSizePerElement = 96000; // matches examples' own generous bound (2s @48kHz)
constexpr size_t kOrder = 16;
constexpr size_t kBlockSize = 480;

// minSize kept comfortably above the ~6.7m below which a line's discrete sample length would be
// shorter than 2*kBlockSize (see FdnTankSpicedBase::setDirectSize()'s clamp).
constexpr float kMinSizeM = 8.f;
constexpr float kMaxSizeM = 20.f;
constexpr float kDecayMs = 2000.f;

using GlideTank = AbacDsp::FdnTankGlide<kMaxSizePerElement, kOrder, kBlockSize>;
using SpicedTank = AbacDsp::FdnTankSpicedBase<kMaxSizePerElement, kOrder, kBlockSize>;
using WalshTank = AbacDsp::FdnTankBlockDelayWalshSIMD<kMaxSizePerElement, kOrder, kBlockSize>;

// Feeds a positive unit impulse through tank and captures numBlocks*kBlockSize samples of
// output via its fixed-BlockSize processBlock(in, out).
template <typename Tank>
[[nodiscard]] std::vector<float> renderImpulseResponse(Tank& tank, const size_t numBlocks)
{
    std::vector<float> rendered(numBlocks * kBlockSize, 0.f);
    std::array<float, kBlockSize> in{};
    std::array<float, kBlockSize> out{};
    in[0] = 1.f;
    for (size_t b = 0; b < numBlocks; ++b)
    {
        tank.processBlock(in.data(), out.data());
        std::copy(out.begin(), out.end(), rendered.begin() + static_cast<std::ptrdiff_t>(b * kBlockSize));
        in.fill(0.f);
    }
    return rendered;
}

// ---- Spectral flatness ----

constexpr size_t kFftSize = 16384; // high resolution: one-shot offline generation, not a hot path
constexpr float kSnapshotSeconds = 0.3f;

void writeMagnitudeSpectrum(std::ofstream& out, const std::vector<float>& window)
{
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(window, magnitude);
    for (size_t bin = 1; bin < kFftSize / 2; ++bin)
    {
        const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
        const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
        out << hz << " " << db << "\n";
    }
}

void writeSpectralComparison(std::ofstream& out)
{
    const auto numBlocks = static_cast<size_t>(1.f * kSampleRate) / kBlockSize;
    const auto start = static_cast<size_t>(kSnapshotSeconds * kSampleRate);

    const auto snapshotAndWrite = [&](auto& tank, const char* title, const char* name)
    {
        out << "@New plot: title=\"" << title << "\"\n";
        const auto rendered = renderImpulseResponse(tank, numBlocks);
        const std::vector<float> window(rendered.begin() + static_cast<std::ptrdiff_t>(start),
                                        rendered.begin() + static_cast<std::ptrdiff_t>(start + kFftSize));
        out << "#" << name << "\n";
        writeMagnitudeSpectrum(out, window);
    };

    GlideTank glide(kSampleRate);
    glide.setMinSize(kMinSizeM);
    glide.setMaxSize(kMaxSizeM);
    glide.setDecay(kDecayMs);
    snapshotAndWrite(glide, "FdnTankGlide (damped, 6kHz)", "FdnTankGlide");

    SpicedTank spiced(kSampleRate);
    spiced.setMinSize(kMinSizeM);
    spiced.setMaxSize(kMaxSizeM);
    spiced.setDecay(kDecayMs);
    snapshotAndWrite(spiced, "FdnTankSpicedBase (undamped)", "FdnTankSpicedBase");

    WalshTank walsh(kSampleRate);
    walsh.setMinSize(kMinSizeM);
    walsh.setMaxSize(kMaxSizeM);
    walsh.setDecay(kDecayMs);
    snapshotAndWrite(walsh, "FdnTankBlockDelayWalshSIMD (undamped)", "FdnTankBlockDelayWalshSIMD");
}

// ---- RT60 per frequency band ----

constexpr std::array<float, 8> kBandCentersHz{125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f};
constexpr float kBandQ = 4.f;
constexpr size_t kRt60EnvelopeWindow = 240; // 5ms

[[nodiscard]] float measureRT60(const std::vector<float>& signal, const float centerHz)
{
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::BandPass> bp;
    bp.computeCoefficients(kSampleRate, centerHz, kBandQ, 0.f);
    std::vector<float> filtered(signal.size());
    bp.processBlock(signal.data(), filtered.data(), signal.size());

    std::vector<float> envelopeDb;
    envelopeDb.reserve(filtered.size() / kRt60EnvelopeWindow);
    for (size_t start = 0; start + kRt60EnvelopeWindow <= filtered.size(); start += kRt60EnvelopeWindow)
    {
        float peak = 0.f;
        for (size_t i = start; i < start + kRt60EnvelopeWindow; ++i)
        {
            peak = std::max(peak, std::abs(filtered[i]));
        }
        envelopeDb.push_back(20.f * std::log10(std::max(peak, 1e-9f)));
    }

    const auto peakIt = std::max_element(envelopeDb.begin(), envelopeDb.end());
    const float peakDb = *peakIt;
    const auto peakIndex = static_cast<size_t>(std::distance(envelopeDb.begin(), peakIt));
    size_t lastAbove = peakIndex;
    for (size_t i = peakIndex; i < envelopeDb.size(); ++i)
    {
        if (envelopeDb[i] > peakDb - 60.f)
        {
            lastAbove = i;
        }
    }
    return static_cast<float>(lastAbove - peakIndex) * static_cast<float>(kRt60EnvelopeWindow) / kSampleRate;
}

void writeRt60ByBand(std::ofstream& out)
{
    out << "@New plot: title=\"FDN tank RT60 vs. frequency band (order=16, decay=2000ms "
           "target)\"\n";
    const auto numBlocks = static_cast<size_t>(3.f * kSampleRate) / kBlockSize;

    const auto measureAndWrite = [&](auto& tank, const char* name)
    {
        const auto rendered = renderImpulseResponse(tank, numBlocks);
        out << "#" << name << "\n";
        for (const float hz : kBandCentersHz)
        {
            out << hz << " " << measureRT60(rendered, hz) << "\n";
        }
    };

    GlideTank glide(kSampleRate);
    glide.setMinSize(kMinSizeM);
    glide.setMaxSize(kMaxSizeM);
    glide.setDecay(kDecayMs);
    measureAndWrite(glide, "FdnTankGlide (damped, 6kHz)");

    SpicedTank spiced(kSampleRate);
    spiced.setMinSize(kMinSizeM);
    spiced.setMaxSize(kMaxSizeM);
    spiced.setDecay(kDecayMs);
    measureAndWrite(spiced, "FdnTankSpicedBase (undamped)");

    WalshTank walsh(kSampleRate);
    walsh.setMinSize(kMinSizeM);
    walsh.setMaxSize(kMaxSizeM);
    walsh.setDecay(kDecayMs);
    measureAndWrite(walsh, "FdnTankBlockDelayWalshSIMD (undamped)");
}

// ---- Energy decay curve (Schroeder integration) ----

// Backward cumulative energy, normalized to the signal's own total - the standard reverberation
// decay-curve estimator (Schroeder 1965), far smoother than a raw envelope at low levels.
[[nodiscard]] std::vector<float> schroederEdcDb(const std::vector<float>& signal)
{
    double total = 0.0;
    for (const float x : signal)
    {
        total += static_cast<double>(x) * x;
    }
    std::vector<float> edc(signal.size());
    double running = total;
    for (size_t i = 0; i < signal.size(); ++i)
    {
        edc[i] = 10.f * static_cast<float>(std::log10(std::max(running / total, 1e-12)));
        running -= static_cast<double>(signal[i]) * signal[i];
    }
    return edc;
}

void writeDecayComparison(std::ofstream& out)
{
    out << "@New plot: title=\"FDN tank energy decay curve, full band (decay=2000ms target)\"\n";
    const auto numBlocks = static_cast<size_t>(3.f * kSampleRate) / kBlockSize;
    constexpr size_t kHop = 240; // 5ms

    const auto writeEdc = [&](auto& tank, const char* name)
    {
        const auto rendered = renderImpulseResponse(tank, numBlocks);
        const auto edc = schroederEdcDb(rendered);
        out << "#" << name << "\n";
        for (size_t i = 0; i < edc.size(); i += kHop)
        {
            out << (static_cast<float>(i) / kSampleRate) << " " << edc[i] << "\n";
        }
    };

    WalshTank walsh(kSampleRate);
    walsh.setMinSize(kMinSizeM);
    walsh.setMaxSize(kMaxSizeM);
    walsh.setDecay(kDecayMs);
    writeEdc(walsh, "FdnTankBlockDelayWalshSIMD (undamped, reference)");

    SpicedTank spiced(kSampleRate);
    spiced.setMinSize(kMinSizeM);
    spiced.setMaxSize(kMaxSizeM);
    spiced.setDecay(kDecayMs);
    writeEdc(spiced, "FdnTankSpicedBase (undamped)");

    GlideTank glideDefault(kSampleRate);
    glideDefault.setMinSize(kMinSizeM);
    glideDefault.setMaxSize(kMaxSizeM);
    glideDefault.setDecay(kDecayMs);
    writeEdc(glideDefault, "FdnTankGlide (damping=6kHz, default)");

    GlideTank glideBright(kSampleRate);
    glideBright.setMinSize(kMinSizeM);
    glideBright.setMaxSize(kMaxSizeM);
    glideBright.setDecay(kDecayMs);
    glideBright.setDamping(2000.f);
    writeEdc(glideBright, "FdnTankGlide (damping=2kHz, brighter cut)");
}
}

int main(int argc, char* argv[])
{
    const std::string spectralPath = argc > 1 ? argv[1] : "rv_spectral.txt";
    const std::string rt60Path = argc > 2 ? argv[2] : "rv_rt60.txt";
    const std::string decayPath = argc > 3 ? argv[3] : "rv_decay.txt";

    std::ofstream spectralOut(spectralPath);
    std::ofstream rt60Out(rt60Path);
    std::ofstream decayOut(decayPath);
    if (!spectralOut || !rt60Out || !decayOut)
    {
        std::cerr << "FdnTankExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeSpectralComparison(spectralOut);
    writeRt60ByBand(rt60Out);
    writeDecayComparison(decayOut);

    std::cout << "FdnTankExplore: wrote " << spectralPath << ", " << rt60Path << ", and " << decayPath << std::endl;
}
