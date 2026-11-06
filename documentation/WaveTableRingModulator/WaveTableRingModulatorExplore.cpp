// Verification plots for src/includes/Modulation/WaveTableRingModulator.h: carrier level against morph and pitch,
// the sideband lines, the carrier harmonics against theory, aliasing with the input lowpass open and closed, and the
// response of that lowpass. PyConPlot.py output; see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Filters/Chebyshev4Lowpass.h"
#include "Modulation/RingModulator.h"
#include "Modulation/WaveTableRingModulator.h"
#include "Wavetables/WaveTableOscillator.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr size_t kFftSize = 65536;
constexpr float kCarrierRms = AbacDsp::WaveTableRingModulator::kDefaultCarrierRms;

[[nodiscard]] double rmsOf(const std::vector<float>& v)
{
    double sum{0.0};
    for (const float x : v)
    {
        sum += static_cast<double>(x) * static_cast<double>(x);
    }
    return std::sqrt(sum / static_cast<double>(v.size()));
}

[[nodiscard]] double toDb(const double gain)
{
    return 20.0 * std::log10(std::max(std::abs(gain), 1e-12));
}

/// The carrier of the ring modulator alone: a constant input of 1 times the carrier.
[[nodiscard]] std::vector<float> carrierOf(AbacDsp::WaveTableRingModulator& sut, const size_t count)
{
    const std::vector<float> ones(count, 1.f);
    std::vector<float> out(count);
    sut.processBlock(ones, out);
    return out;
}

[[nodiscard]] double harmonicAmplitude(const std::vector<float>& v, const size_t cycles, const size_t k)
{
    double re{0.0};
    double im{0.0};
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double phase = kTwoPi * static_cast<double>(cycles * k * i) / static_cast<double>(v.size());
        re += static_cast<double>(v[i]) * std::cos(phase);
        im += static_cast<double>(v[i]) * std::sin(phase);
    }
    return 2.0 * std::hypot(re, im) / static_cast<double>(v.size());
}

// ---- Level against morph and pitch ----

/// Pitches in Hz. None is a multiple of a quarter of the sample rate: a carrier of exactly 12 kHz has its 2nd harmonic
/// on Nyquist, where the samples are all zero. One second is measured, so the cycle count need not be whole.
constexpr std::array<float, 6> kPitches{100.f, 1000.f, 3000.f, 6000.f, 9000.f, 11000.f};
constexpr size_t kLevelSamples = 48000;

[[nodiscard]] double rawOscillatorRmsDb(const AbacDsp::BasicWave third, const float hz, const float morph)
{
    AbacDsp::WaveTableOscillator osc{kSampleRate};
    osc.setWaveset(2, third);
    osc.setFrequency(hz);
    osc.setMorph(2.f * morph - 1.f);
    std::vector<float> block(kLevelSamples);
    osc.processBlock(block.data(), block.size());
    return toDb(rmsOf(block) / 0.5);
}

[[nodiscard]] double compensatedRmsDb(const float hz, const float morph)
{
    AbacDsp::WaveTableRingModulator sut{kSampleRate};
    sut.setFrequency(hz);
    sut.setMorph(morph);
    return toDb(rmsOf(carrierOf(sut, kLevelSamples)) / static_cast<double>(kCarrierRms));
}

void writeLevelPlot(std::ofstream& out, const char* title, const char* limits, const auto& levelAt, double& worst)
{
    out << "@New plot: title=\"" << title << "\" " << limits << "\n";
    for (const float hz : kPitches)
    {
        out << "#" << hz << " Hz\n";
        for (int step = 0; step <= 50; ++step)
        {
            const float morph = static_cast<float>(step) / 50.f;
            const double db = levelAt(hz, morph);
            worst = std::min(worst, db);
            out << morph << " " << db << "\n";
        }
    }
}

