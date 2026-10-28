// Checks AbacDsp::OnePoleSmoother against theory: its step response against 1 - exp(-t / tau), and the distance
// to the target against OnePoleFilter<LowPass>, which stalls. Writes PyConPlot.py text files into the folder named
// by EXPLORE_OUT_DIR (default: the current folder); see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "Filters/OnePoleFilter.h"
#include "Parameters/OnePoleSmoother.h"

namespace
{
using AbacDsp::OnePoleFilter;
using AbacDsp::OnePoleFilterCharacteristic;
using AbacDsp::OnePoleSmoother;

constexpr float kSampleRate{48000.0f};
constexpr std::array<float, 3> kTimeConstantsMs{5.0f, 20.0f, 100.0f};
constexpr float kStepLengthTimeConstants{6.0f};
constexpr size_t kStepPoints{240};

constexpr float kStallTimeConstantMs{20.0f};
constexpr size_t kStallSamples{48000};
constexpr size_t kStallStride{100};
constexpr double kDistanceFloor{1e-9};

[[nodiscard]] OnePoleSmoother makeSmoother(const float timeMs)
{
    OnePoleSmoother smoother;
    smoother.setSampleRate(kSampleRate);
    smoother.setTimeMs(timeMs);
    smoother.snapTo(0.0f);
    smoother.setTarget(1.0f);
    return smoother;
}

void writeStepResponse(std::ofstream& out)
{
    out << "@New plot: title=\"step response against time in time constants\"\n";
    for (const float timeMs : kTimeConstantsMs)
    {
        OnePoleSmoother smoother = makeSmoother(timeMs);
        const auto samples = static_cast<size_t>(kStepLengthTimeConstants * timeMs * 0.001f * kSampleRate);
        const size_t stride = std::max<size_t>(1, samples / kStepPoints);
        out << "#tau=" << timeMs << " ms\n";
        for (size_t i = 1; i <= samples; ++i)
        {
            const float y = smoother.next();
            if (i % stride == 0)
            {
                out << (static_cast<float>(i) / kSampleRate / (timeMs * 0.001f)) << " " << y << "\n";
            }
        }
    }
    out << "#theoretical 1-exp(-t/tau)\n";
    for (size_t i = 0; i <= kStepPoints; ++i)
    {
        const float t = kStepLengthTimeConstants * static_cast<float>(i) / static_cast<float>(kStepPoints);
        out << t << " " << 1.0f - std::exp(-t) << "\n";
    }
}

void writeDistance(std::ofstream& out, const char* name, const auto& distanceAt)
{
    out << "#" << name << "\n";
    for (size_t i = 0; i < kStallSamples; i += kStallStride)
    {
        out << (static_cast<float>(i + 1) / kSampleRate) << " " << std::max(distanceAt(i), kDistanceFloor) << "\n";
    }
}

void writeStall(std::ofstream& out)
{
    OnePoleSmoother smoother = makeSmoother(kStallTimeConstantMs);
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> filter(kSampleRate);
    filter.setFeedback(smoother.getCoefficient());
    std::array<double, kStallSamples> smootherDistance{};
    std::array<double, kStallSamples> filterDistance{};
    for (size_t i = 0; i < kStallSamples; ++i)
    {
        smootherDistance[i] = 1.0 - static_cast<double>(smoother.next());
        filterDistance[i] = 1.0 - static_cast<double>(filter.step(1.0f));
    }
    out << "@New plot: title=\"distance to the target, tau = " << kStallTimeConstantMs << " ms (floor "
        << kDistanceFloor << ")\" logy=true miny=1e-9 maxy=1\n";
    writeDistance(out, "OnePoleSmoother", [&](const size_t i) { return smootherDistance[i]; });
    writeDistance(out, "OnePoleFilter<LowPass>", [&](const size_t i) { return filterDistance[i]; });
    writeDistance(out, "ideal exp(-t/tau)",
                  [&](const size_t i)
                  {
                      return std::exp(-static_cast<double>(i + 1) / static_cast<double>(kSampleRate) /
                                      (static_cast<double>(kStallTimeConstantMs) * 0.001));
                  });
}
}

int main()
{
    const char* folder = std::getenv("EXPLORE_OUT_DIR");
    const std::string directory = folder != nullptr ? folder : ".";
    std::ofstream stepOut(directory + "/smoother_step.txt");
    std::ofstream stallOut(directory + "/smoother_stall.txt");
    if (!stepOut || !stallOut)
    {
        std::cerr << "OnePoleSmootherExplore: ERROR - failed to open output files in " << directory << "\n";
        return 1;
    }
    writeStepResponse(stepOut);
    writeStall(stallOut);
    std::cout << "wrote smoother_step.txt, smoother_stall.txt to " << directory << "\n";
    return 0;
}
