// Checks the segment shapes of AbacDsp::Envelope: ExponentialApproach against 1 - exp(-k x) and the best fit of the
// polynomial CurvedValue, and a pitch envelope (MIDI notes) through Envelope<3>. Writes PyConPlot.py text files into
// the folder named by EXPLORE_OUT_DIR (default: the current folder); see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "Generators/AdsEnvelope.h"

namespace
{
using AbacDsp::CurvedValue;
using AbacDsp::Envelope;
using AbacDsp::ExponentialApproach;
using AbacDsp::SegmentShape;

constexpr float kSampleRate{48000.0f};
constexpr size_t kShapePoints{200};
constexpr std::array<float, 5> kShapeTimeConstants{-3.0f, 1.0f, 3.0f, 5.0f, 7.0f};
constexpr std::array<float, 6> kFitTimeConstants{1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f};

constexpr float kStartNote{48.0f};
constexpr float kPeakNote{84.0f};
constexpr float kSustainNote{52.0f};
constexpr float kAttackMs{5.0f};
constexpr float kDecayMs{400.0f};
constexpr float kDecayTimeConstants{5.0f};
constexpr float kPitchLengthMs{500.0f};

[[nodiscard]] double exponentialTheory(const double x, const double k)
{
    return (1.0 - std::exp(-k * x)) / (1.0 - std::exp(-k));
}

struct Fit
{
    float factor{0.0f};
    double maxError{1e9};
};

[[nodiscard]] Fit bestPolynomialFactor(const double k)
{
    Fit best;
    for (int i = -300; i <= 300; ++i)
    {
        const float factor = static_cast<float>(i) * 0.01f;
        CurvedValue curve;
        curve.setFactor(factor);
        double maxError = 0.0;
        for (size_t p = 0; p <= kShapePoints; ++p)
        {
            const double x = static_cast<double>(p) / static_cast<double>(kShapePoints);
            maxError = std::max(
                maxError, std::abs(static_cast<double>(curve.get(static_cast<float>(x))) - exponentialTheory(x, k)));
        }
        if (maxError < best.maxError)
        {
            best = {factor, maxError};
        }
    }
    return best;
}

void writeShapes(std::ofstream& out)
{
    out << "@New plot: title=\"segment shapes: straight, exponential (k time constants) and the best polynomial for "
           "k = 5\"\n";
    out << "#straight\n";
    for (size_t p = 0; p <= kShapePoints; ++p)
    {
        const float x = static_cast<float>(p) / static_cast<float>(kShapePoints);
        out << x << " " << x << "\n";
    }
    for (const float k : kShapeTimeConstants)
    {
        ExponentialApproach curve;
        curve.setTimeConstants(k);
        out << "#exponential k=" << k << "\n";
        for (size_t p = 0; p <= kShapePoints; ++p)
        {
            const float x = static_cast<float>(p) / static_cast<float>(kShapePoints);
            out << x << " " << curve.get(x) << "\n";
        }
    }
    const Fit fit = bestPolynomialFactor(5.0);
    CurvedValue polynomial;
    polynomial.setFactor(fit.factor);
    out << "#CurvedValue factor=" << fit.factor << " (best fit of k=5)\n";
    for (size_t p = 0; p <= kShapePoints; ++p)
    {
        const float x = static_cast<float>(p) / static_cast<float>(kShapePoints);
        out << x << " " << polynomial.get(x) << "\n";
    }
}

void printFits()
{
    std::cout << "best CurvedValue factor per k, error against the exponential as a fraction of the segment\n";
    for (const float k : kFitTimeConstants)
    {
        const Fit fit = bestPolynomialFactor(static_cast<double>(k));
        CurvedValue curve;
        curve.setFactor(fit.factor);
        float overshoot = 0.0f;
        for (size_t p = 0; p <= kShapePoints; ++p)
        {
            overshoot = std::max(overshoot, curve.get(static_cast<float>(p) / static_cast<float>(kShapePoints)));
        }
        std::cout << "k " << k << ": factor " << fit.factor << ", max error " << fit.maxError << ", peak " << overshoot
                  << "\n";
    }
}

[[nodiscard]] Envelope<3> makePitchEnvelope(const SegmentShape shape)
{
    Envelope<3> envelope{kSampleRate};
    envelope.setSegment(0, kAttackMs, kPeakNote, 0.0f, shape);
    envelope.setSegment(1, kDecayMs, kSustainNote, shape == SegmentShape::Exponential ? kDecayTimeConstants : 0.0f,
                        shape);
    envelope.setSegmentHoldPreviousValue(2, 0.0f);
    envelope.markSustainAt(2);
    envelope.triggerFrom(kStartNote);
    return envelope;
}

[[nodiscard]] double pitchTheory(const double ms)
{
    const double attack = static_cast<double>(kAttackMs);
    if (ms <= attack)
    {
        return static_cast<double>(kStartNote) + (ms / attack) * static_cast<double>(kPeakNote - kStartNote);
    }
    const double x = std::min((ms - attack) / static_cast<double>(kDecayMs), 1.0);
    return static_cast<double>(kPeakNote) + exponentialTheory(x, static_cast<double>(kDecayTimeConstants)) *
                                                static_cast<double>(kSustainNote - kPeakNote);
}

void writePitch(std::ofstream& out)
{
    out << "@New plot: title=\"cutoff note: start 48, peak 84 in 5 ms, then 400 ms to 52\"\n";
    const auto samples = static_cast<size_t>(kPitchLengthMs * 0.001f * kSampleRate);
    const size_t stride = 48;
    double maxDeviation = 0.0;
    for (const SegmentShape shape : {SegmentShape::Polynomial, SegmentShape::Exponential})
    {
        Envelope<3> envelope = makePitchEnvelope(shape);
        out << (shape == SegmentShape::Exponential ? "#exponential decay (k=5)\n" : "#straight decay\n");
        for (size_t i = 1; i <= samples; ++i)
        {
            const float value = envelope.step();
            const double ms = static_cast<double>(i) * 1000.0 / static_cast<double>(kSampleRate);
            if (shape == SegmentShape::Exponential)
            {
                maxDeviation = std::max(maxDeviation, std::abs(static_cast<double>(value) - pitchTheory(ms)));
            }
            if (i % stride == 0)
            {
                out << ms << " " << value << "\n";
            }
        }
    }
    std::cout << "pitch envelope: max deviation of the exponential decay from the closed form: " << maxDeviation
              << " notes\n";
}
}

int main()
{
    const char* folder = std::getenv("EXPLORE_OUT_DIR");
    const std::string directory = folder != nullptr ? folder : ".";
    std::ofstream shapesOut(directory + "/envelope_shapes.txt");
    std::ofstream pitchOut(directory + "/envelope_pitch.txt");
    if (!shapesOut || !pitchOut)
    {
        std::cerr << "EnvelopeCurveExplore: ERROR - failed to open output files in " << directory << "\n";
        return 1;
    }
    writeShapes(shapesOut);
    writePitch(pitchOut);
    printFits();
    std::cout << "wrote envelope_shapes.txt, envelope_pitch.txt to " << directory << "\n";
    return 0;
}
