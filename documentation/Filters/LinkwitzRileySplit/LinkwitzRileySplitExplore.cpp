// Verification plots for src/includes/Filters/LinkwitzRileySplit.h: measured band magnitudes at several split
// frequencies, their deviation from the closed-form digital response, and the allpass behaviour of the sum.
// Plain-text PyConPlot.py output; see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include "Filters/Biquad.h"
#include "Filters/LinkwitzRileySplit.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr float kButterworthQ = 0.70710678f;
constexpr size_t kSettleSamples = 4800;
constexpr size_t kMeasureSamples = 48000; // one second: an integer number of cycles at whole-Hz frequencies
constexpr std::array<float, 4> kSplitFrequencies{200.f, 1000.f, 5000.f, 15000.f};

[[nodiscard]] std::vector<float> logSweepHz(const float lowHz, const float highHz, const size_t count)
{
    std::vector<float> out(count);
    for (size_t i = 0; i < count; ++i)
    {
        const auto frac = static_cast<float>(i) / static_cast<float>(count - 1);
        out[i] = std::round(lowHz * std::pow(highHz / lowHz, frac));
    }
    return out;
}

struct Response
{
    std::complex<double> low;
    std::complex<double> high;
    std::complex<double> sum;
    std::complex<double> allpass;
};

// Complex gain of the bands relative to a unit sine, via correlation against exp(-j*phase).
[[nodiscard]] Response measure(const float splitHz, const float testHz)
{
    AbacDsp::LinkwitzRileySplit split{kSampleRate};
    split.setFrequency(splitHz);
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::AllPass> allpass;
    allpass.computeCoefficients(kSampleRate, splitHz, kButterworthQ, 0.f);

    Response acc{};
    const double phaseInc = 2.0 * std::numbers::pi * static_cast<double>(testHz) / static_cast<double>(kSampleRate);
    const auto scale = 2.0 / static_cast<double>(kMeasureSamples);
    for (size_t n = 0; n < kSettleSamples + kMeasureSamples; ++n)
    {
        const auto in = static_cast<float>(std::sin(phaseInc * static_cast<double>(n)));
        const auto [low, high] = split.step(in);
        const float allpassed = allpass.singleStepAllPass(in);
        if (n < kSettleSamples)
        {
            continue;
        }
        const std::complex<double> ref{std::cos(phaseInc * static_cast<double>(n)),
                                       -std::sin(phaseInc * static_cast<double>(n))};
        acc.low += static_cast<double>(low) * ref * scale;
        acc.high += static_cast<double>(high) * ref * scale;
        acc.sum += (static_cast<double>(low) + static_cast<double>(high)) * ref * scale;
        acc.allpass += static_cast<double>(allpassed) * ref * scale;
    }
    const std::complex<double> inputTerm{0.0, -1.0};
    acc.low /= inputTerm;
    acc.high /= inputTerm;
    acc.sum /= inputTerm;
    acc.allpass /= inputTerm;
    return acc;
}

// |H|^2 of a 2nd-order Butterworth lowpass is 1 / (1 + w^4); squaring the magnitude of the cascade gives
// |LP| = 1 / (1 + w^4) and |HP| = w^4 / (1 + w^4), with w = tan(pi f / fs) / tan(pi fc / fs) for the bilinear
// transform used by Biquad. The analog curve is the same with w = f / fc.
[[nodiscard]] double warped(const double hz, const double splitHz, const bool bilinear)
{
    if (!bilinear)
    {
        return hz / splitHz;
    }
    const double pi = std::numbers::pi;
    return std::tan(pi * hz / kSampleRate) / std::tan(pi * splitHz / kSampleRate);
}

[[nodiscard]] double theoryLow(const double hz, const double splitHz, const bool bilinear)
{
    return 1.0 / (1.0 + std::pow(warped(hz, splitHz, bilinear), 4.0));
}

[[nodiscard]] double theoryHigh(const double hz, const double splitHz, const bool bilinear)
{
    return 1.0 - theoryLow(hz, splitHz, bilinear);
}

[[nodiscard]] double toDb(const double linear)
{
    return 20.0 * std::log10(std::max(linear, 1e-6));
}

[[nodiscard]] double lagDegrees(const std::complex<double>& value)
{
    auto deg = std::arg(value) * 180.0 / std::numbers::pi;
    if (deg > 1.0)
    {
        deg -= 360.0;
    }
    return deg;
}

std::string label(const char* prefix, const float splitHz)
{
    return std::string{prefix} + " " + std::to_string(static_cast<int>(splitHz)) + " Hz";
}

