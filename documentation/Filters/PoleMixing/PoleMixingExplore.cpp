// Generates verification/demonstration plots for the pole-mixing filter family
// (src/includes/Filters/PoleMixingFilter.h): magnitude/phase curves for a representative
// set of poleMixingList presets, resonance behavior up to self-oscillation, overdrive
// (saturator) behavior, cutoff-frequency correctness just below self-oscillation, real-time
// behavior when cutoff/resonance change while running, and the difference between the two
// resonance topologies still in the file: classic last-stage feedback (FixedFourStageFilter)
// vs. bandpass-tap feedback (Filter1Pole4StageSmooth).
// Output is plain text in documentation/Plot/PyConPlot.py's "@New plot:"/"#group" format;
// see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Filters/PoleMixingFilter.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kFftSize = 4096;

// ---- shared measurement helpers ----

template <typename FilterT>
[[nodiscard]] bool impulseResponseIsStable(FilterT& filter)
{
    constexpr size_t kSamples = 16384;
    constexpr size_t kLateStart = kSamples * 3 / 4;
    filter.reset();
    float peak = 0.f;
    float lateMax = 0.f;
    for (size_t i = 0; i < kSamples; ++i)
    {
        const float in = (i == 0) ? 0.1f : 0.f;
        const float out = filter.step(in);
        if (!std::isfinite(out))
        {
            return false;
        }
        peak = std::max(peak, std::abs(out));
        if (i >= kLateStart)
        {
            lateMax = std::max(lateMax, std::abs(out));
        }
    }
    return lateMax < peak * 0.5f;
}