void writeLevel(std::ofstream& out)
{
    double sawLowest{0.0};
    double finLowest{0.0};
    double compensatedLowest{0.0};
    double compensatedHighest{0.0};
    writeLevelPlot(
        out, "Raw oscillator, Sine-Square-Saw (dB re 0.5)", "miny=-35 maxy=2", [](const float hz, const float morph)
        { return rawOscillatorRmsDb(AbacDsp::BasicWave::Saw, hz, morph); }, sawLowest);
    writeLevelPlot(
        out, "Raw oscillator, Sine-Square-SharkFin (dB re 0.5)", "miny=-3 maxy=1", [](const float hz, const float morph)
        { return rawOscillatorRmsDb(AbacDsp::BasicWave::SharkFin, hz, morph); }, finLowest);
    writeLevelPlot(
        out, "Compensated carrier, default slots (dB re 0.57)", "miny=-3 maxy=1",
        [&](const float hz, const float morph)
        {
            const double db = compensatedRmsDb(hz, morph);
            compensatedHighest = std::max(compensatedHighest, db);
            return db;
        },
        compensatedLowest);
    std::printf(
        "level: lowest raw Sine-Square-Saw %.2f dB, raw Sine-Square-SharkFin %.2f dB; compensated %.3f to %.3f dB\n",
        sawLowest, finLowest, compensatedLowest, compensatedHighest);
}

// ---- Sideband lines ----

void writeSidebands(std::ofstream& out)
{
    constexpr double kInputHz = 1365.0 * 48000.0 / static_cast<double>(kFftSize);
    constexpr double kCarrierHz = 410.0 * 48000.0 / static_cast<double>(kFftSize);
    AbacDsp::WaveTableRingModulator sut{kSampleRate};
    sut.setFrequency(static_cast<float>(kCarrierHz));
    std::vector<float> in(kFftSize);
    std::vector<float> product(kFftSize);
    for (size_t i = 0; i < kFftSize; ++i)
    {
        in[i] = static_cast<float>(0.5 * std::sin(kTwoPi * kInputHz * static_cast<double>(i) / 48000.0));
    }
    sut.processBlock(in, product);
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(product, magnitude);
    double strongest{0.0};
    for (const float m : magnitude)
    {
        strongest = std::max(strongest, static_cast<double>(m));
    }
    out << "@New plot: title=\"Spectrum of a " << kInputHz << " Hz sine times a sine carrier of " << kCarrierHz
        << " Hz (dB re the strongest line)\" miny=-130 maxy=5\n#product\n";
    double floorDb{-200.0};
    for (size_t bin = 1; bin < magnitude.size(); ++bin)
    {
        const double hz = static_cast<double>(bin) * 48000.0 / static_cast<double>(kFftSize);
        if (hz > 3000.0)
        {
            break;
        }
        const double db = toDb(static_cast<double>(magnitude[bin]) / strongest);
        out << hz << " " << db << "\n";
        const bool nearLine =
            std::abs(hz - (kInputHz - kCarrierHz)) < 30.0 || std::abs(hz - (kInputHz + kCarrierHz)) < 30.0;
        if (!nearLine)
        {
            floorDb = std::max(floorDb, db);
        }
    }
    std::printf("sidebands: highest bin more than 30 Hz from the two lines %.1f dB re the strongest line\n", floorDb);
}

// ---- Harmonics of the carrier against theory ----

constexpr size_t kHarmonicCycles = 20;
constexpr size_t kHarmonicPeriod = 480;

[[nodiscard]] std::vector<float> carrierCycles(const float morph)
{
    AbacDsp::WaveTableRingModulator sut{kSampleRate};
    sut.setFrequency(kSampleRate / static_cast<float>(kHarmonicPeriod));
    sut.setMorph(morph);
    return carrierOf(sut, kHarmonicPeriod * kHarmonicCycles);
}