void writeBands(std::ofstream& out, const std::vector<float>& sweep)
{
    out << "@New plot: title=\"LinkwitzRileySplit: band magnitudes at four split frequencies\" logx=true hzticks=true "
           "ystep=24\n";
    for (const float splitHz : kSplitFrequencies)
    {
        std::vector<Response> data;
        for (const float hz : sweep)
        {
            data.push_back(measure(splitHz, hz));
        }
        out << "#" << label("low", splitHz) << "\n";
        for (size_t i = 0; i < sweep.size(); ++i)
        {
            out << sweep[i] << " " << toDb(std::abs(data[i].low)) << "\n";
        }
        out << "#" << label("high", splitHz) << "\n";
        for (size_t i = 0; i < sweep.size(); ++i)
        {
            out << sweep[i] << " " << toDb(std::abs(data[i].high)) << "\n";
        }
    }
}

void writeTheory(std::ofstream& out, const std::vector<float>& sweep)
{
    constexpr float splitHz = 1000.f;
    std::vector<Response> data;
    for (const float hz : sweep)
    {
        data.push_back(measure(splitHz, hz));
    }
    out << "@New plot: title=\"LinkwitzRileySplit: measured minus bilinear theory (split=1kHz)\" logx=true "
           "hzticks=true\n";
    out << "#low\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        const double theory = theoryLow(sweep[i], splitHz, true);
        out << sweep[i] << " " << toDb(std::abs(std::abs(data[i].low) - theory)) << "\n";
    }
    out << "#high\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        const double theory = theoryHigh(sweep[i], splitHz, true);
        out << sweep[i] << " " << toDb(std::abs(std::abs(data[i].high) - theory)) << "\n";
    }

    out << "@New plot: title=\"LinkwitzRileySplit: low band, bilinear theory minus analog theory (split=1kHz)\" "
           "logx=true hzticks=true\n";
    out << "#warping of the digital response\n";
    for (const float hz : sweep)
    {
        out << hz << " " << toDb(theoryLow(hz, splitHz, true)) - toDb(theoryLow(hz, splitHz, false)) << "\n";
    }
}

void writeSum(std::ofstream& out, const std::vector<float>& sweep)
{
    constexpr float splitHz = 1000.f;
    std::vector<Response> data;
    for (const float hz : sweep)
    {
        data.push_back(measure(splitHz, hz));
    }
    out << "@New plot: title=\"LinkwitzRileySplit: low + high magnitude (split=1kHz)\" logx=true hzticks=true\n";
    out << "#low + high\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        out << sweep[i] << " " << toDb(std::abs(data[i].sum)) << "\n";
    }
    out << "@New plot: title=\"LinkwitzRileySplit: low + high phase vs. AllPass biquad (Q=1/sqrt2)\" logx=true "
           "hzticks=true ystep=45\n";
    out << "#low + high\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        out << sweep[i] << " " << lagDegrees(data[i].sum) << "\n";
    }
    out << "#AllPass biquad\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        out << sweep[i] << " " << lagDegrees(data[i].allpass) << "\n";
    }
    out << "#-180 degree reference\n" << sweep.front() << " -180\n" << sweep.back() << " -180\n";
    out << "@New plot: title=\"LinkwitzRileySplit: |(low + high) - AllPass biquad| (split=1kHz)\" logx=true "
           "hzticks=true\n";
    out << "#deviation from allpass\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        out << sweep[i] << " " << toDb(std::abs(data[i].sum - data[i].allpass)) << "\n";
    }
}
} // namespace

int main(int argc, char* argv[])
{
    const std::string bandsPath = argc > 1 ? argv[1] : "lrs_bands.txt";
    const std::string theoryPath = argc > 2 ? argv[2] : "lrs_theory.txt";
    const std::string sumPath = argc > 3 ? argv[3] : "lrs_sum.txt";

    std::ofstream bandsOut(bandsPath);
    std::ofstream theoryOut(theoryPath);
    std::ofstream sumOut(sumPath);
    if (!bandsOut || !theoryOut || !sumOut)
    {
        std::cerr << "LinkwitzRileySplitExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    const auto sweep = logSweepHz(20.f, 20000.f, 240);
    writeBands(bandsOut, sweep);
    writeTheory(theoryOut, sweep);
    writeSum(sumOut, sweep);
    std::cout << "LinkwitzRileySplitExplore: wrote " << bandsPath << ", " << theoryPath << " and " << sumPath
              << std::endl;
}