/// @brief Largest resonance (in the filter's own "user" units) whose impulse response still
/// decays. Bisected empirically on the running filter rather than derived analytically,
/// since only one of the two topologies here has a closed form (see README.md).
template <typename MakeFilterFn>
[[nodiscard]] float findCriticalResonance(MakeFilterFn makeFilter)
{
    float hi = 8.f;
    while (hi < 256.f)
    {
        auto probe = makeFilter(hi);
        if (!impulseResponseIsStable(probe))
        {
            break;
        }
        hi *= 2.f;
    }
    float lo = 0.f;
    for (int i = 0; i < 24; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        auto probe = makeFilter(mid);
        if (impulseResponseIsStable(probe))
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    return lo;
}

template <typename FilterT>
[[nodiscard]] float measureSteadyStateAmplitude(FilterT& filter, const float frequency,
                                                const float inputAmplitude = 0.05f)
{
    constexpr size_t kSamples = 16384;
    constexpr size_t kSteadyStart = kSamples * 3 / 4;
    filter.reset();
    float peak = 0.f;
    for (size_t i = 0; i < kSamples; ++i)
    {
        const float in = inputAmplitude *
                         std::sin(2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(i) / kSampleRate);
        const float out = filter.step(in);
        if (i >= kSteadyStart)
        {
            peak = std::max(peak, std::abs(out));
        }
    }
    return peak;
}

/// @brief Global peak over a fixed, wide range rather than a window around the requested
/// cutoff - the correction under test is exactly what might push the true peak well away
/// from the request, and a narrow search window would silently hide that.
template <typename FilterT>
[[nodiscard]] float findPeakFrequency(FilterT& filter, const float loHz, const float hiHz)
{
    float bestHz = loHz;
    float bestAmp = -1.f;
    for (float hz = loHz; hz <= hiHz; hz *= 1.02f)
    {
        const float amp = measureSteadyStateAmplitude(filter, hz);
        if (amp > bestAmp)
        {
            bestAmp = amp;
            bestHz = hz;
        }
    }
    return bestHz;
}

/// @brief Sweeps loHz..hiHz, normalizing to 0 dB at the curve's own peak - the topology
/// comparisons care about peak shape and low-end level relative to the resonance peak, not
/// absolute gain, which differs between the classic and bandpass-tap weight conventions.
template <typename MakeFilterFn>
void writeNormalizedSweep(std::ofstream& out, const std::string& name, MakeFilterFn makeFilter, const float loHz,
                          const float hiHz)
{
    const float critical = findCriticalResonance(makeFilter);
    auto filter = makeFilter(critical * 0.9f);
    std::vector<std::pair<float, float>> points;
    float peak = 1e-9f;
    for (float hz = loHz; hz <= hiHz; hz *= 1.03f)
    {
        const float amp = measureSteadyStateAmplitude(filter, hz);
        points.emplace_back(hz, amp);
        peak = std::max(peak, amp);
    }
    out << "#" << name << "\n";
    for (const auto& [hz, amp] : points)
    {
        out << hz << " " << 20.f * std::log10(std::max(amp / peak, 1e-6f)) << "\n";
    }
}

template <typename StepFn>
void writeEnvelope(std::ofstream& out, const size_t totalSamples, const size_t windowSamples, StepFn step)
{
    float peak = 0.f;
    for (size_t i = 0; i < totalSamples; ++i)
    {
        peak = std::max(peak, std::abs(step(i)));
        if ((i + 1) % windowSamples == 0)
        {
            out << (static_cast<float>(i) / kSampleRate) << " " << peak << "\n";
            peak = 0.f;
        }
    }
}

void writeMagnitudeSpectrum(std::ofstream& out, const std::vector<float>& window)
{
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(window, magnitude);
    for (size_t bin = 1; bin < kFftSize / 4; ++bin) // up to sampleRate/8: plenty for a 300 Hz probe's harmonics
    {
        const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
        const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
        out << hz << " " << db << "\n";
    }
}

// ---- magnitude/phase curves for representative presets ----

constexpr std::array<std::string_view, 11> kRepresentativePresets{"LP1", "LP2", "LP4", "HP1", "HP2",  "HP4",
                                                                  "BP2", "BP4", "AP2", "AP4", "Notch"};
constexpr float kResponseCutoff = 1000.f;
constexpr float kResponseFreqLo = 50.f;
constexpr float kResponseFreqHi = 12000.f;

void writeResponse(std::ofstream& out)
{
    out << "@New plot: title=\"magnitude (cutoff=1kHz, resonance=0)\" logx=true\n";
    for (const auto presetName : kRepresentativePresets)
    {
        const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex(presetName)].cf;
        AbacDsp::FourStageFilterTheoretical<float> theoretical(kSampleRate, cf);
        out << "#" << presetName << "\n";
        for (float hz = kResponseFreqLo; hz <= kResponseFreqHi; hz *= 1.05f)
        {
            const float mag = static_cast<float>(theoretical.magnitudeBP(kResponseCutoff, hz, 0.f));
            out << hz << " " << 20.f * std::log10(std::max(mag, 1e-6f)) << "\n";
        }
    }

    out << "@New plot: title=\"phase (cutoff=1kHz, resonance=0)\" logx=true\n";
    for (const auto presetName : kRepresentativePresets)
    {
        const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex(presetName)].cf;
        AbacDsp::FourStageFilterTheoretical<float> theoretical(kSampleRate, cf);
        out << "#" << presetName << "\n";
        for (float hz = kResponseFreqLo; hz <= kResponseFreqHi; hz *= 1.05f)
        {
            float phase = 0.f;
            theoretical.phase(hz / kResponseCutoff, 0.f, phase);
            out << hz << " " << phase * 180.f / std::numbers::pi_v<float> << "\n";
        }
    }
}

// ---- resonance behavior, up to self-oscillation (bandpass-tap topology) ----

void writeResonanceSweep(std::ofstream& out, const std::string_view presetName)
{
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex(presetName)].cf;
    const auto makeFilter = [&cf](const float resonance)
    {
        AbacDsp::Filter1Pole4StageSmooth f(kSampleRate);
        f.setFilterCoefficients(cf);
        f.setParameterSmoothTimeMs(2.f);
        f.setCutoffFrequencyClean(kResponseCutoff);
        f.setResonance(resonance);
        return f;
    };
    // resonance is normalized (1.0 = measured critical), so these fractions are the
    // resonance values directly - no separate critical-resonance lookup needed here.
    for (const float resonance : {0.f, 0.5f, 0.8f, 0.95f})
    {
        auto filter = makeFilter(resonance);
        out << "#resonance=" << resonance << "\n";
        for (float hz = 30.f; hz <= kResponseFreqHi; hz *= 1.04f)
        {
            const float amp = measureSteadyStateAmplitude(filter, hz);
            out << hz << " " << 20.f * std::log10(std::max(amp / 0.05f, 1e-6f)) << "\n";
        }
    }
}

