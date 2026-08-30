// Verification plots for AbacDsp::DiffuserDelayChain (src/includes/Diffuser/DiffusorDelayChain.h):
// Schroeder vs. Direct feedback-style spectral flatness, and echo-density growth (Abel & Huang
// 2006) across element counts. Plain-text PyConPlot.py output; see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Diffuser/DiffusorDelayChain.h"
#include "Helpers/ConstructArray.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kMaxDelaySamples = 24000; // matches examples/maxdiffuser's own bound
constexpr size_t kMaxElements = 20;
constexpr size_t kBlockSize = 480;

// Matches examples/maxdiffuser's own defaults (MaxDiffuserImpl.h), so these plots
// reflect the diffuser as it's actually configured, not an arbitrary test setting.
constexpr size_t kDefaultElements = 6;
constexpr float kFeedback = 0.65f;
constexpr float kBulge = 0.46f;
constexpr float kBottomSizeM = 0.7f;
constexpr float kTopSizeM = 7.f;

using SchroederChain =
    AbacDsp::DiffuserDelayChain<kMaxDelaySamples, kMaxElements, AbacDsp::AllpassFeedbackStyle::Schroeder>;
using DirectChain = AbacDsp::DiffuserDelayChain<kMaxDelaySamples, kMaxElements, AbacDsp::AllpassFeedbackStyle::Direct>;

// Feeds a unit impulse through chain and captures totalSamples of output, processed in
// kBlockSize chunks (the constructor's blkSize bound, which its internal scratch buffers
// are sized to).
template <typename Chain>
[[nodiscard]] std::vector<float> renderImpulseResponse(Chain& chain, const size_t totalSamples)
{
    std::vector<float> rendered(totalSamples, 0.f);
    std::vector<float> in(kBlockSize, 0.f);
    std::vector<float> out(kBlockSize, 0.f);
    in[0] = 1.f;
    size_t pos = 0;
    while (pos < totalSamples)
    {
        const auto n = std::min(kBlockSize, totalSamples - pos);
        chain.processBlock(in.data(), out.data(), n);
        std::copy_n(out.data(), n, rendered.data() + pos);
        std::fill_n(in.begin(), n, 0.f);
        pos += n;
    }
    return rendered;
}

// ---- Spectral flatness ----

constexpr size_t kFftSize = 16384; // high resolution: one-shot offline generation, not a hot path
// At feedback=0.5 the six-element chain decays roughly -20dB per 50ms (measured), so
// this sits well past the elements' fill-in time but well above the FFT noise floor.
constexpr float kSnapshotSeconds = 0.08f;

void writeMagnitudeSpectrum(std::ofstream& out, const std::vector<float>& window)
{
    AbacDsp::HannWindowMagnitudesFft fft(kFftSize);
    std::vector<float> magnitude(kFftSize / 2);
    fft.compute(window, magnitude);
    for (size_t bin = 1; bin < kFftSize / 2; ++bin)
    {
        const float hz = static_cast<float>(bin) * kSampleRate / static_cast<float>(kFftSize);
        const float db = 20.f * std::log10(std::max(magnitude[bin], 1e-9f));
        out << hz << " " << db << "\n";
    }
}

template <typename Chain>
void writeStyleSpectrum(std::ofstream& out, const char* name)
{
    Chain chain(kSampleRate, kBlockSize);
    chain.resetDiffuser(kDefaultElements, kFeedback, kBulge, kBottomSizeM, kTopSizeM, AbacDsp::skipSmoothing);
    chain.setDamper(20001.f); // bypass the in-loop damper (default 1000 Hz): isolate the
                              // allpass feedback topology's own magnitude response
    const auto totalSamples = static_cast<size_t>(1.f * kSampleRate);
    const auto rendered = renderImpulseResponse(chain, totalSamples);
    const auto start = static_cast<size_t>(kSnapshotSeconds * kSampleRate);
    const std::vector<float> window(rendered.begin() + static_cast<std::ptrdiff_t>(start),
                                    rendered.begin() + static_cast<std::ptrdiff_t>(start + kFftSize));
    out << "#" << name << "\n";
    writeMagnitudeSpectrum(out, window);
}