void writeHarmonics(std::ofstream& out)
{
    constexpr size_t kLast = 20;
    const auto measured = [&](const float morph)
    {
        const auto carrier = carrierCycles(morph);
        std::vector<double> db(kLast + 1, -200.0);
        for (size_t k = 1; k <= kLast; ++k)
        {
            db[k] =
                toDb(harmonicAmplitude(carrier, kHarmonicCycles, k) / harmonicAmplitude(carrier, kHarmonicCycles, 1));
        }
        return db;
    };
    const auto& fin = AbacDsp::WaveTableStore::getTableSet(AbacDsp::BasicWave::SharkFin).tables[0].data;
    const std::vector<float> finCycle(fin.begin(), fin.begin() + AbacDsp::WaveTableSize);
    const auto finDb = [&](const size_t k)
    { return toDb(harmonicAmplitude(finCycle, 1, k) / harmonicAmplitude(finCycle, 1, 1)); };
    out << "@New plot: title=\"Carrier harmonics at 100 Hz (dB re the fundamental)\" miny=-100 maxy=3\n";
    double worstSquare{0.0};
    double worstFin{0.0};
    for (const float morph : {0.f, 0.5f, 1.f})
    {
        const auto db = measured(morph);
        out << "#morph " << morph << (morph == 0.f ? " (sine)" : morph == 0.5f ? " (square)" : " (shark fin)") << "\n";
        for (size_t k = 1; k <= kLast; ++k)
        {
            out << k << " " << std::max(db[k], -100.0) << "\n";
            if (morph == 0.5f && k % 2 == 1)
            {
                worstSquare = std::max(worstSquare, std::abs(db[k] - toDb(1.0 / static_cast<double>(k))));
            }
            if (morph == 1.f)
            {
                worstFin = std::max(worstFin, std::abs(db[k] - finDb(k)));
            }
        }
    }
    out << "#square, ideal 1/k\n";
    for (size_t k = 1; k <= kLast; k += 2)
    {
        out << k << " " << toDb(1.0 / static_cast<double>(k)) << "\n";
    }
    out << "#shark fin, from its table\n";
    for (size_t k = 1; k <= kLast; ++k)
    {
        out << k << " " << finDb(k) << "\n";
    }
    std::printf("harmonics: largest deviation, square (odd) from 1/k %.3f dB, shark fin from its table %.3f dB\n",
                worstSquare, worstFin);
}

/// The first four harmonics against the morph, each in dB re the carrier RMS: where a harmonic is weak or missing.
void writeMorphHarmonics(std::ofstream& out)
{
    out << "@New plot: title=\"Level of the first harmonics against the morph, 100 Hz (dB re the carrier RMS)\" "
           "miny=-60 "
           "maxy=6\n";
    std::array<std::vector<double>, 4> levels;
    std::vector<float> morphs;
    for (int step = 0; step <= 100; ++step)
    {
        morphs.push_back(static_cast<float>(step) / 100.f);
        const auto carrier = carrierCycles(morphs.back());
        for (size_t k = 1; k <= levels.size(); ++k)
        {
            const double rmsOfHarmonic = harmonicAmplitude(carrier, kHarmonicCycles, k) / std::numbers::sqrt2;
            levels[k - 1].push_back(toDb(rmsOfHarmonic / static_cast<double>(kCarrierRms)));
        }
    }
    double weakestFundamental{0.0};
    for (size_t k = 1; k <= levels.size(); ++k)
    {
        out << "#harmonic " << k << "\n";
        for (size_t i = 0; i < morphs.size(); ++i)
        {
            out << morphs[i] << " " << std::max(levels[k - 1][i], -60.0) << "\n";
        }
    }
    for (const double db : levels[0])
    {
        weakestFundamental = std::min(weakestFundamental, db);
    }
    std::printf("morph harmonics: weakest fundamental over the morph %.2f dB re the carrier RMS\n", weakestFundamental);
}

// ---- Aliasing against the carrier frequency ----

struct Partial
{
    double hz;
    double amplitude;
};