void writeResonance(std::ofstream& out)
{
    out << "@New plot: title=\"LP4 approaching self-oscillation\" logx=true\n";
    writeResonanceSweep(out, "LP4");
    out << "@New plot: title=\"BP4 approaching self-oscillation\" logx=true\n";
    writeResonanceSweep(out, "BP4");

    out << "@New plot: title=\"LP4 self-oscillation (single impulse, zero input after)\"\n#envelope\n";
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf;
    AbacDsp::Filter1Pole4StageSmooth filter(kSampleRate);
    filter.setFilterCoefficients(cf);
    filter.setParameterSmoothTimeMs(2.f);
    filter.setCutoffFrequencyClean(kResponseCutoff);
    filter.setResonance(1.05f); // 5% past the normalized self-oscillation threshold
    writeEnvelope(out, static_cast<size_t>(1.5f * kSampleRate), 64,
                  [&filter](const size_t i) { return filter.step(i == 0 ? 0.02f : 0.f); });
}

// ---- overdrive / saturator behavior ----

void writeOverdriveSpectra(std::ofstream& out, const std::string_view title)
{
    out << "@New plot: title=\"" << title << " spectrum vs. input level\" logx=true\n";
    for (const float amplitude : {0.05f, 0.5f, 2.f})
    {
        AbacDsp::Lp24Smooth filter(kSampleRate);
        filter.setCutoff(2000.f);
        filter.setResonance(0.f);
        std::vector<float> rendered(kFftSize + 400);
        for (size_t i = 0; i < rendered.size(); ++i)
        {
            const float in =
                amplitude * std::sin(2.f * std::numbers::pi_v<float> * 300.f * static_cast<float>(i) / kSampleRate);
            rendered[i] = filter.step(in);
        }
        out << "#level=" << amplitude << "\n";
        const std::vector<float> window(rendered.end() - static_cast<std::ptrdiff_t>(kFftSize), rendered.end());
        writeMagnitudeSpectrum(out, window);
    }
}

void writeOverdriveSpectraBandpass(std::ofstream& out, const std::string_view title)
{
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf;
    out << "@New plot: title=\"" << title << " spectrum vs. input level\" logx=true\n";
    for (const float amplitude : {0.05f, 0.5f, 2.f})
    {
        AbacDsp::Filter1Pole4StageSmooth filter(kSampleRate);
        filter.setFilterCoefficients(cf);
        filter.setParameterSmoothTimeMs(2.f);
        filter.setCutoffFrequencyClean(2000.f);
        filter.setResonance(0.f);
        std::vector<float> rendered(kFftSize + 400);
        for (size_t i = 0; i < rendered.size(); ++i)
        {
            const float in =
                amplitude * std::sin(2.f * std::numbers::pi_v<float> * 300.f * static_cast<float>(i) / kSampleRate);
            rendered[i] = filter.step(in);
        }
        out << "#level=" << amplitude << "\n";
        const std::vector<float> window(rendered.end() - static_cast<std::ptrdiff_t>(kFftSize), rendered.end());
        writeMagnitudeSpectrum(out, window);
    }
}

void writeOverdrive(std::ofstream& out)
{
    out << "@New plot: title=\"saturator transfer curves\"\n#atan(x)\n";
    for (float x = -6.f; x <= 6.f; x += 0.05f)
    {
        out << x << " " << std::atan(x) << "\n";
    }
    out << "#x/sqrt(1+x^2)\n";
    for (float x = -6.f; x <= 6.f; x += 0.05f)
    {
        out << x << " " << AbacDsp::Filter1Pole4StageSmooth::compress(x) << "\n";
    }

    writeOverdriveSpectra(out, "classic (atan) LP4");
    writeOverdriveSpectraBandpass(out, "bandpass-tap (x/sqrt(1+x^2)) LP4");
}

// ---- cutoff-frequency correctness, just below self-oscillation ----

[[nodiscard]] std::vector<float> geometricSweep(const float loHz, const float hiHz, const float ratio)
{
    std::vector<float> hz;
    for (float f = loHz; f <= hiHz; f *= ratio)
    {
        hz.push_back(f);
    }
    return hz;
}

float measurePeakFrequencyClassic(const float requestedCutoff)
{
    const auto makeFilter = [requestedCutoff](const float resonance)
    {
        AbacDsp::Lp24Smooth f(kSampleRate);
        f.setCutoff(requestedCutoff);
        f.setResonance(resonance);
        return f;
    };
    const float critical = findCriticalResonance(makeFilter);
    auto filter = makeFilter(critical * 0.9f);
    return findPeakFrequency(filter, 50.f, 20000.f);
}

