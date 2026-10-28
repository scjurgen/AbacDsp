// Checks StampedYinPitchDetector and PitchNoteTracker over 40 Hz to 2 kHz: pitch error, time to the first note, and
// readings from white noise, with YinPitchDetector for comparison. Writes PyConPlot.py text files into the folder
// named by EXPLORE_OUT_DIR (default: the current folder); see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>

#include "Analysis/PitchNoteTracker.h"
#include "Analysis/StampedYinPitchDetector.h"
#include "Analysis/YinPitchDetector.h"
#include "Numbers/Random.h"

namespace
{
using namespace AbacDsp;

constexpr float kRate{48000.0f};
constexpr float kPi{3.14159265358979323846f};
constexpr float kLowestHz{40.0f};
constexpr float kHighestHz{2000.0f};
constexpr float kHopHz{50.0f};
constexpr int kPoints{48};

[[nodiscard]] float logSpaced(const float low, const float high, const int i, const int count)
{
    return low * std::pow(high / low, static_cast<float>(i) / static_cast<float>(count - 1));
}

[[nodiscard]] float sineAt(const float hz, const float amplitude, const size_t i)
{
    return amplitude * std::sin(2.0f * kPi * hz * static_cast<float>(i) / kRate);
}

[[nodiscard]] double centsOf(const float measured, const float expected)
{
    return measured > 0.0f ? 1200.0 * std::log2(static_cast<double>(measured) / static_cast<double>(expected)) : 0.0;
}

void writeRange(std::ofstream& out)
{
    out << "@New plot: title=\"pitch error of a steady sine, amplitude 0.5, range 40 Hz to 2 kHz\"\n";
    for (const size_t decimation : {1u, 4u})
    {
        out << "#" << (decimation == 1 ? "full rate" : "decimated by 4") << "\n";
        for (int i = 0; i < kPoints; ++i)
        {
            const float hz = logSpaced(45.0f, 1900.0f, i, kPoints);
            StampedYinPitchDetector detector(kRate, kLowestHz, kHighestHz, kHopHz, {decimation});
            float pitch = 0.0f;
            for (size_t n = 0; n < 48000; ++n)
            {
                pitch = detector.step(sineAt(hz, 0.5f, n));
            }
            out << hz << " " << centsOf(pitch, hz) << "\n";
        }
    }
}

[[nodiscard]] std::optional<double> firstNoteMs(const float hz, const size_t decimation)
{
    StampedYinPitchDetector detector(kRate, kLowestHz, kHighestHz, kHopHz, {decimation});
    PitchNoteTracker tracker;
    for (size_t n = 0; n < 48000; ++n)
    {
        std::ignore = detector.step(sineAt(hz, 0.5f, n));
        if (!detector.hasNewPitch())
        {
            continue;
        }
        const auto event = tracker.update({detector.getCurrentPitch(), detector.getLastConfidence(),
                                           detector.getLastLevel(), detector.getLastWindowEnd()});
        if (event && !event->released)
        {
            return static_cast<double>(event->stamp) / static_cast<double>(kRate) * 1000.0;
        }
    }
    return std::nullopt;
}

void writeLatency(std::ofstream& out)
{
    out << "@New plot: title=\"time of the first note after the tone starts\"\n";
    for (const size_t decimation : {1u, 4u})
    {
        out << "#" << (decimation == 1 ? "full rate" : "decimated by 4") << "\n";
        for (int i = 0; i < kPoints; ++i)
        {
            const float hz = logSpaced(41.0f, 1900.0f, i, kPoints);
            if (const auto ms = firstNoteMs(hz, decimation))
            {
                out << hz << " " << *ms << "\n";
            }
        }
    }
}

struct NoiseReading
{
    bool isNew;
    float pitch;
    float confidence;
};

[[nodiscard]] double pitchedFraction(const float amplitude, const float minConfidence, auto&& makeStep)
{
    Random random(5);
    auto step = makeStep();
    int readings = 0;
    int pitched = 0;
    for (size_t n = 0; n < 96000; ++n)
    {
        const NoiseReading reading = step(amplitude * (2.0f * random.uniform() - 1.0f));
        if (reading.isNew && n > 4800)
        {
            ++readings;
            pitched += reading.pitch > 0.0f && reading.confidence >= minConfidence ? 1 : 0;
        }
    }
    return 100.0 * pitched / std::max(readings, 1);
}

void writeNoisePlot(std::ofstream& out, const char* title, const float minConfidence)
{
    out << "@New plot: title=\"" << title << "\"\n";
    const auto series = [&](const char* name, auto&& makeStep)
    {
        out << "#" << name << "\n";
        for (int i = 0; i < 25; ++i)
        {
            const float amplitude = logSpaced(1e-4f, 1.0f, i, 25);
            out << amplitude << " " << pitchedFraction(amplitude, minConfidence, makeStep) << "\n";
        }
    };
    series("YinPitchDetector, 80 Hz to 1 kHz",
           []
           {
               return [detector = YinPitchDetector(kRate, 80.0f, 1000.0f, kHopHz)](const float x) mutable
               {
                   const float pitch = detector.step(x);
                   return NoiseReading{detector.hasNewPitch(), pitch, detector.getLastConfidence()};
               };
           });
    series("StampedYinPitchDetector, 40 Hz to 2 kHz",
           []
           {
               return [detector = StampedYinPitchDetector(kRate, kLowestHz, kHighestHz, kHopHz)](const float x) mutable
               {
                   const float pitch = detector.step(x);
                   return NoiseReading{detector.hasNewPitch(), pitch, detector.getLastConfidence()};
               };
           });
    series("StampedYinPitchDetector, decimated by 4",
           []
           {
               return [detector =
                           StampedYinPitchDetector(kRate, kLowestHz, kHighestHz, kHopHz, {4})](const float x) mutable
               {
                   const float pitch = detector.step(x);
                   return NoiseReading{detector.hasNewPitch(), pitch, detector.getLastConfidence()};
               };
           });
}

void writeNoise(std::ofstream& out)
{
    writeNoisePlot(out, "white noise: readings that report a pitch (percent)", 0.0f);
    writeNoisePlot(out, "white noise: readings with confidence of at least 0.85, the tracker's gate (percent)", 0.85f);
}
}

int main()
{
    const char* folder = std::getenv("EXPLORE_OUT_DIR");
    const std::string directory = folder != nullptr ? folder : ".";
    std::ofstream rangeOut(directory + "/yin_range.txt");
    std::ofstream latencyOut(directory + "/yin_latency.txt");
    std::ofstream noiseOut(directory + "/yin_noise.txt");
    if (!rangeOut || !latencyOut || !noiseOut)
    {
        std::cerr << "StampedYinExplore: ERROR - failed to open output files in " << directory << "\n";
        return 1;
    }
    writeRange(rangeOut);
    writeLatency(latencyOut);
    writeNoise(noiseOut);
    std::cout << "wrote yin_range.txt, yin_latency.txt, yin_noise.txt to " << directory << "\n";
    return 0;
}
