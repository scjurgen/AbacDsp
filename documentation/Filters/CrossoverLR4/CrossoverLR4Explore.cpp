// Verification plots for src/includes/Graph/Nodes/CrossoverLR4.h: band magnitudes, the summed
// output's allpass behaviour, and its deviation from a reference AllPass biquad.
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
#include "Graph/Nodes/CrossoverLR4.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr float kCrossoverHz = 1000.f;
constexpr float kButterworthQ = 0.70710678f;
constexpr size_t kSettleSamples = 4800;
constexpr size_t kMeasureSamples = 48000; // one second: an integer number of cycles at whole-Hz frequencies

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

// Complex gain of one signal relative to a unit-amplitude sine, via correlation against exp(-j*phase).
struct Response
{
    std::complex<double> low;
    std::complex<double> high;
    std::complex<double> sum;
    std::complex<double> allpass;
};

[[nodiscard]] Response measure(const float testHz)
{
    AbacDsp::Graph::Nodes::CrossoverLR4 crossover{kSampleRate};
    crossover.setParameter(0, kCrossoverHz);
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::AllPass> allpass;
    allpass.computeCoefficients(kSampleRate, kCrossoverHz, kButterworthQ, 0.f);

    Response acc{};
    const double phaseInc = 2.0 * std::numbers::pi * static_cast<double>(testHz) / static_cast<double>(kSampleRate);
    for (size_t n = 0; n < kSettleSamples + kMeasureSamples; ++n)
    {
        const auto in = static_cast<float>(std::sin(phaseInc * static_cast<double>(n)));
        std::array<float, 2> out{};
        std::array<const float*, 1> ins{&in};
        std::array<float*, 2> outs{&out[0], &out[1]};
        crossover.process(ins, outs, 1);
        const float allpassed = allpass.singleStepAllPass(in);
        if (n < kSettleSamples)
        {
            continue;
        }
        const std::complex<double> ref{std::cos(phaseInc * static_cast<double>(n)),
                                       -std::sin(phaseInc * static_cast<double>(n))};
        const auto scale = 2.0 / static_cast<double>(kMeasureSamples);
        acc.low += static_cast<double>(out[0]) * ref * scale;
        acc.high += static_cast<double>(out[1]) * ref * scale;
        acc.sum += (static_cast<double>(out[0]) + static_cast<double>(out[1])) * ref * scale;
        acc.allpass += static_cast<double>(allpassed) * ref * scale;
    }
    // The input sine sin(p) correlates to -j, so divide it out to get the gain relative to the input.
    const std::complex<double> inputTerm{0.0, -1.0};
    acc.low /= inputTerm;
    acc.high /= inputTerm;
    acc.sum /= inputTerm;
    acc.allpass /= inputTerm;
    return acc;
}

[[nodiscard]] double toDb(const double linear)
{
    return 20.0 * std::log10(std::max(linear, 1e-6));
}

// Phase in (-360, 0]: a 2nd-order allpass only ever lags.
[[nodiscard]] double lagDegrees(const std::complex<double>& value)
{
    auto deg = std::arg(value) * 180.0 / std::numbers::pi;
    if (deg > 1.0)
    {
        deg -= 360.0;
    }
    return deg;
}

void writeSeries(std::ofstream& out, const char* name, const std::vector<float>& sweep,
                 const std::vector<Response>& data, double (*value)(const Response&))
{
    out << "#" << name << "\n";
    for (size_t i = 0; i < sweep.size(); ++i)
    {
        out << sweep[i] << " " << value(data[i]) << "\n";
    }
}

void writeResponse(std::ofstream& out, const std::vector<float>& sweep, const std::vector<Response>& data)
{
    out << "@New plot: title=\"CrossoverLR4: band magnitudes (crossover=1kHz)\" logx=true hzticks=true ystep=24\n";
    writeSeries(out, "lowOut", sweep, data, [](const Response& r) { return toDb(std::abs(r.low)); });
    writeSeries(out, "highOut", sweep, data, [](const Response& r) { return toDb(std::abs(r.high)); });
    writeSeries(out, "lowOut + highOut", sweep, data, [](const Response& r) { return toDb(std::abs(r.sum)); });
}

void writeSumPhase(std::ofstream& out, const std::vector<float>& sweep, const std::vector<Response>& data)
{
    out << "@New plot: title=\"CrossoverLR4: lowOut + highOut magnitude stays flat (crossover=1kHz)\" logx=true hzticks=true\n";
    writeSeries(out, "lowOut + highOut", sweep, data, [](const Response& r) { return toDb(std::abs(r.sum)); });

    out << "@New plot: title=\"CrossoverLR4: lowOut + highOut phase vs. AllPass biquad (Q=1/sqrt2)\" logx=true hzticks=true ystep=45\n";
    writeSeries(out, "lowOut + highOut", sweep, data, [](const Response& r) { return lagDegrees(r.sum); });
    writeSeries(out, "AllPass biquad", sweep, data, [](const Response& r) { return lagDegrees(r.allpass); });
    out << "#-180 degree reference\n" << sweep.front() << " -180\n" << sweep.back() << " -180\n";
}

void writeError(std::ofstream& out, const std::vector<float>& sweep, const std::vector<Response>& data)
{
    out << "@New plot: title=\"CrossoverLR4: |(lowOut + highOut) - AllPass biquad| (crossover=1kHz)\" logx=true hzticks=true\n";
    writeSeries(out, "deviation from allpass", sweep, data,
                [](const Response& r) { return toDb(std::abs(r.sum - r.allpass)); });
}
}

int main(int argc, char* argv[])
{
    const std::string responsePath = argc > 1 ? argv[1] : "cl4_response.txt";
    const std::string sumPath = argc > 2 ? argv[2] : "cl4_sum_allpass.txt";
    const std::string errorPath = argc > 3 ? argv[3] : "cl4_error.txt";

    std::ofstream responseOut(responsePath);
    std::ofstream sumOut(sumPath);
    std::ofstream errorOut(errorPath);
    if (!responseOut || !sumOut || !errorOut)
    {
        std::cerr << "CrossoverLR4Explore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    const auto sweep = logSweepHz(20.f, 20000.f, 240);
    std::vector<Response> data;
    data.reserve(sweep.size());
    for (const float hz : sweep)
    {
        data.push_back(measure(hz));
    }

    writeResponse(responseOut, sweep, data);
    writeSumPhase(sumOut, sweep, data);
    writeError(errorOut, sweep, data);

    std::cout << "CrossoverLR4Explore: wrote " << responsePath << ", " << sumPath << ", and " << errorPath << std::endl;
}