float measurePeakFrequencyBandpass(const float requestedCutoff)
{
    // resonance is normalized (1.0 = critical), so 0.9f directly is "just below
    // self-oscillation" - no separate critical-resonance search needed.
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf;
    AbacDsp::Filter1Pole4StageSmooth filter(kSampleRate);
    filter.setFilterCoefficients(cf);
    filter.setParameterSmoothTimeMs(2.f);
    filter.setCutoffFrequency(requestedCutoff); // warped: this is the correction under test
    filter.setResonance(0.9f);
    return findPeakFrequency(filter, 50.f, 20000.f);
}

void writeCutoffAccuracySweep(std::ofstream& out, const std::vector<float>& sweep)
{
    out << "#y=x (requested)\n";
    for (const float hz : sweep)
    {
        out << hz << " " << hz << "\n";
    }
    out << "#classic (warpCutoffForSampleRate)\n";
    for (const float requested : sweep)
    {
        out << requested << " " << measurePeakFrequencyClassic(requested) << "\n";
    }
    out << "#bandpass-tap (adaptResonanceFrequency)\n";
    for (const float requested : sweep)
    {
        out << requested << " " << measurePeakFrequencyBandpass(requested) << "\n";
    }
}

void writeCutoffAccuracy(std::ofstream& out)
{
    const float thirdSemitone = std::pow(2.f, 1.f / 36.f); // 36 steps/octave: a musical, not arbitrary, grid

    out << "@New plot: title=\"resonant-peak frequency vs. requested cutoff\" logx=true logy=true\n";
    writeCutoffAccuracySweep(out, geometricSweep(100.f, 12000.f, thirdSemitone));

    out << "@New plot: title=\"resonant-peak frequency vs. requested cutoff (zoom: 1000-12000 Hz)\"\n";
    writeCutoffAccuracySweep(out, geometricSweep(1000.f, 12000.f, thirdSemitone));
}

// ---- real-time behavior when cutoff/resonance change while running ----

float probeTone(const float hz, const size_t i)
{
    return 0.1f * std::sin(2.f * std::numbers::pi_v<float> * hz * static_cast<float>(i) / kSampleRate);
}

void writeCutoffJumpEnvelope(std::ofstream& out)
{
    constexpr float kProbeHz = 1500.f;
    constexpr float kLowCutoff = 800.f;
    constexpr float kHighCutoff = 4000.f;
    constexpr size_t kTotalSamples = static_cast<size_t>(0.3f * kSampleRate);
    constexpr size_t kJumpSample = kTotalSamples / 3;
    constexpr size_t kWindow = 32; // one full 1500 Hz probe period: resolves the 10ms glide without beating

    out << "#classic (linear ramp, 10ms)\n";
    {
        AbacDsp::Lp24Smooth filter(kSampleRate);
        filter.setCutoff(kLowCutoff);
        filter.setSmoothingSteps(static_cast<size_t>(0.01f * kSampleRate));
        writeEnvelope(out, kTotalSamples, kWindow,
                      [&filter](const size_t i)
                      {
                          if (i == kJumpSample)
                          {
                              filter.setCutoff(kHighCutoff);
                          }
                          const float in = probeTone(kProbeHz, i);
                          float outSample = 0.f;
                          filter.processBlock(&in, &outSample, 1); // step() alone never advances the ramp
                          return outSample;
                      });
    }
    out << "#bandpass-tap (exponential smoothing, 10ms)\n";
    {
        AbacDsp::Filter1Pole4StageSmooth filter(kSampleRate);
        filter.setFilterCoefficients(AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf);
        filter.setParameterSmoothTimeMs(10.f);
        filter.setCutoffFrequencyClean(kLowCutoff);
        filter.setResonance(0.f);
        writeEnvelope(out, kTotalSamples, kWindow,
                      [&filter](const size_t i)
                      {
                          if (i == kJumpSample)
                          {
                              filter.setCutoffFrequencyClean(kHighCutoff);
                          }
                          return filter.step(probeTone(kProbeHz, i));
                      });
    }
}