/// Largest component more than 40 bins from every line |f +/- h fc| inside the band, in dB re the strongest line.
[[nodiscard]] double aliasedDb(const std::vector<Partial>& input, const double carrierHz, const float inputCutoff,
                               const bool oddCarrierOnly)
{
    AbacDsp::WaveTableRingModulator sut{kSampleRate};
    sut.setWaveset(1, AbacDsp::BasicWave::Square);
    sut.setMorph(0.5f);
    sut.setFrequency(static_cast<float>(carrierHz));
    sut.setInputCutoff(inputCutoff);
    constexpr size_t kWarmUp = 4800;
    std::vector<float> product(kFftSize);
    for (size_t i = 0; i < kWarmUp + kFftSize; ++i)
    {
        double x{0.0};
        for (const Partial& p : input)
        {
            x += p.amplitude * std::sin(kTwoPi * p.hz * static_cast<double>(i) / 48000.0);
        }
        const float y = sut.step(static_cast<float>(x));
        if (i >= kWarmUp)
        {
            product[i - kWarmUp] = y;
        }
    }
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(product, magnitude);
    const double binHz = 48000.0 / static_cast<double>(kFftSize);
    std::vector<bool> isLine(magnitude.size(), false);
    const auto markLine = [&](const double hz)
    {
        const double bin = std::abs(hz) / binHz;
        if (bin >= static_cast<double>(magnitude.size()))
        {
            return;
        }
        const auto lo = static_cast<size_t>(std::max(0.0, bin - 40.0));
        const auto hi = std::min(magnitude.size() - 1, static_cast<size_t>(bin + 40.0));
        for (size_t b = lo; b <= hi; ++b)
        {
            isLine[b] = true;
        }
    };
    const auto harmonicLimit = static_cast<size_t>(0.3 * 48000.0 / carrierHz);
    for (const Partial& p : input)
    {
        for (size_t h = 0; h <= std::max<size_t>(harmonicLimit, 1); ++h)
        {
            if (oddCarrierOnly && h % 2 == 0)
            {
                continue;
            }
            markLine(p.hz + static_cast<double>(h) * carrierHz);
            markLine(p.hz - static_cast<double>(h) * carrierHz);
        }
    }
    double strongest{0.0};
    double aliased{0.0};
    for (size_t b = 3; b < magnitude.size(); ++b)
    {
        (isLine[b] ? strongest : aliased) =
            std::max(isLine[b] ? strongest : aliased, static_cast<double>(magnitude[b]));
    }
    return toDb(aliased / strongest);
}

constexpr double kFirstCarrierHz = 101.3;
constexpr double kLastCarrierHz = 11900.0;

/// A carrier whose period is nearly a whole number of samples lets the folded components land on genuine sidebands, and
/// the measurement then reports nothing; such a frequency is lowered until the period has a clear fraction.
[[nodiscard]] double awayFromWholePeriods(double hz)
{
    const auto fractionOfPeriod = [](const double frequency)
    {
        const double period = 48000.0 / frequency;
        return period - std::floor(period);
    };
    while (fractionOfPeriod(hz) < 0.1 || fractionOfPeriod(hz) > 0.9)
    {
        hz *= 0.995;
    }
    return hz;
}

void writeAliasing(std::ofstream& out)
{
    struct Case
    {
        const char* name;
        std::vector<Partial> input;
        float cutoff;
    };
    const float open = AbacDsp::Chebyshev4Lowpass::kOpenCutoff;
    const std::vector<Case> cases{
        {"input 9600 Hz (0.2 fs), lowpass open", {{9600.0, 0.5}}, open},
        {"440 Hz and 19200 Hz (0.4 fs), lowpass open", {{440.0, 0.5}, {19200.0, 0.25}}, open},
        {"440 Hz and 19200 Hz (0.4 fs), lowpass at 9600 Hz", {{440.0, 0.5}, {19200.0, 0.25}}, 9600.f},
    };
    out << "@New plot: title=\"Aliased components against the strongest sideband, square carrier (dB)\" logx=true "
           "hzticks=true miny=-120 maxy=5\n";
    std::vector<double> worst(cases.size(), -200.0);
    for (size_t c = 0; c < cases.size(); ++c)
    {
        out << "#" << cases[c].name << "\n";
        for (int i = 0; i <= 30; ++i)
        {
            const double hz = awayFromWholePeriods(
                kFirstCarrierHz * std::pow(kLastCarrierHz / kFirstCarrierHz, static_cast<double>(i) / 30.0));
            const double db = aliasedDb(cases[c].input, hz, cases[c].cutoff, true);
            worst[c] = std::max(worst[c], db);
            out << hz << " " << db << "\n";
        }
    }
    for (size_t c = 0; c < cases.size(); ++c)
    {
        std::printf("aliasing: %s: worst %.1f dB\n", cases[c].name, worst[c]);
    }
}

