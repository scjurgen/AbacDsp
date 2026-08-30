// Verification plots for src/includes/Filters/Biquad.h: family magnitude and phase response,
// Chebyshev Type1 vs Type2, empirical stability, and PeakBiquad boost/cut symmetry. See README.md.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Filters/Biquad.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr float kButterworthQ = 0.7071f;

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

// ---- Biquad<type> family response ----

template <AbacDsp::BiquadFilterType Type>
void writeFamilyResponse(std::ofstream& out, const char* name, const float freqHz, const float q, const float gainDb,
                         const std::vector<float>& sweep)
{
    AbacDsp::Biquad<Type> filter;
    filter.computeCoefficients(kSampleRate, freqHz, q, gainDb);
    out << "#" << name << "\n";
    for (const float hz : sweep)
    {
        out << hz << " " << filter.magnitudeInDb(hz / kSampleRate) << "\n";
    }
}

void writeFamilyResponses(std::ofstream& out)
{
    out << "@New plot: title=\"Biquad<type> family: magnitude response (freq=1kHz, Q=0.7071, "
           "gain=+12dB where used)\" logx=true\n";
    const auto sweep = logSweepHz(20.f, 20000.f, 500);
    constexpr float freqHz = 1000.f;
    constexpr float gainDb = 12.f;

    writeFamilyResponse<AbacDsp::BiquadFilterType::LowPass>(out, "LowPass", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::HighPass>(out, "HighPass", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::BandPass>(out, "BandPass", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::Notch>(out, "Notch", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::Peak>(out, "Peak", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::LoShelf>(out, "LoShelf", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::HiShelf>(out, "HiShelf", freqHz, kButterworthQ, gainDb, sweep);
    writeFamilyResponse<AbacDsp::BiquadFilterType::AllPass>(out, "AllPass", freqHz, kButterworthQ, gainDb, sweep);
}

// ---- Biquad<type> family phase response ----

// No analytical phase in this class (only magnitude), so phase is measured the same way a
// lock-in amplifier would: correlate settled output against sin/cos at the exact drive
// frequency to get its I/Q pair directly, with no FFT bin-width or windowing error at all.
constexpr size_t kPhaseSettleSamples = 4000;
constexpr size_t kPhaseMeasureSamples = 4000;

template <AbacDsp::BiquadFilterType Type>
[[nodiscard]] float measurePhaseDeg(const float testHz, const float designHz, const float q, const float gainDb)
{
    AbacDsp::Biquad<Type> filter;
    filter.computeCoefficients(kSampleRate, designHz, q, gainDb);

    constexpr size_t total = kPhaseSettleSamples + kPhaseMeasureSamples;
    std::vector<float> in(total);
    std::vector<float> out(total);
    const float phaseInc = 2.f * std::numbers::pi_v<float> * testHz / kSampleRate;
    for (size_t n = 0; n < total; ++n)
    {
        in[n] = std::sin(phaseInc * static_cast<float>(n));
    }
    filter.processBlock(in.data(), out.data(), total);

    double iIn = 0.0, qIn = 0.0, iOut = 0.0, qOut = 0.0;
    for (size_t n = kPhaseSettleSamples; n < total; ++n)
    {
        const float p = phaseInc * static_cast<float>(n);
        const float c = std::cos(p);
        const float s = std::sin(p);
        iIn += in[n] * c;
        qIn += in[n] * s;
        iOut += out[n] * c;
        qOut += out[n] * s;
    }
    const auto phaseIn = static_cast<float>(std::atan2(qIn, iIn));
    const auto phaseOut = static_cast<float>(std::atan2(qOut, iOut));
    float relative = phaseOut - phaseIn;
    while (relative > std::numbers::pi_v<float>)
    {
        relative -= 2.f * std::numbers::pi_v<float>;
    }
    while (relative < -std::numbers::pi_v<float>)
    {
        relative += 2.f * std::numbers::pi_v<float>;
    }
    return relative * 180.f / std::numbers::pi_v<float>;
}

template <AbacDsp::BiquadFilterType Type>
void writePhaseResponse(std::ofstream& out, const char* name, const float designHz, const float q, const float gainDb,
                        const std::vector<float>& sweep)
{
    out << "#" << name << "\n";
    float unwrapOffset = 0.f;
    float previous = 0.f;
    bool first = true;
    for (const float hz : sweep)
    {
        const float raw = measurePhaseDeg<Type>(hz, designHz, q, gainDb);
        if (!first)
        {
            float diff = raw + unwrapOffset - previous;
            while (diff > 180.f)
            {
                unwrapOffset -= 360.f;
                diff -= 360.f;
            }
            while (diff < -180.f)
            {
                unwrapOffset += 360.f;
                diff += 360.f;
            }
        }
        first = false;
        previous = raw + unwrapOffset;
        out << hz << " " << previous << "\n";
    }
}

void writePhaseResponses(std::ofstream& out)
{
    out << "@New plot: title=\"Biquad<type> family: phase response (freq=1kHz, Q=0.7071, "
           "gain=+12dB where used)\" logx=true\n";
    const auto sweep = logSweepHz(20.f, 20000.f, 200);
    constexpr float freqHz = 1000.f;
    constexpr float gainDb = 12.f;

    writePhaseResponse<AbacDsp::BiquadFilterType::LowPass>(out, "LowPass", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::HighPass>(out, "HighPass", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::BandPass>(out, "BandPass", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::Notch>(out, "Notch", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::Peak>(out, "Peak", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::LoShelf>(out, "LoShelf", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::HiShelf>(out, "HiShelf", freqHz, kButterworthQ, gainDb, sweep);
    writePhaseResponse<AbacDsp::BiquadFilterType::AllPass>(out, "AllPass", freqHz, kButterworthQ, gainDb, sweep);
}

// ---- ChebyshevBiquad Type1 vs Type2 ----

void writeChebyshevComparison(std::ofstream& out)
{
    const auto sweep = logSweepHz(20.f, 20000.f, 500);

    const auto writeOne = [&](const size_t order, const float fc, const float rippleDb, const char* title)
    {
        out << "@New plot: title=\"" << title << "\" logx=true\n";
        AbacDsp::ChebyshevBiquad type1(kSampleRate);
        type1.computeType1(order, fc, rippleDb, true);
        out << "#Type1 (equiripple passband)\n";
        for (const float hz : sweep)
        {
            out << hz << " " << type1.getMagnitudeInDb(hz) << "\n";
        }

        AbacDsp::ChebyshevBiquad type2(kSampleRate);
        type2.computeType2(order, fc, rippleDb, true);
        out << "#Type2 (equiripple stopband)\n";
        for (const float hz : sweep)
        {
            out << hz << " " << type2.getMagnitudeInDb(hz) << "\n";
        }
    };

    writeOne(6, 1000.f, 1.f, "ChebyshevBiquad order=6, ripple=1dB, fc=1kHz lowpass");
    writeOne(8, 1000.f, 3.f, "ChebyshevBiquad order=8, ripple=3dB, fc=1kHz lowpass");
}

// ---- Pole-radius stability ----

// a1/a2 are protected (no coefficient getter), so stability is measured the empirical way
// instead: render an impulse response and compare its tail level to its transient peak. A
// decaying (stable) filter reads strongly negative; a growing/undamped one reads near 0dB.
constexpr size_t kStabilityRenderSamples = 8192;
constexpr size_t kStabilityWindow = 256;

template <AbacDsp::BiquadFilterType Type>
[[nodiscard]] float tailDecayDbFor(const float freqHz, const float q, const float gainDb)
{
    AbacDsp::Biquad<Type> filter;
    filter.computeCoefficients(kSampleRate, freqHz, q, gainDb);
    std::vector<float> impulse(kStabilityRenderSamples, 0.f);
    impulse[0] = 1.f;
    std::vector<float> rendered(kStabilityRenderSamples);
    filter.processBlock(impulse.data(), rendered.data(), kStabilityRenderSamples);

    const auto peakOf = [](const float* data, const size_t n)
    {
        float peak = 0.f;
        for (size_t i = 0; i < n; ++i)
        {
            peak = std::max(peak, std::abs(data[i]));
        }
        return peak;
    };
    const float overallPeak = peakOf(rendered.data(), rendered.size());
    const float tailPeak = peakOf(rendered.data() + rendered.size() - kStabilityWindow, kStabilityWindow);
    return 20.f * std::log10(std::max(tailPeak, 1e-12f) / std::max(overallPeak, 1e-12f));
}

void writeStabilityLimit(std::ofstream& out, const float xLow, const float xHigh)
{
    out << "#not decaying (0dB)\n" << xLow << " 0\n" << xHigh << " 0\n";
}

void writeStability(std::ofstream& out)
{
    out << "@New plot: title=\"Tail decay vs. frequency (Q=0.7071, gain=+12dB where used)\" "
           "logx=true\n";
    const auto sweep = logSweepHz(20.f, 20000.f, 500);
    const auto writeVsFreq = [&](const auto typeTag, const char* name)
    {
        out << "#" << name << "\n";
        for (const float hz : sweep)
        {
            out << hz << " " << tailDecayDbFor<decltype(typeTag)::value>(hz, kButterworthQ, 12.f) << "\n";
        }
    };
    writeVsFreq(std::integral_constant<AbacDsp::BiquadFilterType, AbacDsp::BiquadFilterType::LowPass>{}, "LowPass");
    writeVsFreq(std::integral_constant<AbacDsp::BiquadFilterType, AbacDsp::BiquadFilterType::Peak>{}, "Peak (+12dB)");
    writeVsFreq(std::integral_constant<AbacDsp::BiquadFilterType, AbacDsp::BiquadFilterType::BandPass>{}, "BandPass");
    writeStabilityLimit(out, sweep.front(), sweep.back());

    out << "@New plot: title=\"Tail decay vs. Q at fc=1kHz, stressed up to Q=200\"\n";
    std::vector<float> qSweep(400);
    for (size_t i = 0; i < qSweep.size(); ++i)
    {
        qSweep[i] = 0.1f + 200.f * static_cast<float>(i) / static_cast<float>(qSweep.size() - 1);
    }
    const auto writeVsQ = [&](const auto typeTag, const char* name, const float gainDb)
    {
        out << "#" << name << "\n";
        for (const float q : qSweep)
        {
            out << q << " " << tailDecayDbFor<decltype(typeTag)::value>(1000.f, q, gainDb) << "\n";
        }
    };
    writeVsQ(std::integral_constant<AbacDsp::BiquadFilterType, AbacDsp::BiquadFilterType::LowPass>{}, "LowPass", 0.f);
    writeVsQ(std::integral_constant<AbacDsp::BiquadFilterType, AbacDsp::BiquadFilterType::Peak>{}, "Peak (+12dB)",
             12.f);
    writeVsQ(std::integral_constant<AbacDsp::BiquadFilterType, AbacDsp::BiquadFilterType::BandPass>{}, "BandPass", 0.f);
    writeStabilityLimit(out, qSweep.front(), qSweep.back());
}

// ---- PeakBiquad boost/cut mirror symmetry ----

constexpr size_t kFftSize = 16384;

[[nodiscard]] std::vector<float> measurePeakBiquadSpectrumDb(const float freqHz, const float gainDb, const float q)
{
    AbacDsp::PeakBiquad filter(kSampleRate);
    filter.computeCoefficients(freqHz, gainDb, q);
    std::vector<float> impulse(kFftSize, 0.f);
    impulse[0] = 1.f;
    std::vector<float> rendered(kFftSize);
    filter.processBlock(impulse.data(), rendered.data(), kFftSize);

    // Rectangular, not Hann: this impulse response is front-loaded, and a Hann taper
    // would zero out sample 0 - almost all of its actual content.
    std::vector<float> magnitude;
    AbacDsp::BasicFFT::realDataToMagnitude<float, AbacDsp::FftRectangularWindow>(rendered, magnitude);
    std::vector<float> db(kFftSize / 2);
    for (size_t bin = 0; bin < db.size(); ++bin)
    {
        db[bin] = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
    }
    return db;
}

void writePeakSymmetry(std::ofstream& out)
{
    out << "@New plot: title=\"PeakBiquad: +12dB boost vs. mirrored -12dB cut (freq=1kHz, "
           "Q=1)\" logx=true\n";
    constexpr float freqHz = 1000.f;
    constexpr float q = 1.f;
    const auto boost = measurePeakBiquadSpectrumDb(freqHz, 12.f, q);
    const auto cut = measurePeakBiquadSpectrumDb(freqHz, -12.f, q);

    const auto writeSeries = [&](const char* name, const std::vector<float>& db, const float sign)
    {
        out << "#" << name << "\n";
        for (size_t bin = 1; bin < db.size(); ++bin)
        {
            const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
            out << hz << " " << (sign * db[bin]) << "\n";
        }
    };
    writeSeries("+12dB boost", boost, 1.f);
    writeSeries("-1 x (-12dB cut)", cut, -1.f);
}
}

int main(int argc, char* argv[])
{
    const std::string responsePath = argc > 1 ? argv[1] : "bq_response.txt";
    const std::string phasePath = argc > 2 ? argv[2] : "bq_phase.txt";
    const std::string chebyshevPath = argc > 3 ? argv[3] : "bq_chebyshev.txt";
    const std::string stabilityPath = argc > 4 ? argv[4] : "bq_stability.txt";
    const std::string peakSymmetryPath = argc > 5 ? argv[5] : "bq_peaksymmetry.txt";

    std::ofstream responseOut(responsePath);
    std::ofstream phaseOut(phasePath);
    std::ofstream chebyshevOut(chebyshevPath);
    std::ofstream stabilityOut(stabilityPath);
    std::ofstream peakSymmetryOut(peakSymmetryPath);
    if (!responseOut || !phaseOut || !chebyshevOut || !stabilityOut || !peakSymmetryOut)
    {
        std::cerr << "BiquadsExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeFamilyResponses(responseOut);
    writePhaseResponses(phaseOut);
    writeChebyshevComparison(chebyshevOut);
    writeStability(stabilityOut);
    writePeakSymmetry(peakSymmetryOut);

    std::cout << "BiquadsExplore: wrote " << responsePath << ", " << phasePath << ", " << chebyshevPath << ", "
              << stabilityPath << ", and " << peakSymmetryPath << std::endl;
}