void writeResonanceJumpEnvelope(std::ofstream& out)
{
    constexpr float kCutoff = 1000.f;
    constexpr size_t kTotalSamples = static_cast<size_t>(0.3f * kSampleRate);
    constexpr size_t kJumpSample = kTotalSamples / 3;
    constexpr size_t kWindow = 48; // one full 1000 Hz probe period: resolves the glide without beating
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf;

    out << "#classic (no resonance smoothing)\n";
    {
        const auto makeFilter = [](const float resonance)
        {
            AbacDsp::Lp24Smooth f(kSampleRate);
            f.setCutoff(kCutoff);
            f.setResonance(resonance);
            return f;
        };
        const float critical = findCriticalResonance(makeFilter);
        AbacDsp::Lp24Smooth filter(kSampleRate);
        filter.setCutoff(kCutoff);
        filter.setResonance(0.f);
        writeEnvelope(out, kTotalSamples, kWindow,
                      [&filter, critical](const size_t i)
                      {
                          if (i == kJumpSample)
                          {
                              filter.setResonance(critical * 0.9f);
                          }
                          return filter.step(probeTone(kCutoff, i));
                      });
    }
    out << "#bandpass-tap (exponentially smoothed resonance)\n";
    {
        // resonance is normalized (1.0 = critical): no separate critical-resonance search
        // needed here, unlike the classic block above.
        AbacDsp::Filter1Pole4StageSmooth filter(kSampleRate);
        filter.setFilterCoefficients(cf);
        filter.setParameterSmoothTimeMs(10.f); // slower than the search's own probe: the glide should be visible
        filter.setCutoffFrequencyClean(kCutoff);
        filter.setResonance(0.f);
        writeEnvelope(out, kTotalSamples, kWindow,
                      [&filter](const size_t i)
                      {
                          if (i == kJumpSample)
                          {
                              filter.setResonance(0.9f);
                          }
                          return filter.step(probeTone(kCutoff, i));
                      });
    }
}

void writeRealtime(std::ofstream& out)
{
    out << "@New plot: title=\"cutoff jump 800->4000 Hz, probe tone 1500 Hz\"\n";
    writeCutoffJumpEnvelope(out);
    out << "@New plot: title=\"resonance jump near self-oscillation, probe tone at cutoff\"\n";
    writeResonanceJumpEnvelope(out);
}

// ---- classic vs. bandpass-tap feedback: the two topologies still in the file ----

template <typename ClassicFilterT>
void writeTopologyPairForPreset(std::ofstream& out, const std::string_view presetName)
{
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex(presetName)].cf;

    writeNormalizedSweep(
        out, std::string(presetName) + " classic",
        [](const float resonance)
        {
            ClassicFilterT f(kSampleRate);
            f.setCutoff(kResponseCutoff);
            f.setResonance(resonance);
            return f;
        },
        20.f, 15000.f);

    writeNormalizedSweep(
        out, std::string(presetName) + " bandpass-tap",
        [&cf](const float resonance)
        {
            AbacDsp::Filter1Pole4StageSmooth f(kSampleRate);
            f.setFilterCoefficients(cf);
            f.setParameterSmoothTimeMs(2.f);
            f.setCutoffFrequencyClean(kResponseCutoff);
            f.setResonance(resonance);
            return f;
        },
        20.f, 15000.f);
}

void writeTopologySelfOscillation(std::ofstream& out)
{
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf;
    constexpr size_t kTotalSamples = static_cast<size_t>(1.5f * kSampleRate);
    constexpr size_t kWindow = 64;

    out << "#classic\n";
    {
        const auto makeFilter = [](const float resonance)
        {
            AbacDsp::Lp24Smooth f(kSampleRate);
            f.setCutoff(kResponseCutoff);
            f.setResonance(resonance);
            return f;
        };
        auto filter = makeFilter(findCriticalResonance(makeFilter) * 1.05f);
        writeEnvelope(out, kTotalSamples, kWindow,
                      [&filter](const size_t i) { return filter.step(i == 0 ? 0.02f : 0.f); });
    }
    out << "#bandpass-tap\n";
    {
        // resonance is normalized (1.0 = critical): no separate critical-resonance search
        // needed here, unlike the classic block above.
        AbacDsp::Filter1Pole4StageSmooth filter(kSampleRate);
        filter.setFilterCoefficients(cf);
        filter.setParameterSmoothTimeMs(2.f);
        filter.setCutoffFrequencyClean(kResponseCutoff);
        filter.setResonance(1.05f);
        writeEnvelope(out, kTotalSamples, kWindow,
                      [&filter](const size_t i) { return filter.step(i == 0 ? 0.02f : 0.f); });
    }
}

