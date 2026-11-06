// Verification plots for src/includes/Analysis/MelColumns.h: the level of a tone against its frequency, the level
// scale against the amplitude, and the level of white noise per band against the noise expected from the triangles.
// PyConPlot.py output; see README.md.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <numbers>
#include <thread>
#include <vector>

#include "Analysis/MelColumns.h"

namespace
{
constexpr float kSampleRate = 48000.f;

/// All columns the signal produces over `columns` frames, oldest first; the worker is given time to keep up.
std::vector<float> runSignal(AbacDsp::MelColumns& mel, const std::function<float(size_t)>& signal, const size_t columns)
{
    const size_t hop = mel.fftLength() / 4;
    std::vector<float> collected;
    std::vector<float> buffer(static_cast<size_t>(mel.bands()) * mel.columns());
    size_t index{0};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (collected.size() < columns * mel.bands() && std::chrono::steady_clock::now() < deadline)
    {
        std::vector<float> block(hop);
        for (auto& sample : block)
        {
            sample = signal(index++);
        }
        mel.processBlock(block);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        const size_t got = mel.pull(buffer);
        collected.insert(collected.end(), buffer.begin(),
                         buffer.begin() + static_cast<std::ptrdiff_t>(got * mel.bands()));
    }
    return collected;
}

float sineAt(const size_t n, const double hz, const double amplitude)
{
    return static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(n) / kSampleRate));
}

double toDb(const double gain)
{
    return 20.0 * std::log10(std::max(std::abs(gain), 1e-12));
}

/// 0..1 of a column value back to dB with the default scale.
double valueToDb(const double value)
{
    const AbacDsp::MelColumns::Settings settings;
    return settings.floorDb + value * (settings.ceilDb - settings.floorDb);
}

// ---- Level of a tone against its frequency ----

void writeTone(std::ofstream& out)
{
    out << "@New plot: title=\"Loudest band for a full-scale sine (dB re full scale)\" miny=-8 maxy=1\n#loudest band\n";
    std::vector<std::pair<double, double>> centreError;
    double worst{0.0};
    for (int i = 0; i <= 120; ++i)
    {
        const double hz = 50.0 * std::pow(18000.0 / 50.0, i / 120.0);
        AbacDsp::MelColumns mel{kSampleRate};
        const auto columns = runSignal(mel, [hz](const size_t n) { return sineAt(n, hz, 1.0); }, 10);
        const auto last = columns.end() - static_cast<std::ptrdiff_t>(mel.bands());
        const auto peak = std::max_element(last, columns.end());
        const double db = valueToDb(*peak);
        out << hz << " " << db << "\n";
        worst = std::min(worst, db);
        const auto band = static_cast<unsigned>(peak - last);
        centreError.emplace_back(hz, 100.0 * (static_cast<double>(mel.bandCentreHz(band)) / hz - 1.0));
    }
    out << "@New plot: title=\"Centre of the loudest band against the frequency of the sine (% error)\" miny=-15 "
           "maxy=15\n"
           "#centre error\n";
    for (const auto& [hz, error] : centreError)
    {
        out << hz << " " << error << "\n";
    }
    std::printf("tone: lowest peak level over 50 Hz to 18 kHz: %.2f dB\n", worst);
}

// ---- Level scale ----

void writeLinearity(std::ofstream& out)
{
    out << "@New plot: title=\"Loudest band for a 1 kHz sine against its level (dB)\" miny=-100 maxy=5\n";
    std::vector<std::pair<double, double>> measured;
    double worst{0.0};
    for (int level = -100; level <= 0; level += 5)
    {
        AbacDsp::MelColumns mel{kSampleRate};
        const double amplitude = std::pow(10.0, level / 20.0);
        const auto columns = runSignal(mel, [amplitude](const size_t n) { return sineAt(n, 1000.0, amplitude); }, 10);
        const auto last = columns.end() - static_cast<std::ptrdiff_t>(mel.bands());
        const double db = valueToDb(*std::max_element(last, columns.end()));
        measured.emplace_back(level, db);
        if (level > -80)
        {
            worst = std::max(worst, std::abs(db - level));
        }
    }
    out << "#measured\n";
    for (const auto& [level, db] : measured)
    {
        out << level << " " << db << "\n";
    }
    out << "#ideal, clamped at the floor of -90 dB\n";
    for (const auto& [level, db] : measured)
    {
        out << level << " " << std::max<double>(level, -90.0) << "\n";
    }
    std::printf("linearity: largest deviation from the ideal from -80 to 0 dB: %.2f dB\n", worst);
}

// ---- White noise per band ----

void writeNoise(std::ofstream& out)
{
    AbacDsp::MelColumns mel{kSampleRate};
    uint32_t state{987654321};
    const auto noise = [&state](size_t)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 23) - 1.f;
    };
    const size_t count = 400;
    const auto columns = runSignal(mel, noise, count);
    const size_t have = columns.size() / mel.bands();
    std::vector<double> meanDb(mel.bands(), 0.0);
    for (size_t c = 0; c < have; ++c)
    {
        for (unsigned b = 0; b < mel.bands(); ++b)
        {
            meanDb[b] += valueToDb(columns[c * mel.bands() + b]) / static_cast<double>(have);
        }
    }
    // Power per bin of Hann-windowed noise of variance 1/3: sigma^2 * 3 / (8 N); a band sums the bins under its
    // triangle, whose area in bins is (high - low) / (2 binHz), at least one bin for a narrow band.
    const double binHz = kSampleRate / static_cast<double>(mel.fftLength());
    out << "@New plot: title=\"Uniform white noise, mean level per band (dB re full scale)\" miny=-50 "
           "maxy=-10\n#measured\n";
    for (unsigned b = 0; b < mel.bands(); ++b)
    {
        out << mel.bandCentreHz(b) << " " << meanDb[b] << "\n";
    }
    out << "#expected from the triangles\n";
    double worst{0.0};
    for (unsigned b = 1; b + 1 < mel.bands(); ++b)
    {
        const double width = static_cast<double>(mel.bandCentreHz(b + 1) - mel.bandCentreHz(b - 1));
        const double bins = std::max(1.0, width / (2.0 * binHz));
        const double power = (1.0 / 3.0) * 3.0 / (8.0 * mel.fftLength()) * bins;
        const double db = 10.0 * std::log10(16.0 / 1.5 * power);
        out << mel.bandCentreHz(b) << " " << db << "\n";
        if (width / (2.0 * binHz) > 8.0)
        {
            worst = std::max(worst, std::abs(db - meanDb[b]));
        }
    }
    std::printf("noise: %zu columns, largest deviation from the triangle area for bands wider than 8 bins: %.2f dB\n",
                have, worst);
}
} // namespace

int main(const int argc, char** argv)
{
    const std::array<std::string, 3> names{"mc_tone.txt", "mc_linearity.txt", "mc_noise.txt"};
    std::array<std::ofstream, 3> files;
    for (size_t i = 0; i < files.size(); ++i)
    {
        files[i].open(argc > static_cast<int>(i) + 1 ? argv[i + 1] : names[i]);
    }
    writeTone(files[0]);
    writeLinearity(files[1]);
    writeNoise(files[2]);
    return 0;
}