void writeSpectralComparison(std::ofstream& out)
{
    out << "@New plot: title=\"DiffuserDelayChain magnitude response: Schroeder vs. Direct feedback style "
           "(elements=6, feedback=0.65, damper bypassed)\"\n";
    writeStyleSpectrum<SchroederChain>(out, "Schroeder (flat)");
    writeStyleSpectrum<DirectChain>(out, "Direct (not flat)");
}

// ---- Echo density growth ----

constexpr size_t kNedWindow = 2400; // ~50ms: shorter windows are too noisy to read a trend from
constexpr size_t kNedHop = 120;     // fine time granularity; window overlap keeps it smooth
// erfc(1/sqrt(2)): the fraction of samples exceeding one std-dev in a true Gaussian,
// normalizing the raw fraction below to 1.0 once the tail is fully diffuse (Abel & Huang).
constexpr float kNedGaussianFraction = 0.317310508f;

[[nodiscard]] float normalizedEchoDensityAt(const std::vector<float>& signal, const size_t start,
                                            const size_t windowLen) noexcept
{
    float sumSquares = 0.f;
    for (size_t i = 0; i < windowLen; ++i)
    {
        sumSquares += signal[start + i] * signal[start + i];
    }
    const float stddev = std::sqrt(sumSquares / static_cast<float>(windowLen));
    if (stddev <= 0.f)
    {
        return 0.f;
    }
    const auto above = std::count_if(signal.begin() + static_cast<std::ptrdiff_t>(start),
                                     signal.begin() + static_cast<std::ptrdiff_t>(start + windowLen),
                                     [stddev](const float x) { return std::abs(x) > stddev; });
    const float fraction = static_cast<float>(above) / static_cast<float>(windowLen);
    return fraction / kNedGaussianFraction;
}

void writeEchoDensity(std::ofstream& out)
{
    constexpr std::array<size_t, 3> elementCounts{4, 8, 16};
    // Past this the tail is below the FFT/float noise floor at feedback=0.5 (measured);
    // density readings beyond it would track numerical noise, not real diffusion.
    const auto totalSamples = static_cast<size_t>(0.25f * kSampleRate);

    out << "@New plot: title=\"DiffuserDelayChain normalized echo density vs. element count "
           "(feedback=0.65, bulge=0.46)\"\n";
    for (const size_t elements : elementCounts)
    {
        SchroederChain chain(kSampleRate, kBlockSize);
        chain.resetDiffuser(elements, kFeedback, kBulge, kBottomSizeM, kTopSizeM, AbacDsp::skipSmoothing);
        const auto rendered = renderImpulseResponse(chain, totalSamples);

        out << "#elements=" << elements << "\n";
        for (size_t start = 0; start + kNedWindow <= rendered.size(); start += kNedHop)
        {
            const float t = static_cast<float>(start) / kSampleRate;
            out << t << " " << normalizedEchoDensityAt(rendered, start, kNedWindow) << "\n";
        }
    }
}
// ---- Build-up / decay character ----

// Fine resolution (one-shot generation, cheap): 1ms peak window, ~0.17ms hop.
constexpr size_t kEnvelopeWindow = 48;
constexpr size_t kEnvelopeHop = 8;
constexpr float kEnvelopeRenderSeconds = 2.f; // 20 elements hasn't reached the floor by 1s (measured)

void writeEnvelope(std::ofstream& out, const std::vector<float>& rendered)
{
    out << "#envelope\n";
    for (size_t start = 0; start + kEnvelopeWindow <= rendered.size(); start += kEnvelopeHop)
    {
        float peak = 0.f;
        for (size_t i = start; i < start + kEnvelopeWindow; ++i)
        {
            peak = std::max(peak, std::abs(rendered[i]));
        }
        const float t = static_cast<float>(start) / kSampleRate;
        const float db = 20.f * std::log10(std::max(peak, 1e-9f));
        out << t << " " << db << "\n";
    }
}

// A hand-wired, unmodulated 4-tap chain at Dattorro's (1997) plate-reverb prime delay sizes,
// the classic fixed-size reference point DiffuserDelayChain's bulge-distributed sizing
// generalizes - not reachable through DiffuserDelayChain's own public API.
struct DattorroStyleQuad
{
    static constexpr std::array<size_t, 4> kSizesSamples{229, 173, 613, 449};
    using Delay = AbacDsp::ModulatingAllPassDelay<kMaxDelaySamples, AbacDsp::AllpassFeedbackStyle::Schroeder>;