void writeTopology(std::ofstream& out)
{
    out << "@New plot: title=\"LP4 resonance peak: classic vs bandpass-tap feedback\" logx=true\n";
    writeTopologyPairForPreset<AbacDsp::Lp24Smooth>(out, "LP4");

    out << "@New plot: title=\"self-oscillation: classic vs bandpass-tap feedback (LP4)\"\n";
    writeTopologySelfOscillation(out);

    out << "@New plot: title=\"LP4/HP4/BP4, classic vs bandpass-tap, near self-oscillation\" logx=true\n";
    writeTopologyPairForPreset<AbacDsp::Lp24Smooth>(out, "LP4");
    writeTopologyPairForPreset<AbacDsp::Hp24Smooth>(out, "HP4");
    writeTopologyPairForPreset<AbacDsp::Bp24Smooth>(out, "BP4");
}

// ---- raw (uncorrected) cutoff/resonance data, for fitPoleMixingCorrections.py ----

/// @brief Plain columns, not PyConPlot format: this feeds a curve-fitting script, not a plot.
/// Swept against setCutoffFrequencyClean() and setResonanceRaw() - the raw pole and feedback
/// parameters, with no correction of either kind applied - so the fit built from this data
/// can never be circular with the very corrections it produces (setResonance()'s
/// normalization is itself built from criticalResonanceForRawCutoff(), which this function
/// exists to (re)generate the input data for).
void writeRawCorrectionData(std::ofstream& out)
{
    const float thirdSemitone = std::pow(2.f, 1.f / 36.f);
    const auto& cf = AbacDsp::poleMixingList[AbacDsp::findFilterIndex("LP4")].cf;

    out << "# raw_cutoff_hz measured_peak_hz critical_resonance\n";
    for (const float rawCutoff : geometricSweep(20.f, 20000.f, thirdSemitone))
    {
        const auto makeFilter = [&cf, rawCutoff](const float resonance)
        {
            AbacDsp::Filter1Pole4StageSmooth f(kSampleRate);
            f.setFilterCoefficients(cf);
            f.setParameterSmoothTimeMs(2.f);
            f.setCutoffFrequencyClean(rawCutoff);
            f.setResonanceRaw(resonance);
            return f;
        };
        const float critical = findCriticalResonance(makeFilter);
        auto filter = makeFilter(critical * 0.9f);
        const float peak = findPeakFrequency(filter, 20.f, 22000.f);
        out << rawCutoff << " " << peak << " " << critical << "\n";
    }
}
}

int main(int argc, char* argv[])
{
    const std::array<std::string, 7> defaultNames{"pm_response.txt",           "pm_resonance.txt", "pm_overdrive.txt",
                                                  "pm_cutoff_accuracy.txt",    "pm_realtime.txt",  "pm_topology.txt",
                                                  "pm_raw_correction_data.txt"};
    std::array<std::string, 7> paths{};
    for (size_t i = 0; i < paths.size(); ++i)
    {
        paths[i] = (static_cast<int>(i) + 1 < argc) ? argv[i + 1] : defaultNames[i];
    }

    std::array<std::ofstream, 7> outs{};
    for (size_t i = 0; i < outs.size(); ++i)
    {
        outs[i].open(paths[i]);
        if (!outs[i])
        {
            std::cerr << "PoleMixingExplore: ERROR - failed to open " << paths[i] << " for writing\n";
            return 1;
        }
    }

    std::cout << "response:" << std::endl;
    writeResponse(outs[0]);
    std::cout << "resonance:" << std::endl;
    writeResonance(outs[1]);
    std::cout << "overdrive:" << std::endl;
    writeOverdrive(outs[2]);
    std::cout << "cutoff accuracy:" << std::endl;
    writeCutoffAccuracy(outs[3]);
    std::cout << "real time:" << std::endl;
    writeRealtime(outs[4]);
    std::cout << "topology:" << std::endl;
    writeTopology(outs[5]);
    std::cout << "raw correction:" << std::endl;
    writeRawCorrectionData(outs[6]);

    std::cout << "PoleMixingExplore: wrote";
    for (const auto& p : paths)
    {
        std::cout << " " << p;
    }
    std::cout << std::endl;
}
