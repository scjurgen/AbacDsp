// Verification plots for src/includes/Filters/PinkFilter.h: measured power spectral density
// against the theoretical -3dB/octave pink slope, and each variant's accuracy (FastPink=true:
// 3 poles, claimed +/-0.5dB; FastPink=false: 7 poles, claimed +/-0.05dB). See README.md.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Filters/PinkFilter.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kFftSize = 4096;
constexpr size_t kNumSegments = 500'000;

[[nodiscard]] std::vector<float> logSweepHz(const float lowHz, const float highHz, const size_t count)
{
    std::vector<float> out(count);
    for (size_t i = 0; i < count; ++i)
    {
        const auto frac = static_cast<float>(i) / static_cast<float>(count - 1);
        out[i] = lowHz * std::pow(highHz / lowHz, frac);
    }
    return out;
}

// Welch's method: average the squared magnitude of many Hann-windowed segments of a
// stationary random signal into a smooth power spectral density estimate, in dB.
template <bool FastPink>
[[nodiscard]] std::vector<float> measurePinkPsdDb()
{
    auto noise = AbacDsp::FFTResponse::generateNoiseSignal(kFftSize * kNumSegments);
    AbacDsp::PinkFilter<FastPink> filter;
    std::vector<float> filtered(noise.size());
    for (auto& v : noise)
    {
        v *= 1.2;
    }
    filter.processBlock(noise.data(), filtered.data(), noise.size());

    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    std::vector<double> powerSum(kFftSize / 2, 0.0);
    std::vector<float> segment(kFftSize);
    for (size_t s = 0; s < kNumSegments; ++s)
    {
        std::copy_n(filtered.data() + s * kFftSize, kFftSize, segment.begin());
        fft.compute(segment, magnitude);
        for (size_t bin = 0; bin < magnitude.size(); ++bin)
        {
            powerSum[bin] += static_cast<double>(magnitude[bin]) * magnitude[bin];
        }
    }

    std::vector<float> db(kFftSize / 2);
    for (size_t bin = 0; bin < db.size(); ++bin)
    {
        const double meanPower = powerSum[bin] / static_cast<double>(kNumSegments);
        db[bin] = 10.f * static_cast<float>(std::log10(std::max(meanPower, 1e-18)));
    }
    return db;
}

[[nodiscard]] float dbAtHz(const std::vector<float>& db, const float hz)
{
    const auto bin = static_cast<size_t>(std::lround(hz * static_cast<float>(kFftSize) / kSampleRate));
    return db[std::clamp<size_t>(bin, 1, db.size() - 1)];
}

void writeSpectrum(std::ofstream& out)
{
    out << "@New plot: title=\"PinkFilter measured PSD vs. ideal -3dB/octave (fitted at 1kHz)\" "
           "logx=true\n";
    const auto fastDb = measurePinkPsdDb<true>();
    const auto accurateDb = measurePinkPsdDb<false>();
    constexpr float f0 = 1000.f;
    const float anchorDb = dbAtHz(accurateDb, f0);

    const auto writeSeries = [&](const char* name, const std::vector<float>& db)
    {
        out << "#" << name << "\n";
        for (size_t bin = 1; bin < db.size(); ++bin)
        {
            const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
            out << hz << " " << db[bin] << "\n";
        }
    };
    writeSeries("FastPink=true (3 poles)", fastDb);
    writeSeries("FastPink=false (7 poles)", accurateDb);

    out << "#ideal -3dB/octave\n";
    const auto sweep = logSweepHz(20.f, 20000.f, 100);
    for (const float hz : sweep)
    {
        out << hz << " " << (anchorDb - 10.f * std::log10(hz / f0)) << "\n";
    }
}

void writeAccuracy(std::ofstream& out)
{
    const auto fastDb = measurePinkPsdDb<true>();
    const auto accurateDb = measurePinkPsdDb<false>();
    constexpr float f0 = 1000.f;

    const auto writeError = [&](const std::vector<float>& db, const char* title, const char* name)
    {
        out << "@New plot: title=\"" << title << "\" logx=true\n#" << name << "\n";
        const float anchorDb = dbAtHz(db, f0);
        for (size_t bin = 1; bin < db.size(); ++bin)
        {
            const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
            if (hz < 20.f || hz > 20000.f)
            {
                continue;
            }
            const float ideal = anchorDb - 10.f * std::log10(hz / f0);
            out << hz << " " << (db[bin] - ideal) << "\n";
        }
    };
    writeError(fastDb, "PinkFilter<true> (3 poles) error vs. ideal -3dB/octave (claimed +/-0.5dB)",
               "FastPink=true error");
    writeError(accurateDb, "PinkFilter<false> (7 poles) error vs. ideal -3dB/octave (claimed +/-0.05dB)",
               "FastPink=false error");
}
}

int main(int argc, char* argv[])
{
    const std::string spectrumPath = argc > 1 ? argv[1] : "pf_spectrum.txt";
    const std::string accuracyPath = argc > 2 ? argv[2] : "pf_accuracy.txt";

    std::ofstream spectrumOut(spectrumPath);
    std::ofstream accuracyOut(accuracyPath);
    if (!spectrumOut || !accuracyOut)
    {
        std::cerr << "PinkFilterExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeSpectrum(spectrumOut);
    writeAccuracy(accuracyOut);

    std::cout << "PinkFilterExplore: wrote " << spectrumPath << " and " << accuracyPath << std::endl;
}
