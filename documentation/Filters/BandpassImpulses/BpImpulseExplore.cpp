
#include <array>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include "Analysis/SimpleStats.h"
#include "Analysis/ZeroCrossings.h"
#include "Filters/BiquadResoBP.h"
#include "Filters/BiquadResoBPParallelSIMD.h"
#include "Filters/BiquadResoBandPassParallel.h"
#include "Filters/SvfResoBP.h"
#include "Generators/Excitation.h"
#include "Generators/ResoGenerator.h"
#include "Numbers/Convert.h"

static constexpr float sampleRate{48000.f};

std::size_t findLastAboveEpsilon(const std::vector<float>& v, float epsilon) noexcept
{
    const auto it = std::find_if(v.rbegin(), v.rend(), [epsilon](float x) noexcept { return std::fabs(x) > epsilon; });

    if (it == v.rend())
    {
        return v.size(); // "not found" indicator
    }

    return static_cast<std::size_t>(std::distance(v.begin(), it.base() - 1));
}
void checkCompensationModelForWaveExcitation()
{
    AbacDsp::SimpleStats<float> statsAll;
    const auto T60 = Convert::dbToGain(-60.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "{";
    float decayStart = 0.0078125f / 8.f;
    float decayEnd = 64.f;
    for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
    {
        std::cout << static_cast<size_t>(decay * 48000) << "f,";
    }
    std::cout << "};\n";
    for (int n = 0; n <= 132; n += 1)
    {
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        std::cout << "{";
        for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
        {
            AbacDsp::SvfResoBP sut{sampleRate};
            sut.setByDecay(0, freq, decay);
            sut.reset(0, AbacDsp::ResonanceCompensation::compensate(n, decay));
            // sut.reset(0, 1.f);

            const int periodLength = 1 + static_cast<int>(ceil(sampleRate / freq));
            float maxValue = 0;
            int decayTime = 0;
            std::vector<float> result;
            for (size_t j = 0; decayTime < 48000 * decay * 2; ++j)
            {
                float localMax = 0;
                for (int i = 0; i < periodLength * 2; ++i)
                {
                    decayTime++;
                    std::array<float, 1> out{};
                    sut.process0(out.data(), 1);
                    const auto v = out[0];
                    result.push_back(v);
                    maxValue = std::max(std::abs(v), maxValue);
                    localMax = std::max(std::abs(v), localMax);
                }
                if (decayTime > 1000 && localMax < T60)
                {
                    break;
                }
            }
            // std::cout << findLastAboveEpsilon(result, T60) / (decay * 48000.f) << "\t";
            std::cout << " " << std::setprecision(8) << maxValue << "f";
            // std::cout << " " << std::setprecision(8) << std::log(1 / maxValue) << "f";
            if (decay * 2 <= decayEnd)
            {
                std::cout << ", ";
            }
            statsAll.addDataPoint(maxValue);
        }
        std::cout << "}, //" << n << "\t" << freq << "\n";
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
}

void computeCompensationModelForWaveExcitation(int start, int step)
{
    AbacDsp::SimpleStats<float> statsAll;
    const auto T60 = Convert::dbToGain(-60.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "{";
    float decayStart = 0.0078125f / 8.f;
    float decayEnd = 64.f;
    for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
    {
        std::cout << static_cast<size_t>(decay * 48000) << "f,";
    }
    std::cout << "};\n";
    for (int n = start; n <= 132; n += step)
    {
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        std::cout << "{";
        for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
        {
            AbacDsp::SvfResoBP sut{sampleRate};
            sut.setByDecay(0, freq, decay);
            sut.reset(0, AbacDsp::ResonanceCompensation::compensate(n, decay));
            sut.reset(0, 1.f);

            const int periodLength = 1 + static_cast<int>(ceil(sampleRate / freq));
            float maxValue = 0;
            int decayTime = 0;
            std::vector<float> result;
            for (size_t j = 0; decayTime < 48000 * decay * 2; ++j)
            {
                float localMax = 0;
                for (int i = 0; i < periodLength * 4; ++i)
                {
                    decayTime++;
                    std::array<float, 1> out{};
                    sut.process0(out.data(), 1);
                    const auto v = out[0];
                    result.push_back(v);
                    maxValue = std::max(std::abs(v), maxValue);
                    localMax = std::max(std::abs(v), localMax);
                }
                if (decayTime > 1000 && localMax < T60)
                {
                    break;
                }
            }
            // std::cout << findLastAboveEpsilon(result, T60) / (decay * 48000.f) << "\t";
            // std::cout << " " << std::setprecision(8) << maxValue << "f";
            std::cout << " " << std::setprecision(8) << std::log(1 / maxValue) << "f";
            if (decay * 2 <= decayEnd)
            {
                std::cout << ", ";
            }
            statsAll.addDataPoint(maxValue);
        }
        std::cout << "}, //" << n << "\t" << freq << "\n";
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
}

// ---- verification plots for the ResoBP filter family bug fixes ----
// Output is plain text in documentation/Plot/PyConPlot.py's "@New plot:"/"#group" format;
// see README.md.

namespace
{

/// @brief Steady-state sine response, read from the last quarter of totalSamples so any
/// startup transient has settled. step is called once per sample with the drive value.
template <typename StepFn>
[[nodiscard]] float measureSteadyStateAmplitude(const size_t totalSamples, const float frequency,
                                                const float inputAmplitude, StepFn step)
{
    const auto steadyStart = totalSamples * 3 / 4;
    float peak = 0.f;
    for (size_t i = 0; i < totalSamples; ++i)
    {
        const float in =
            inputAmplitude * std::sin(2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(i) / sampleRate);
        const float out = step(in);
        if (i >= steadyStart)
        {
            peak = std::max(peak, std::abs(out));
        }
    }
    return peak;
}

/// @brief Single-impulse 60 dB decay time, tracked as a per-period local maximum (like the
/// unit tests) rather than a raw sample threshold, so a zero crossing isn't mistaken for decay.
template <typename FilterT>
[[nodiscard]] float measureT60Seconds(FilterT& filter, const float frequency, const float maxSeconds)
{
    const auto periodLength = static_cast<size_t>(sampleRate / frequency) + 1;
    const auto maxSamples = static_cast<size_t>(maxSeconds * sampleRate);
    float peak = 0.f;
    float localMax = 0.f;
    size_t periodStart = 0;
    for (size_t i = 0; i < maxSamples; ++i)
    {
        const float out = filter.step(i == 0 ? 1.f : 0.f);
        peak = std::max(peak, std::abs(out));
        localMax = std::max(localMax, std::abs(out));
        if (i + 1 - periodStart >= periodLength)
        {
            if (peak > 0.f && localMax < peak * 1e-3f) // -60 dB relative to the impulse peak
            {
                return static_cast<float>(periodStart) / sampleRate;
            }
            localMax = 0.f;
            periodStart = i + 1;
        }
    }
    return maxSeconds; // never decayed within the window
}

/// @brief Sweeps loHz..hiHz through resetFn/stepFn, normalizing to 0 dB at the curve's own
/// peak. SvfResoBP's raw (uncompensated) gain at resonance is deliberately not unity - see
/// ResonanceCompensation - so only shape, not absolute level, is comparable across classes.
template <typename ResetFn, typename StepFn>
void writeNormalizedSweep(std::ofstream& out, const std::string_view name, const size_t samples, const float loHz,
                          const float hiHz, ResetFn resetFn, StepFn stepFn)
{
    constexpr float probeAmplitude = 0.1f;
    std::vector<std::pair<float, float>> points;
    float peak = 1e-9f;
    for (float hz = loHz; hz <= hiHz; hz *= 1.005f)
    {
        resetFn();
        const float amp = measureSteadyStateAmplitude(samples, hz, probeAmplitude, stepFn);
        points.emplace_back(hz, amp);
        peak = std::max(peak, amp);
    }
    out << "#" << name << "\n";
    for (const auto& [hz, amp] : points)
    {
        out << hz << " " << 20.f * std::log10(std::max(amp / peak, 1e-6f)) << "\n";
    }
}

/// @brief One subplot per class rather than one overlaid plot: SvfResoBP and BiquadResoBP
/// land on nearly the same curve, which is the point, but makes an overlay hard to read as
/// two distinct lines rather than one.
void writeResponse(std::ofstream& out)
{
    constexpr float f0 = 1000.f;
    constexpr float decay = 0.1f;
    constexpr size_t kSamples = 8192;
    constexpr float loHz = 200.f;
    constexpr float hiHz = 5000.f;

    AbacDsp::SvfResoBP svf{sampleRate};
    svf.setByDecay(0, f0, decay);
    out << "@New plot: title=\"SvfResoBP magnitude (f0=1kHz, decay=100ms)\" logx=true\n";
    writeNormalizedSweep(
        out, "SvfResoBP", kSamples, loHz, hiHz, [&svf] { svf.reset(); },
        [&svf](const float in) { return svf.step(in); });

    AbacDsp::BiquadResoBP biquad{sampleRate};
    biquad.setByDecay(0, f0, decay);
    out << "@New plot: title=\"BiquadResoBP magnitude (f0=1kHz, decay=100ms)\" logx=true\n";
    writeNormalizedSweep(
        out, "BiquadResoBP", kSamples, loHz, hiHz, [&biquad] { biquad.reset(); },
        [&biquad](const float in) { return biquad.step(in); });
}

/// @brief One subplot per class, plotting the relative error against the requested decay time
/// rather than measured-vs-requested overlaid on a y=x line: at this accuracy, the overlay and
/// the reference line are indistinguishable, while the error is immediately readable. Swept
/// geometrically rather than a handful of fixed points, since nothing here is expensive enough
/// to need coarsening (worst case a few seconds of decay to measure, once).
/// y-axis is symlog (linear within +/-1%, log beyond) so the sub-1% settled region and the
/// short-decay spike are both legible in one plot. The +/-10% band is a commonly-cited,
/// approximate order-of-magnitude reference for reverberation-time JND (see README.md) -
/// context only, not a precise perceptual threshold.
template <typename FilterT>
void writeDecayErrorSweep(std::ofstream& out, const std::string_view title, const float f0, const float loDecay,
                          const float hiDecay)
{
    out << "@New plot: title=\"" << title << "\" logx=true symlogy=true linthreshy=1.0\n#error (%)\n";
    for (float t = loDecay; t <= hiDecay; t *= 1.02f)
    {
        FilterT filter{sampleRate};
        filter.setByDecay(0, f0, t);
        const float measured = measureT60Seconds(filter, f0, t * 4.f);
        out << t << " " << (measured - t) / t * 100.f << "\n";
    }
    out << "#~10% (approx. reverberation-time JND, context only)\n" << loDecay << " 10\n" << hiDecay << " 10\n";
    out << "#~10% (approx. reverberation-time JND, context only)\n" << loDecay << " -10\n" << hiDecay << " -10\n";
}

void writeDecayAccuracy(std::ofstream& out)
{
    constexpr float f0 = 440.f;
    constexpr float loDecay = 0.02f;
    constexpr float hiDecay = 2.56f;

    writeDecayErrorSweep<AbacDsp::SvfResoBP>(out, "SvfResoBP: 60 dB decay-time error (f0=440 Hz)", f0, loDecay,
                                             hiDecay);
    writeDecayErrorSweep<AbacDsp::BiquadResoBP>(out, "BiquadResoBP: 60 dB decay-time error (f0=440 Hz)", f0, loDecay,
                                                hiDecay);
}

/// @brief Peak amplitude after a ResonanceCompensation-derived reset, across a decay sweep
/// deliberately not landing on the LUT's power-of-two columns. 0 dB is the target: an exactly
/// compensated resonator. Window matches SvfResoBPTest.checkCompensationModelForWaveExcitation.
/// The sweep starts at 0.15 s so even the lowest note here (36, ~65 Hz) gets several full
/// periods before the 60 dB point - below that, decay time and period length are comparable
/// and "peak amplitude" stops being a meaningful measurement independent of this fix.
void writeCompensationAccuracy(std::ofstream& out)
{
    constexpr size_t kMeasureSamples = 2000;
    out << "@New plot: title=\"ResonanceCompensation peak amplitude across a dense decay sweep (0 dB = exact)\" "
           "logx=true\n";
    const std::array<float, 4> notes{36.f, 60.f, 84.f, 108.f};
    for (const float note : notes)
    {
        const auto freq = Convert::noteToFrequency(note);
        out << "#note=" << note << "\n";
        for (float decay = 0.15f; decay <= 40.f; decay *= 1.03f) // not power-of-two: lands between LUT columns
        {
            AbacDsp::SvfResoBP filter{sampleRate};
            filter.setByDecay(0, freq, decay);
            filter.reset(0, AbacDsp::ResonanceCompensation::compensate(note, decay));

            float peak = 0.f;
            for (size_t i = 0; i < kMeasureSamples; ++i)
            {
                peak = std::max(peak, std::abs(filter.step0()));
            }
            out << decay << " " << 20.f * std::log10(std::max(peak, 1e-6f)) << "\n";
        }
    }
}

void writePitchBend(std::ofstream& out)
{
    constexpr float baseFreq = 440.f;
    constexpr float decayTime = 2.f;
    constexpr size_t stabilizeSamples = 480;
    constexpr size_t measureSamples = 4800;

    const auto measureFreq = [](AbacDsp::SvfResoBP& filter) -> float
    {
        std::vector<float> signal(measureSamples);
        for (size_t i = 0; i < measureSamples; ++i)
        {
            signal[i] = filter.step(0.f);
        }
        const auto stats = AbacDsp::calculateZeroCrossingStatistics(signal.data(), measureSamples, true);
        return sampleRate / stats.meanPeriodLen;
    };

    const auto expectedFreq = [](const float cents) { return baseFreq * std::exp2(cents / 1200.f); };
    const auto centsError = [](const float measured, const float expected)
    { return 1200.f * std::log2(measured / expected); };

    out << "@New plot: title=\"pitch-bend frequency error, before damp() switch\"\n#error (cents)\n";
    for (float cents = -1200.f; cents <= 1200.f; cents += 10.f)
    {
        AbacDsp::SvfResoBP filter{sampleRate};
        filter.setByDecay(0, baseFreq, decayTime);
        filter.pitchBendCents(cents);
        for (size_t i = 0; i < stabilizeSamples; ++i)
        {
            (void) filter.step(i == 0 ? 1024.f : 0.f);
        }
        out << cents << " " << centsError(measureFreq(filter), expectedFreq(cents)) << "\n";
    }

    out << "@New plot: title=\"pitch-bend frequency error, after damp() switch to the other set\"\n#error (cents)\n";
    for (float cents = -1200.f; cents <= 1200.f; cents += 10.f)
    {
        AbacDsp::SvfResoBP filter{sampleRate};
        filter.setByDecay(0, baseFreq, decayTime);
        filter.setByDecay(1, baseFreq, decayTime);
        filter.pitchBendCents(cents);
        for (size_t i = 0; i < stabilizeSamples; ++i)
        {
            (void) filter.step(i == 0 ? 1024.f : 0.f);
        }
        filter.damp(true);
        out << cents << " " << centsError(measureFreq(filter), expectedFreq(cents)) << "\n";
    }
}

void writeTopology(std::ofstream& out)
{
    constexpr float f0 = 800.f;
    constexpr float decay = 0.15f;
    constexpr size_t kSamples = 8192;
    constexpr float loHz = 200.f;
    constexpr float hiHz = 3000.f;

    AbacDsp::SvfResoBP svf{sampleRate};
    svf.setByDecay(0, f0, decay);
    out << "@New plot: title=\"SvfResoBP (f0=800 Hz, decay=150 ms)\" logx=true\n";
    writeNormalizedSweep(
        out, "SvfResoBP", kSamples, loHz, hiHz, [&svf] { svf.reset(); },
        [&svf](const float in) { return svf.step(in); });

    AbacDsp::BiquadResoBP biquad{sampleRate};
    biquad.setByDecay(0, f0, decay);
    out << "@New plot: title=\"BiquadResoBP (f0=800 Hz, decay=150 ms)\" logx=true\n";
    writeNormalizedSweep(
        out, "BiquadResoBP", kSamples, loHz, hiHz, [&biquad] { biquad.reset(); },
        [&biquad](const float in) { return biquad.step(in); });

    AbacDsp::BiquadResoBandPassParallel<1, 1> scalarBank{sampleRate};
    scalarBank.setByDecay(0, 0, f0, decay);
    out << "@New plot: title=\"BiquadResoBandPassParallel, element 0 of a 1-element bank "
           "(f0=800 Hz, decay=150 ms)\" logx=true\n";
    writeNormalizedSweep(
        out, "BiquadResoBandPassParallel", kSamples, loHz, hiHz, [&scalarBank] { scalarBank.reset(0); },
        [&scalarBank](const float in) { return scalarBank.step(0, in); });

    AbacDsp::BiquadResoBPParallelSIMD<4, 1> simdBank{sampleRate};
    simdBank.setByDecay(0, 0, f0, decay);
    std::array<float, 1> simdIn{};
    std::array<float, 1> simdOut{};
    out << "@New plot: title=\"BiquadResoBPParallelSIMD, element 0 of a 4-element bank "
           "(f0=800 Hz, decay=150 ms)\" logx=true\n";
    writeNormalizedSweep(
        out, "BiquadResoBPParallelSIMD", kSamples, loHz, hiHz, [&simdBank] { simdBank.reset(0); },
        [&simdBank, &simdIn, &simdOut](const float in)
        {
            simdIn[0] = in;
            simdBank.process(simdIn.data(), simdOut.data());
            return simdOut[0];
        });
}

/// @brief Peak-hold envelope follower, decaying at `release` per sample - smooths the intra-period
/// ripple of a ringing bandpass without needing period alignment.
[[nodiscard]] float peakHold(const float previous, const float sampleValue, const float release) noexcept
{
    return std::max(std::abs(sampleValue), previous * release);
}

/// @brief Output envelope (dB, normalized to its own peak) plotted alongside isActive() (mapped
/// to 0 dB active / -80 dB inactive, same axis), so the state can be checked directly against
/// real, audible signal presence rather than read as a bare timeline with no reference.
void writeIsActive(std::ofstream& out)
{
    constexpr float release = 0.98f; // ~50-sample peak-hold time constant
    constexpr size_t kSamples = 1500;

    out << "@New plot: title=\"BiquadResoBP: output envelope vs. isActive() (f0=1kHz, decay=10ms)\"\n";
    {
        AbacDsp::BiquadResoBP filter{sampleRate};
        filter.setByDecay(0, 1000.f, 0.01f);
        filter.reset(1.f, 0.f); // matches the SIMD bank's reset(0, 1.f, 0.f) below: same absolute scale,
                                // so both panels' energy-threshold crossings land in comparable places
        filter.triggered();

        std::vector<float> envelope;
        std::vector<float> state;
        float env = 0.f;
        for (size_t i = 0; i < kSamples; ++i)
        {
            env = peakHold(env, filter.step(0.f), release);
            envelope.push_back(env);
            state.push_back(filter.isActive() ? 1.f : 0.f);
        }
        const float peak = *std::max_element(envelope.begin(), envelope.end());
        out << "#output envelope (dB)\n";
        for (size_t i = 0; i < envelope.size(); ++i)
        {
            out << i << " " << 20.f * std::log10(std::max(envelope[i] / peak, 1e-6f)) << "\n";
        }
        out << "#isActive() (0 dB=active, -80 dB=inactive)\n";
        for (size_t i = 0; i < state.size(); ++i)
        {
            out << i << " " << (state[i] > 0.5f ? 0.f : -80.f) << "\n";
        }
    }

    out << "@New plot: title=\"BiquadResoBPParallelSIMD element 0: output envelope vs. isActive() "
           "(4-element bank)\"\n";
    {
        AbacDsp::BiquadResoBPParallelSIMD<4, 1> bank{sampleRate};
        bank.setByDecay(0, 0, 1000.f, 0.01f);
        bank.reset(0, 1.f, 0.f);
        const std::array<float, 1> silence{};

        std::vector<float> envelope;
        std::vector<float> state;
        float env = 0.f;
        for (size_t i = 0; i < kSamples; ++i)
        {
            std::array<float, 1> outBuf{};
            bank.process(silence.data(), outBuf.data());
            env = peakHold(env, outBuf[0], release);
            envelope.push_back(env);
            state.push_back(bank.isActive(0) ? 1.f : 0.f);
        }
        const float peak = *std::max_element(envelope.begin(), envelope.end());
        out << "#output envelope (dB)\n";
        for (size_t i = 0; i < envelope.size(); ++i)
        {
            out << i << " " << 20.f * std::log10(std::max(envelope[i] / peak, 1e-6f)) << "\n";
        }
        out << "#isActive() (0 dB=active, -80 dB=inactive)\n";
        for (size_t i = 0; i < state.size(); ++i)
        {
            out << i << " " << (state[i] > 0.5f ? 0.f : -80.f) << "\n";
        }
    }
}

}

int main(int argc, char* argv[])
{
    const std::array<std::string, 6> defaultNames{
        "rb_response.txt",  "rb_decay_accuracy.txt", "rb_compensation_accuracy.txt",
        "rb_pitchbend.txt", "rb_topology.txt",       "rb_isactive.txt"};
    std::array<std::string, 6> paths{};
    for (size_t i = 0; i < paths.size(); ++i)
    {
        paths[i] = (static_cast<int>(i) + 1 < argc) ? argv[i + 1] : defaultNames[i];
    }

    std::array<std::ofstream, 6> outs{};
    for (size_t i = 0; i < outs.size(); ++i)
    {
        outs[i].open(paths[i]);
        if (!outs[i])
        {
            std::cerr << "BpImpulseExplore: ERROR - failed to open " << paths[i] << " for writing\n";
            return 1;
        }
    }

    std::cout << "response:" << std::endl;
    writeResponse(outs[0]);
    std::cout << "decay accuracy:" << std::endl;
    writeDecayAccuracy(outs[1]);
    std::cout << "compensation accuracy:" << std::endl;
    writeCompensationAccuracy(outs[2]);
    std::cout << "pitch bend:" << std::endl;
    writePitchBend(outs[3]);
    std::cout << "topology:" << std::endl;
    writeTopology(outs[4]);
    std::cout << "isActive:" << std::endl;
    writeIsActive(outs[5]);

    std::cout << "BpImpulseExplore: wrote";
    for (const auto& p : paths)
    {
        std::cout << " " << p;
    }
    std::cout << std::endl;
    return 0;
}

/// @brief Dumps the ResonanceCompensation LUT source data to stdout, for hand-pasting into
/// SvfResoBP.h if the compensation model is ever refit. Not wired into main(); call this
/// from main() instead, temporarily, when actually regenerating the table. See README.md.
void writeRawCompensationLutDump()
{
    for (size_t i = 0; i < 128; ++i)
    {
        std::cout << i << "\t" << AbacDsp::ResonanceCompensation::compensate(i, 0.5f) << "\n";
    }
    checkCompensationModelForWaveExcitation();
    computeCompensationModelForWaveExcitation(0, 12);
}