    explicit DattorroStyleQuad(const float sampleRate, const float feedback)
        : m_delay(AbacDsp::constructArray<Delay, 4>(sampleRate))
    {
        for (size_t i = 0; i < 4; ++i)
        {
            m_delay[i].setSize(kSizesSamples[i], AbacDsp::skipSmoothing);
            m_delay[i].setFeedback(feedback);
        }
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::copy_n(source, numSamples, target);
        for (auto& d : m_delay)
        {
            d.processBlockInplace(target, numSamples);
        }
    }

    std::array<Delay, 4> m_delay;
};

void writeBuildupDecay(std::ofstream& out)
{
    const auto totalSamples = static_cast<size_t>(kEnvelopeRenderSeconds * kSampleRate);

    out << "@New plot: title=\"Dattorro-style 4-tap (fixed 229/173/613/449, feedback=0.65)\"\n";
    {
        DattorroStyleQuad quad(kSampleRate, kFeedback);
        writeEnvelope(out, renderImpulseResponse(quad, totalSamples));
    }

    out << "@New plot: title=\"DiffuserDelayChain, 8 elements (bulge formula, feedback=0.65)\"\n";
    {
        SchroederChain chain(kSampleRate, kBlockSize);
        chain.resetDiffuser(8, kFeedback, kBulge, kBottomSizeM, kTopSizeM, AbacDsp::skipSmoothing);
        writeEnvelope(out, renderImpulseResponse(chain, totalSamples));
    }

    out << "@New plot: title=\"DiffuserDelayChain, 20 elements (bulge formula, feedback=0.65)\"\n";
    {
        SchroederChain chain(kSampleRate, kBlockSize);
        chain.resetDiffuser(20, kFeedback, kBulge, kBottomSizeM, kTopSizeM, AbacDsp::skipSmoothing);
        writeEnvelope(out, renderImpulseResponse(chain, totalSamples));
    }
}

// ---- Raw impulse-response waveform vs. feedback (Dattorro-style 4-tap) ----

// 50ms: long enough to show feedback=0's single delayed impulse (the sum of the four
// element sizes, ~30.5ms, since feedback=0 degenerates each element to a plain delay).
constexpr float kWaveformWindowSeconds = 0.05f;

void writeImpulseWaveforms(std::ofstream& out)
{
    constexpr std::array<float, 6> feedbacks{0.f, 0.2f, 0.4f, 0.6f, 0.8f, 1.f};
    const auto totalSamples = static_cast<size_t>(kWaveformWindowSeconds * kSampleRate);

    for (const float feedback : feedbacks)
    {
        out << "@New plot: title=\"Dattorro-style 4-tap, feedback=" << feedback << "\"\n#waveform\n";
        DattorroStyleQuad quad(kSampleRate, feedback);
        const auto rendered = renderImpulseResponse(quad, totalSamples);
        for (size_t i = 0; i < rendered.size(); ++i)
        {
            out << (static_cast<float>(i) / kSampleRate) << " " << rendered[i] << "\n";
        }
    }
}
}

int main(int argc, char* argv[])
{
    const std::string spectralPath = argc > 1 ? argv[1] : "df_spectral.txt";
    const std::string densityPath = argc > 2 ? argv[2] : "df_density.txt";
    const std::string buildupPath = argc > 3 ? argv[3] : "df_buildup.txt";
    const std::string waveformPath = argc > 4 ? argv[4] : "df_waveform.txt";

    std::ofstream spectralOut(spectralPath);
    std::ofstream densityOut(densityPath);
    std::ofstream buildupOut(buildupPath);
    std::ofstream waveformOut(waveformPath);
    if (!spectralOut || !densityOut || !buildupOut || !waveformOut)
    {
        std::cerr << "DiffuserExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writeSpectralComparison(spectralOut);
    writeEchoDensity(densityOut);
    writeBuildupDecay(buildupOut);
    writeImpulseWaveforms(waveformOut);

    std::cout << "DiffuserExplore: wrote " << spectralPath << ", " << densityPath << ", " << buildupPath << ", and "
              << waveformPath << std::endl;
}
