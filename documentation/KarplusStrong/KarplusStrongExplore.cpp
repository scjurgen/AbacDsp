// Generates verification/demonstration plots for the Karplus-Strong string model
// (src/includes/Generators/KarplusStrongString.h, KarplusStrongVoice.h): spectral
// brightness loss over decay and across PluckType noise colors, empirical vs. theoretical
// decay time, pitch behavior across a bendInCents slide and a live setDamper() change (the
// PalmMute pitch-compensation fix), and one envelope timeline per excitation technique.
// Output is plain text in documentation/Plot/PyConPlot.py's "@New plot:"/"#group" format;
// see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Generators/ExcitationTechnique.h"
#include "Generators/KarplusStrongVoice.h"
#include "Numbers/Convert.h"

namespace
{
constexpr float kSampleRate = 48000.f;
using TestString = AbacDsp::KarplusStrongString<10000>;
using TestVoice = AbacDsp::KarplusStrongVoice<10000>;

// ---- Spectral behavior ----

constexpr size_t kFftSize = 4096;
constexpr size_t kSpectralBinLimit = kFftSize / 8; // up to sampleRate/16 - plenty for a plucked string

void writeMagnitudeSpectrum(std::ofstream& out, const std::vector<float>& window)
{
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(window, magnitude);
    for (size_t bin = 1; bin < kSpectralBinLimit; ++bin)
    {
        const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
        const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
        out << hz << " " << db << "\n";
    }
}

void writeSpectralBehavior(std::ofstream& out)
{
    constexpr std::array<float, 3> snapshotSeconds{0.02f, 0.5f, 2.f};

    out << "@New plot: title=\"brightness loss over decay (note 60, damper=0.5)\"\n";
    TestString ringing(kSampleRate);
    ringing.setPluckType(AbacDsp::PluckType::WhiteStatic);
    ringing.setDamper(0.5f);
    ringing.setDecayByTime(4000.f);
    ringing.trigger(60.f, 1.f);
    std::vector<float> rendered(static_cast<size_t>(3.f * kSampleRate));
    std::ranges::generate(rendered, [&ringing] { return ringing.step(); });
    for (const float t : snapshotSeconds)
    {
        const auto start = static_cast<size_t>(t * kSampleRate);
        if (start + kFftSize > rendered.size())
        {
            continue;
        }
        out << "#t=" << t << "s\n";
        const std::vector<float> window(rendered.begin() + static_cast<std::ptrdiff_t>(start),
                                        rendered.begin() + static_cast<std::ptrdiff_t>(start + kFftSize));
        writeMagnitudeSpectrum(out, window);
    }

    out << "@New plot: title=\"initial-burst color by PluckType (note 60)\"\n";
    const std::array<std::pair<AbacDsp::PluckType, const char*>, 3> pluckTypes{
        {{AbacDsp::PluckType::WhiteStatic, "White"},
         {AbacDsp::PluckType::Pink, "Pink"},
         {AbacDsp::PluckType::Brown, "Brown"}}};
    for (const auto& [type, name] : pluckTypes)
    {
        TestString s(kSampleRate);
        s.setPluckType(type);
        s.setDamperCutoff(24000.f); // bypass the damper: isolate the burst's own color
        s.trigger(60.f, 1.f);
        std::vector<float> burst(kFftSize + 200);
        std::ranges::generate(burst, [&s] { return s.step(); });
        out << "#" << name << "\n";
        const std::vector<float> window(burst.begin() + 100,
                                        burst.begin() + 100 + static_cast<std::ptrdiff_t>(kFftSize));
        writeMagnitudeSpectrum(out, window);
    }
}

// ---- Decay verification ----

constexpr std::array<float, 3> kDecayNotes{48.f, 60.f, 72.f};
constexpr float kDecayMs = 2000.f;
constexpr float kOctaveFactor = 1.f;
constexpr size_t kDecayRmsWindow = 500;

void writeDecayVerification(std::ofstream& out)
{
    for (const float note : kDecayNotes)
    {
        TestString s(kSampleRate);
        s.setPluckType(AbacDsp::PluckType::WhiteStatic);
        s.setDecayByTime(kDecayMs);
        s.setDecayOctaveFactor(kOctaveFactor);
        s.trigger(note, 1.f);
        s.setDamperCutoff(24000.f); // bypass the damper: isolate decayGain's own contribution

        const auto totalSamples = static_cast<size_t>(kDecayMs * 0.001f * kSampleRate * 1.5f);
        std::vector<float> rendered(totalSamples);
        std::ranges::generate(rendered, [&s] { return s.step(); });

        float referencePeak = 0.f;
        for (size_t i = 0; i < kDecayRmsWindow; ++i)
        {
            referencePeak = std::max(referencePeak, std::abs(rendered[i]));
        }

        out << "@New plot: title=\"note " << note << "\"\n#empirical\n";
        for (size_t start = 0; start + kDecayRmsWindow <= rendered.size(); start += kDecayRmsWindow)
        {
            float peak = 0.f;
            for (size_t i = start; i < start + kDecayRmsWindow; ++i)
            {
                peak = std::max(peak, std::abs(rendered[i]));
            }
            const float t = static_cast<float>(start) / kSampleRate;
            const float db = 20.f * std::log10(std::max(peak, 1e-9f) / referencePeak);
            out << t << " " << db << "\n";
        }

        const float octavesFromReference = (note - 60.f) / 12.f;
        const float effectiveDecayMs = kDecayMs * std::exp2(-kOctaveFactor * octavesFromReference);
        const float totalSeconds = static_cast<float>(totalSamples) / kSampleRate;
        out << "#theoretical -20dB per decayTime\n";
        out << 0.f << " " << 0.f << "\n";
        out << totalSeconds << " " << (-20.f * totalSeconds / (effectiveDecayMs * 0.001f)) << "\n";
    }
}

// ---- Bend / pitch-compensation behavior ----

constexpr size_t kPitchWindowSamples = 2000;
// Narrow +/-10% radius (a wider one risks locking onto a harmonic/sub-harmonic lag); not
// a zero-crossing count, which conflates harmonic content with the fundamental here.
constexpr float kPitchSearchFraction = 0.1f;

// Self-seeded from the previous window's own result (see writePitchTrack()), the same idea
// as KarplusStrongString_test.cpp's measureFrequencyByAutocorrelation() but tracking pitch
// continuously through a bend instead of checking one fixed target.
[[nodiscard]] float autocorrelationPitchHz(const std::vector<float>& signal, const size_t start, const size_t windowLen,
                                           const float expectedHz)
{
    const float expectedPeriod = kSampleRate / expectedHz;
    const auto baseLag = static_cast<size_t>(std::lround(expectedPeriod));
    const auto searchRadius =
        std::max<size_t>(4, static_cast<size_t>(std::lround(kPitchSearchFraction * expectedPeriod)));
    if (baseLag <= searchRadius || start + windowLen + baseLag + searchRadius >= signal.size())
    {
        return expectedHz;
    }

    const auto correlationAt = [&](const size_t lag) noexcept
    {
        float dot = 0.f;
        float refEnergy = 0.f;
        float cmpEnergy = 0.f;
        for (size_t i = 0; i < windowLen; ++i)
        {
            const float a = signal[start + i];
            const float b = signal[start + lag + i];
            dot += a * b;
            refEnergy += a * a;
            cmpEnergy += b * b;
        }
        return dot / std::sqrt(refEnergy * cmpEnergy + 1e-12f);
    };

    size_t bestLag = baseLag - searchRadius;
    float bestCorrelation = correlationAt(bestLag);
    for (size_t lag = baseLag - searchRadius + 1; lag <= baseLag + searchRadius; ++lag)
    {
        const float correlation = correlationAt(lag);
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }
    if (bestCorrelation < 0.5f)
    {
        return expectedHz; // weak match (e.g. decayed to near-silence): hold the last value
    }

    const float y1 = correlationAt(bestLag - 1);
    const float y2 = bestCorrelation;
    const float y3 = correlationAt(bestLag + 1);
    const float denom = y1 - 2.f * y2 + y3;
    const float offset = std::abs(denom) < 1e-9f ? 0.f : 0.5f * (y1 - y3) / denom;
    return kSampleRate / (static_cast<float>(bestLag) + offset);
}

// Tracks only within [rangeStart, rangeEnd), self-seeded from initialExpectedHz - callers
// that know a bend happens partway through render two ranges with different seeds (see
// writeBendBehavior()) instead of relying on the search radius to leap the jump itself.
void writePitchTrackRange(std::ofstream& out, const std::vector<float>& rendered, const float initialExpectedHz,
                          const size_t rangeStart, const size_t rangeEnd)
{
    float expected = initialExpectedHz;
    for (size_t start = rangeStart; start + kPitchWindowSamples <= rangeEnd; start += kPitchWindowSamples)
    {
        expected = autocorrelationPitchHz(rendered, start, kPitchWindowSamples, expected);
        out << (static_cast<float>(start) / kSampleRate) << " " << expected << "\n";
    }
}

void writePitchTrack(std::ofstream& out, const std::vector<float>& rendered, const float initialExpectedHz)
{
    writePitchTrackRange(out, rendered, initialExpectedHz, 0, rendered.size());
}

void writeBendBehavior(std::ofstream& out)
{
    constexpr auto changeAtSample = static_cast<size_t>(1.f * kSampleRate);
    constexpr auto totalSamples = static_cast<size_t>(3.f * kSampleRate);

    out << "@New plot: title=\"bendInCents(+1200) at t=1s (note 60)\"\n#tracked pitch\n";
    TestString bendString(kSampleRate);
    bendString.setPluckType(AbacDsp::PluckType::WhiteStatic);
    bendString.setDamper(0.f);
    bendString.setDecayByTime(4000.f);
    bendString.trigger(60.f, 1.f);
    std::vector<float> bendRendered(totalSamples);
    for (size_t i = 0; i < totalSamples; ++i)
    {
        if (i == changeAtSample)
        {
            bendString.bendInCents(1200.f);
        }
        bendRendered[i] = bendString.step();
    }
    // Seeded separately either side of the bend at the two known target frequencies (+1200
    // cents = exactly 2x), rather than asking the search radius to leap an octave on its own.
    writePitchTrackRange(out, bendRendered, Convert::noteToFrequency(60.f), 0, changeAtSample);
    writePitchTrackRange(out, bendRendered, Convert::noteToFrequency(60.f) * 2.f, changeAtSample, totalSamples);

    out << "@New plot: title=\"live setDamper() 0.2 to 0.8 at t=1s (note 60) - pitch should stay flat\"\n#tracked "
           "pitch\n";
    TestString damperString(kSampleRate);
    damperString.setPluckType(AbacDsp::PluckType::WhiteStatic);
    damperString.setDecayByTime(4000.f);
    damperString.setDamper(0.2f);
    damperString.trigger(60.f, 1.f);
    std::vector<float> damperRendered(totalSamples);
    for (size_t i = 0; i < totalSamples; ++i)
    {
        if (i == changeAtSample)
        {
            damperString.setDamper(0.8f);
        }
        damperRendered[i] = damperString.step();
    }
    writePitchTrack(out, damperRendered, Convert::noteToFrequency(60.f));
}

// ---- Excitation technique envelopes ----

constexpr size_t kEnvelopeWindow = 200;
constexpr float kEnvelopeSeconds = 2.f;
constexpr float kPrePluckSeconds = 0.3f; // for techniques that act on an already-ringing string

// Output saturates well below unity gain (measured ceiling ~0.25 peak); Pluck/Strike reuse
// this as their own excitation strength too, so their burst isn't dwarfed by the pre-roll.
constexpr float kDemoTriggerGain = 20.f;

struct TechniqueDemo
{
    const char* name;
    AbacDsp::ExcitationEvent event;
    bool prePluck;
};

void writeExcitationTechniques(std::ofstream& out)
{
    const std::array<TechniqueDemo, 8> demos{{
        {"pluck", {0.f, 0.f, AbacDsp::ExcitationType::Pluck, kDemoTriggerGain, {}}, true},
        {"strike", {0.f, 0.f, AbacDsp::ExcitationType::Strike, kDemoTriggerGain, {}}, true},
        {"mute", {0.f, 20.f, AbacDsp::ExcitationType::Mute, 0.f, {}}, true},
        {"palmmute", {0.f, 1500.f, AbacDsp::ExcitationType::PalmMute, 1.f, {}}, true},
        {"bow", {0.f, 1500.f, AbacDsp::ExcitationType::Bow, 0.4f, {}}, true},
        {"sympathetic", {0.f, 1500.f, AbacDsp::ExcitationType::Sympathetic, 0.7f, 2.f}, true},
        {"wind", {0.f, 1500.f, AbacDsp::ExcitationType::Wind, 0.4f, {}}, true},
        {"rub", {0.f, 1500.f, AbacDsp::ExcitationType::Rub, 0.4f, {}}, true},
    }};

    for (const auto& demo : demos)
    {
        TestVoice voice(kSampleRate);
        voice.setPluckType(AbacDsp::PluckType::WhiteStatic);
        out << "@New plot: title=\"" << demo.name << "\"\n#envelope\n";

        float peak = 0.f;
        size_t sampleIndex = 0;
        const auto flushWindow = [&](const size_t i)
        {
            if ((i + 1) % kEnvelopeWindow == 0)
            {
                // Floored before the log: an exact-zero peak (e.g. Mute once fully silent)
                // would otherwise send this to -inf and blow out every subplot's shared scale.
                out << (static_cast<float>(i) / kSampleRate) << " " << std::log(std::max(peak, 1e-6f)) * 20 << "\n";
                peak = 0.f;
            }
        };

        if (demo.prePluck)
        {
            voice.trigger(60.f, kDemoTriggerGain);
            const auto preRollSamples = static_cast<size_t>(kPrePluckSeconds * kSampleRate);
            for (; sampleIndex < preRollSamples; ++sampleIndex)
            {
                peak = std::max(peak, std::abs(voice.step()));
                flushWindow(sampleIndex);
            }
        }
        voice.scheduleExcitation(demo.event);

        const auto totalSamples = static_cast<size_t>(kEnvelopeSeconds * kSampleRate);
        for (; sampleIndex < totalSamples; ++sampleIndex)
        {
            peak = std::max(peak, std::abs(voice.step()));
            flushWindow(sampleIndex);
        }
    }
}
}

int main(int argc, char* argv[])
{
    const std::string spectralPath = argc > 1 ? argv[1] : "ks_spectral.txt";
    const std::string decayPath = argc > 2 ? argv[2] : "ks_decay.txt";
    const std::string bendPath = argc > 3 ? argv[3] : "ks_bend.txt";
    const std::string excitationPath = argc > 4 ? argv[4] : "ks_excitation.txt";

    std::ofstream spectralOut(spectralPath);
    std::ofstream decayOut(decayPath);
    std::ofstream bendOut(bendPath);
    std::ofstream excitationOut(excitationPath);
    if (!spectralOut || !decayOut || !bendOut || !excitationOut)
    {
        std::cerr << "KarplusStrongExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeSpectralBehavior(spectralOut);
    writeDecayVerification(decayOut);
    writeBendBehavior(bendOut);
    writeExcitationTechniques(excitationOut);

    std::cout << "KarplusStrongExplore: wrote " << spectralPath << ", " << decayPath << ", " << bendPath << ", and "
              << excitationPath << std::endl;
}