// ---- Response of the input lowpass ----

[[nodiscard]] double sectionMagnitude(const double cf, const AbacDsp::BiquadSection& c)
{
    const std::complex<double> z1 = std::polar(1.0, -kTwoPi * cf);
    const std::complex<double> z2 = z1 * z1;
    const auto num = static_cast<double>(c.b0) + static_cast<double>(c.b1) * z1 + static_cast<double>(c.b2) * z2;
    const auto den = 1.0 + static_cast<double>(c.a1) * z1 + static_cast<double>(c.a2) * z2;
    return std::abs(num / den);
}

void writePrefilter(std::ofstream& out)
{
    out << "@New plot: title=\"Input lowpass, 1 dB ripple (dB)\" logx=true hzticks=true miny=-90 maxy=3\n";
    for (const float cutoff : {1000.f, 5000.f, 9600.f})
    {
        AbacDsp::Chebyshev4Lowpass filter{kSampleRate};
        filter.set(cutoff, 1.f, true);
        out << "#cutoff " << cutoff << " Hz\n";
        for (int i = 0; i <= 200; ++i)
        {
            const double hz = 20.0 * std::pow(23000.0 / 20.0, static_cast<double>(i) / 200.0);
            const double cf = hz / 48000.0;
            double db{0.0};
            for (size_t s = 0; s < 2; ++s)
            {
                db += toDb(sectionMagnitude(cf, filter.section(s).getCurrent()));
            }
            out << hz << " " << db << "\n";
        }
    }
}

// ---- Against the sine ring modulator ----

void printAgainstRingModulator()
{
    AbacDsp::WaveTableRingModulator sut{kSampleRate};
    AbacDsp::RingModulator reference{kSampleRate};
    sut.setFrequency(1234.5f);
    reference.setFrequency(1234.5f);
    sut.resetPhase();
    double worst{0.0};
    constexpr float kSineRms = 0.70710678f;
    for (size_t i = 0; i < 48000; ++i)
    {
        const float in = 0.5f * std::sin(static_cast<float>(kTwoPi) * 317.f * static_cast<float>(i) / kSampleRate);
        const float expected = reference.step(in) * kCarrierRms / kSineRms;
        worst = std::max(worst, static_cast<double>(std::abs(expected - sut.step(in))));
    }
    std::printf("against RingModulator: largest difference over one second, scaled by 0.57/0.7071: %.2e\n", worst);
}
} // namespace

int main(const int argc, char** argv)
{
    const std::array<std::string, 6> names{"wr_level.txt",           "wr_sidebands.txt", "wr_harmonics.txt",
                                           "wr_morph_harmonics.txt", "wr_aliasing.txt",  "wr_prefilter.txt"};
    std::array<std::ofstream, 6> files;
    for (size_t i = 0; i < files.size(); ++i)
    {
        files[i].open(argc > static_cast<int>(i) + 1 ? argv[i + 1] : names[i]);
    }
    writeLevel(files[0]);
    writeSidebands(files[1]);
    writeHarmonics(files[2]);
    writeMorphHarmonics(files[3]);
    writeAliasing(files[4]);
    writePrefilter(files[5]);
    printAgainstRingModulator();
    return 0;
}
