// Verification plots for src/includes/Filters/OnePoleFilter.h: magnitude response across all
// four characteristics, setDecayTime()'s exact timing, and AllPass's flat-magnitude/90-degree-
// at-cutoff phase behavior. Plain-text PyConPlot.py output; see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include "Filters/OnePoleFilter.h"

namespace
{
constexpr float kSampleRate = 48000.f;

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

// ---- Magnitude response across characteristics ----

template <AbacDsp::OnePoleFilterCharacteristic Characteristic>
void writeResponse(std::ofstream& out, const char* name, const float cutoffHz, const std::vector<float>& sweep)
{
    AbacDsp::OnePoleFilter<Characteristic> filter(kSampleRate, cutoffHz);
    out << "#" << name << "\n";
    for (const float hz : sweep)
    {
        const float mag = filter.magnitude(hz);
        out << hz << " " << (20.f * std::log10(std::max(mag, 1e-9f))) << "\n";
    }
}

void writeResponses(std::ofstream& out)
{
    out << "@New plot: title=\"OnePoleFilter: magnitude response, all four characteristics "
           "(cutoff=1kHz)\" logx=true\n";
    const auto sweep = logSweepHz(20.f, 24000.f, 500);
    constexpr float cutoffHz = 1000.f;
    writeResponse<AbacDsp::OnePoleFilterCharacteristic::LowPass>(out, "LowPass", cutoffHz, sweep);
    writeResponse<AbacDsp::OnePoleFilterCharacteristic::HighPass>(out, "HighPass", cutoffHz, sweep);
    writeResponse<AbacDsp::OnePoleFilterCharacteristic::HighPassLeaky>(out, "HighPassLeaky", cutoffHz, sweep);
    writeResponse<AbacDsp::OnePoleFilterCharacteristic::AllPass>(out, "AllPass", cutoffHz, sweep);
}

// ---- setDecayTime() exact timing ----

struct DecayConfig
{
    float fraction;
    float seconds;
};

void writeDecayTime(std::ofstream& out)
{
    out << "@New plot: title=\"OnePoleFilter LowPass: setDecayTime() measured vs. requested\"\n";
    constexpr std::array<DecayConfig, 3> configs{{{0.1f, 0.05f}, {0.01f, 0.1f}, {0.001f, 0.2f}}};
    constexpr auto totalSamples = static_cast<size_t>(0.3f * kSampleRate);

    for (const auto& cfg : configs)
    {
        AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass> filter(kSampleRate);
        filter.setDecayTime(cfg.seconds, cfg.fraction);

        std::vector<float> impulse(totalSamples, 0.f);
        impulse[0] = 1.f;
        std::vector<float> rendered(totalSamples);
        filter.processBlock(impulse.data(), rendered.data(), totalSamples);

        // setDecayTime()'s fraction is relative to the impulse response's own peak (h[0] =
        // 1-pole, tiny for a pole near 1), not to an absolute 0dB reference.
        const float peak0 = std::abs(rendered[0]);
        const float targetDb = 20.f * std::log10(cfg.fraction);
        size_t crossingSample = totalSamples;
        for (size_t n = 0; n < totalSamples; ++n)
        {
            if (std::abs(rendered[n]) <= cfg.fraction * peak0)
            {
                crossingSample = n;
                break;
            }
        }
        const float measuredSeconds = static_cast<float>(crossingSample) / kSampleRate;

        out << "#requested t=" << cfg.seconds << "s, fraction=" << cfg.fraction << ", measured=" << measuredSeconds
            << "s\n";
        for (size_t n = 0; n < totalSamples; ++n)
        {
            const float t = static_cast<float>(n) / kSampleRate;
            const float db = 20.f * std::log10(std::max(std::abs(rendered[n]) / peak0, 1e-9f));
            out << t << " " << db << "\n";
        }
        out << "#target level (" << cfg.fraction << ")\n0 " << targetDb << "\n"
            << (2.f * cfg.seconds) << " " << targetDb << "\n";
    }
}

// ---- AllPass: flat magnitude, phase reaches 90 degrees at cutoff ----

constexpr size_t kPhaseSettleSamples = 4000;
constexpr size_t kPhaseMeasureSamples = 4000;

[[nodiscard]] float measureAllPassPhaseDeg(const float testHz, const float cutoffHz)
{
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::AllPass> filter(kSampleRate, cutoffHz);

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

void writeAllPassPhase(std::ofstream& out)
{
    const auto sweep = logSweepHz(20.f, 20000.f, 300);
    constexpr float cutoffHz = 1000.f;

    out << "@New plot: title=\"OnePoleFilter AllPass: magnitude stays flat (cutoff=1kHz)\" "
           "logx=true\n#magnitude\n";
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::AllPass> filter(kSampleRate, cutoffHz);
    for (const float hz : sweep)
    {
        out << hz << " " << (20.f * std::log10(filter.magnitude(hz))) << "\n";
    }

    out << "@New plot: title=\"OnePoleFilter AllPass: phase reaches 90 degrees at cutoff\" "
           "logx=true\n#phase\n";
    for (const float hz : sweep)
    {
        out << hz << " " << measureAllPassPhaseDeg(hz, cutoffHz) << "\n";
    }
    out << "#90 degree reference\n" << sweep.front() << " 90\n" << sweep.back() << " 90\n";
}
}

int main(int argc, char* argv[])
{
    const std::string responsePath = argc > 1 ? argv[1] : "op_response.txt";
    const std::string decayPath = argc > 2 ? argv[2] : "op_decaytime.txt";
    const std::string phasePath = argc > 3 ? argv[3] : "op_allpass_phase.txt";

    std::ofstream responseOut(responsePath);
    std::ofstream decayOut(decayPath);
    std::ofstream phaseOut(phasePath);
    if (!responseOut || !decayOut || !phaseOut)
    {
        std::cerr << "OnePoleExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeResponses(responseOut);
    writeDecayTime(decayOut);
    writeAllPassPhase(phaseOut);

    std::cout << "OnePoleExplore: wrote " << responsePath << ", " << decayPath << ", and " << phasePath << std::endl;
}
