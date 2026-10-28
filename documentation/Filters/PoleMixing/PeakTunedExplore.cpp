// Checks PeakTunedPoleMixingFilter against theory and against FixedFourStageFilter, and shows CutoffTrajectory.
// Writes PyConPlot.py text files into the folder named by EXPLORE_OUT_DIR (default: the current folder) and prints
// the figures quoted in README.md; see there.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "Filters/PeakTunedPoleMixing.h"
#include "Filters/PoleMixingFilter.h"
#include "Parameters/CutoffTrajectory.h"

namespace
{
using namespace AbacDsp;

constexpr float kSampleRate{48000.0f};
constexpr int kPoints{40};
constexpr float kLowestHz{100.0f};
constexpr float kHighestHz{20000.0f};
constexpr std::array<double, 2> kFractions{0.85, 0.95};

[[nodiscard]] float frequencyAt(const int i)
{
    return kLowestHz * std::pow(kHighestHz / kLowestHz, static_cast<float>(i) / static_cast<float>(kPoints - 1));
}

[[nodiscard]] const PoleMixingPreset& lowPass4()
{
    return kPoleMixingPresets[findPoleMixingPreset("4 pole lp")];
}

[[nodiscard]] double responsePeakOverRequest(const float requestedHz, const double fraction)
{
    const PoleMixingTuning tuning = poleMixingTuningForPeak(requestedHz, kSampleRate);
    const PoleMixingPreset& preset = lowPass4();
    const double resonance = fraction * static_cast<double>(tuning.oscillationThreshold) / preset.resonanceFeedback;
    double best = 0.0;
    double bestHz = 0.0;
    for (double hz = 0.7 * requestedHz; hz < 1.05 * requestedHz && hz < 0.499 * kSampleRate; hz *= 1.0005)
    {
        const double magnitude = std::abs(poleMixingResponse(preset, tuning.pole, resonance, hz, kSampleRate));
        if (magnitude > best)
        {
            best = magnitude;
            bestHz = hz;
        }
    }
    return bestHz / static_cast<double>(requestedHz);
}

void writeTuning(std::ofstream& out)
{
    out << "@New plot: title=\"response peak over requested frequency, 4 pole lp, 48 kHz\"\n";
    for (const double fraction : kFractions)
    {
        out << "#" << fraction * 100.0 << " % of the oscillation threshold\n";
        for (int i = 0; i < kPoints; ++i)
        {
            const float hz = frequencyAt(i);
            out << hz << " " << responsePeakOverRequest(hz, fraction) << "\n";
        }
    }
}

// Frequency of a sustained oscillation from its rising zero crossings in the last second of a three second run.
template <typename Process>
[[nodiscard]] double oscillationHz(Process&& process)
{
    constexpr int length = 3 * 48000;
    constexpr int measureFrom = 2 * 48000;
    int first = -1;
    int last = -1;
    int crossings = 0;
    float previous = 0.0f;
    for (int i = 0; i < length; ++i)
    {
        const float y = process(i == 0 ? 1e-6f : 0.0f);
        if (i > measureFrom && previous < 0.0f && y >= 0.0f)
        {
            crossings += first >= 0 ? 1 : 0;
            first = first < 0 ? i : first;
            last = i;
        }
        previous = y;
    }
    return crossings > 2 ? static_cast<double>(kSampleRate) * crossings / static_cast<double>(last - first) : 0.0;
}

[[nodiscard]] double tunedOscillationHz(const float requestedHz)
{
    const PoleMixingTuning tuning = poleMixingTuningForPeak(requestedHz, kSampleRate);
    PeakTunedPoleMixingFilter filter;
    filter.setPreset(findPoleMixingPreset("4 pole lp"));
    filter.setPole(tuning.pole);
    filter.setResonance(1.01f * tuning.oscillationThreshold / lowPass4().resonanceFeedback);
    return oscillationHz([&filter](const float x) { return filter.process(x); });
}

// FixedFourStageFilter feeds atan(in - v3 * reso) back, so its loop gain is reso = 4 * the value given to
// setResonance(); the edge of oscillation is that of the pole its warped cutoff gives.
[[nodiscard]] double fixedOscillationHz(const float requestedHz)
{
    FixedFourStageFilter<0, 0, 0, 0, 1> filter(kSampleRate);
    std::ignore = filter.setCutoff(2.0f * requestedHz);
    const float stageCutoff = filter.setCutoff(requestedHz);
    const double pole = std::exp(-2.0 * 3.14159265358979323846 * static_cast<double>(stageCutoff) / kSampleRate);
    filter.setResonance(static_cast<float>(1.01 * poleMixingOscillationThreshold(pole) / 4.0));
    return oscillationHz([&filter](const float x) { return filter.step(x); });
}

void writePitch(std::ofstream& out)
{
    out << "@New plot: title=\"self-oscillation frequency over requested frequency, 1.01 times the edge, 48 kHz\"\n";
    out << "#requested\n" << kLowestHz << " 1\n" << kHighestHz << " 1\n";
    out << "#PeakTunedPoleMixingFilter\n";
    for (int i = 0; i < kPoints; ++i)
    {
        const float hz = frequencyAt(i);
        out << hz << " " << tunedOscillationHz(hz) / static_cast<double>(hz) << "\n";
    }
    out << "#FixedFourStageFilter\n";
    for (int i = 0; i < kPoints; ++i)
    {
        const float hz = frequencyAt(i);
        out << hz << " " << fixedOscillationHz(hz) / static_cast<double>(hz) << "\n";
    }
}

void writeWaveform(std::ofstream& out)
{
    constexpr float requestedHz = 1000.0f;
    constexpr int settle = 2 * 48000;
    constexpr int shown = 120;
    const PoleMixingTuning tuning = poleMixingTuningForPeak(requestedHz, kSampleRate);
    PeakTunedPoleMixingFilter tuned;
    tuned.setPreset(findPoleMixingPreset("4 pole lp"));
    tuned.setPole(tuning.pole);
    tuned.setResonance(3.0f * tuning.oscillationThreshold / lowPass4().resonanceFeedback);

    FixedFourStageFilter<0, 0, 0, 0, 1> fixed(kSampleRate);
    std::ignore = fixed.setCutoff(2.0f * requestedHz);
    const float stageCutoff = fixed.setCutoff(requestedHz);
    const double pole = std::exp(-2.0 * 3.14159265358979323846 * static_cast<double>(stageCutoff) / kSampleRate);
    fixed.setResonance(static_cast<float>(3.0 * poleMixingOscillationThreshold(pole) / 4.0));

    std::vector<float> tunedOut(shown);
    std::vector<float> fixedOut(shown);
    for (int i = 0; i < settle + shown; ++i)
    {
        const float x = i == 0 ? 1e-6f : 0.0f;
        const float a = tuned.process(x);
        const float b = fixed.step(x);
        if (i >= settle)
        {
            tunedOut[static_cast<size_t>(i - settle)] = a;
            fixedOut[static_cast<size_t>(i - settle)] = b;
        }
    }
    const auto peakOf = [](const std::vector<float>& v) { return *std::ranges::max_element(v); };
    std::cout << "waveform at 1 kHz, 3 times the edge: peak " << peakOf(tunedOut) << " (PeakTuned), "
              << peakOf(fixedOut) << " (FixedFourStageFilter)\n";
    out << "@New plot: title=\"self-oscillation at 1 kHz, 3 times the edge\"\n#PeakTunedPoleMixingFilter\n";
    for (int i = 0; i < shown; ++i)
    {
        out << (static_cast<float>(i) / kSampleRate * 1000.0f) << " " << tunedOut[static_cast<size_t>(i)] << "\n";
    }
    out << "#FixedFourStageFilter\n";
    for (int i = 0; i < shown; ++i)
    {
        out << (static_cast<float>(i) / kSampleRate * 1000.0f) << " " << fixedOut[static_cast<size_t>(i)] << "\n";
    }
}

void writeGlide(std::ofstream& noteOut, std::ofstream& hzOut)
{
    constexpr int length = 12000;
    CutoffTrajectory trajectory;
    trajectory.setValue(57.0f);
    trajectory.retarget(81.0f, 4800);
    noteOut << "@New plot: title=\"glide in semitones, a retarget at 2400 samples\"\n#note\n";
    hzOut << "@New plot: title=\"the same glide as frequency\"\n#frequency (Hz)\n";
    for (int i = 0; i < length; ++i)
    {
        if (i == 2400)
        {
            trajectory.retarget(45.0f, 4800);
        }
        const float note = trajectory.next();
        const float ms = static_cast<float>(i + 1) / kSampleRate * 1000.0f;
        noteOut << ms << " " << note << "\n";
        hzOut << ms << " " << 440.0f * std::exp2((note - 69.0f) / 12.0f) << "\n";
    }
}
}

int main()
{
    const char* folder = std::getenv("EXPLORE_OUT_DIR");
    const std::string directory = folder != nullptr ? folder : ".";
    std::ofstream tuningOut(directory + "/pt_tuning.txt");
    std::ofstream pitchOut(directory + "/pt_pitch.txt");
    std::ofstream waveformOut(directory + "/pt_waveform.txt");
    std::ofstream glideNoteOut(directory + "/pt_glide_note.txt");
    std::ofstream glideHzOut(directory + "/pt_glide_hz.txt");
    if (!tuningOut || !pitchOut || !waveformOut || !glideNoteOut || !glideHzOut)
    {
        std::cerr << "PeakTunedExplore: ERROR - failed to open output files in " << directory << "\n";
        return 1;
    }
    writeTuning(tuningOut);
    writePitch(pitchOut);
    writeWaveform(waveformOut);
    writeGlide(glideNoteOut, glideHzOut);
    std::cout << "wrote pt_tuning, pt_pitch, pt_waveform, pt_glide_note and pt_glide_hz (.txt) to " << directory
              << "\n";
    return 0;
}
