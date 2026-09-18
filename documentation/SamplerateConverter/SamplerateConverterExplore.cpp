// Verification plots for src/includes/SamplerateConverter/: SrPullConverter and
// SrPushConverter, the pull- and push-model callers over the shared SincFilter-based
// resampling core - aliasing spectrograms and a quantified alias-to-fundamental SNR
// table across ratio and filter choice, for all six shipped sinc kernels. Writes into
// the directory given as argv[1]; see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Analysis/FftMisc.h"
#include "Analysis/Spectrogram.h"
#include "Filters/Sinc/sinc_11_128.h"
#include "Filters/Sinc/sinc_21_512.h"
#include "Filters/Sinc/sinc_33_512.h"
#include "Filters/Sinc/sinc_4.h"
#include "Filters/Sinc/sinc_69_768.h"
#include "Filters/Sinc/sinc_7_128.h"
#include "SamplerateConverter/SrPullConverter.h"
#include "SamplerateConverter/SrPushConverter.h"

namespace
{
constexpr float kSampleRate = 48000.f;

struct FilterEntry
{
    std::string name;
    std::shared_ptr<AbacDsp::SincFilter> filter;
};

// All six sinc kernels shipped in src/includes/Filters/Sinc/, by their actual
// identifiers (not filenames): the short one used by tapelooper/organicchorus, and five
// wider ones trading processing cost for stopband depth.
[[nodiscard]] std::vector<FilterEntry> makeFilterSet()
{
    return {
        {"sinc4", std::make_shared<AbacDsp::SincFilter>(sinc4)},
        {"init_7_128", std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_7_128)},
        {"init_11_128", std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_11_128)},
        {"init_21_512", std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_21_512)},
        {"init_33_512", std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_33_512)},
        {"init_69_768", std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_69_768)},
    };
}

[[nodiscard]] std::vector<float> makeSweepTone(const float startHz, const float endHz, const size_t numFrames)
{
    std::vector<float> tone(numFrames);
    float phase = 0.f;
    for (size_t i = 0; i < numFrames; ++i)
    {
        const float frac = numFrames > 1 ? static_cast<float>(i) / static_cast<float>(numFrames - 1) : 0.f;
        const float hz = startHz + (endHz - startHz) * frac;
        tone[i] = std::sin(phase);
        phase += 2.f * std::numbers::pi_v<float> * hz / kSampleRate;
    }
    return tone;
}

// Feeds a linear-sweep (or, with startHz==endHz, constant-tone) input through
// SrPushConverter in one fetchBlock() call at a static ratio (no glide - isolates the
// base-ratio resampler, same isolation DelaysExplore.cpp applies to its own sweep).
[[nodiscard]] std::vector<float> renderPushSweep(const std::shared_ptr<AbacDsp::SincFilter>& filter, const float ratio,
                                                 const float startHz, const float endHz, const size_t desiredOutFrames)
{
    const auto inFrames = static_cast<size_t>(static_cast<float>(desiredOutFrames) / ratio) + 8192;
    const auto in = makeSweepTone(startHz, endHz, inFrames);

    AbacDsp::SrPushConverter<1> conv(filter);
    std::vector<float> out(desiredOutFrames + 8192);
    const auto generated = conv.fetchBlock(ratio, in.data(), in.size(), out.data(), out.size());
    out.resize(std::min(generated, desiredOutFrames));
    return out;
}

// Same input/ratio/output contract as renderPushSweep(), but drives SrPullConverter:
// the whole precomputed sweep is served through one callback (matching
// SrPullConverter_test.cpp's own makeCallback pattern) and the full desired output is
// requested in a single fetchBlock() call.
[[nodiscard]] std::vector<float> renderPullSweep(const std::shared_ptr<const AbacDsp::SincFilter>& filter,
                                                 const float ratio, const float startHz, const float endHz,
                                                 const size_t desiredOutFrames)
{
    const auto inFrames = static_cast<size_t>(static_cast<float>(desiredOutFrames) / ratio) + 8192;
    const auto in = makeSweepTone(startHz, endHz, inFrames);

    AbacDsp::SrPullConverter conv(filter);
    std::vector<float> out(desiredOutFrames);
    bool served = false;
    std::function<long(float**, size_t)> cb = [&](float** ptr, size_t) -> long
    {
        if (served)
        {
            *ptr = nullptr;
            return 0;
        }
        served = true;
        *ptr = const_cast<float*>(in.data());
        return static_cast<long>(in.size());
    };
    conv.fetchBlock(ratio, out.data(), desiredOutFrames, 1, cb);
    return out;
}

// SimpleSpectrogram's worker queue silently drops a frame fed while full (fine for its
// realtime UI use case, not for a batch capture) - poll queueHasRoom() before every feed
// to avoid that. sampleRate must be the render's true resulting rate (kSampleRate*ratio),
// not the input's nominal kSampleRate - see README.md's "Generating the data" section.
void writeSpectrogramGrid(std::ofstream& out, const std::vector<float>& audio, const float sampleRate,
                          const unsigned fftLength)
{
    AbacDsp::SimpleSpectrogram spec;
    spec.setSampleRate(sampleRate);
    spec.setFftLength(fftLength);
    const size_t hop = spec.forwardLength();
    const size_t expectedFrames = audio.size() >= fftLength ? (audio.size() - fftLength) / hop + 1 : 0;
    spec.setSlices(expectedFrames + 8);

    for (size_t fed = 0; fed < audio.size();)
    {
        while (!spec.queueHasRoom())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const size_t chunk = std::min(hop, audio.size() - fed);
        spec.processBlock(audio.data() + fed, chunk);
        fed += chunk;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (spec.getImageSet().activeSlice < expectedFrames && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    const auto img = spec.getImageSet();
    out << img.activeSlice << " " << img.height << " " << img.sampleRate << " " << img.fftLength << " " << hop << "\n";
    for (size_t row = 0; row < img.activeSlice; ++row)
    {
        for (size_t bin = 0; bin < img.height; ++bin)
        {
            const float db = 20.f * std::log10(std::max(img.data[row * img.height + bin], 1e-9f));
            out << db << (bin + 1 < img.height ? ' ' : '\n');
        }
    }
}

// ---- Aliasing spectrograms: extreme downsample (1:10), clean control (1:1), extreme
// upsample (4:1), for both converter classes, all six filters as stacked panels ----

constexpr std::array<float, 3> kSpectrogramRatios{0.1f, 1.0f, 4.0f};
constexpr std::array<const char*, 3> kSpectrogramRatioTags{"0.1", "1.0", "4.0"};
constexpr float kSweepStartHz = 50.f;
constexpr float kSweepEndHz = 15000.f;
constexpr auto kSpecOutFrames = static_cast<size_t>(1.5f * kSampleRate);
constexpr unsigned kSpecFftLength = 2048;

void writeAliasingSpectrograms(const std::string& outDir)
{
    const auto filters = makeFilterSet();

    for (size_t r = 0; r < kSpectrogramRatios.size(); ++r)
    {
        const float ratio = kSpectrogramRatios[r];
        const float outRate = kSampleRate * ratio; // true resulting rate; see writeSpectrogramGrid()

        for (const auto& entry : filters)
        {
            const auto pushAudio = renderPushSweep(entry.filter, ratio, kSweepStartHz, kSweepEndHz, kSpecOutFrames);
            const auto pushBase = outDir + "/sr_aliasing_push_" + kSpectrogramRatioTags[r] + "_" + entry.name;
            std::ofstream pushOut(pushBase + ".txt");
            writeSpectrogramGrid(pushOut, pushAudio, outRate, kSpecFftLength);
            AudioUtility::SaveWav::saveMonoAs(pushBase + ".wav", pushAudio, outRate);

            const auto pullAudio = renderPullSweep(entry.filter, ratio, kSweepStartHz, kSweepEndHz, kSpecOutFrames);
            const auto pullBase = outDir + "/sr_aliasing_pull_" + kSpectrogramRatioTags[r] + "_" + entry.name;
            std::ofstream pullOut(pullBase + ".txt");
            writeSpectrogramGrid(pullOut, pullAudio, outRate, kSpecFftLength);
            AudioUtility::SaveWav::saveMonoAs(pullBase + ".wav", pullAudio, outRate);
        }
    }
}

// ---- Alias-to-fundamental SNR, full ratio sweep x all six filters ----

// A probe fixed to one real frequency is either far below every ratio's own passband (no
// aliasing ever shows: 1000Hz, tried first) or far into the stopband at the low-ratio end
// (attenuated below the noise floor, leaving no real peak to measure: 8000Hz, tried second,
// gave unstable, non-monotonic-in-filter-width numbers). Probing at a fixed fraction of each
// ratio's own new Nyquist instead always sits in the transition band under test.
constexpr float kSnrProbeFraction = 0.9f;

[[nodiscard]] constexpr float snrProbeHzForRatio(const float ratio)
{
    const float nyquist = kSampleRate * 0.5f * (ratio < 1.f ? ratio : 1.f);
    return kSnrProbeFraction * nyquist;
}

constexpr size_t kSnrFftLength = 8192;
constexpr float kSnrGuardHz = 50.f;       // excluded around the fundamental peak when hunting the alias peak
constexpr size_t kSnrWarmupFrames = 8192; // skipped: past the FIR startup transient
constexpr size_t kSnrAnalysisFrames = kSnrFftLength * 16; // averaged across many overlapping windows
constexpr auto kSnrOutFrames = kSnrWarmupFrames + kSnrAnalysisFrames;

// Fundamental/alias found on a Welch-averaged spectrum (FFTResponse::analyse), not one FFT
// window: a single window lands at an arbitrary phase of the beating between the probe and any
// nearby spurious component, making a one-shot ratio swing wildly and non-reproducibly.
// sampleRate is the audio's true resulting rate (see writeSpectrogramGrid()).
[[nodiscard]] float measureSnrDb(const std::vector<float>& audio, const float sampleRate)
{
    if (audio.size() <= kSnrWarmupFrames + kSnrFftLength)
    {
        return 0.f;
    }
    const std::vector steady(audio.begin() + static_cast<std::ptrdiff_t>(kSnrWarmupFrames), audio.end());
    const auto mag = AbacDsp::FFTResponse::analyse<1, 0>(steady, kSnrFftLength);
    if (mag.empty())
    {
        return 0.f;
    }

    const auto peakIt = std::max_element(mag.begin(), mag.end());
    const auto peakIdx = static_cast<size_t>(std::distance(mag.begin(), peakIt));
    const float peakMag = *peakIt;

    const auto binHz = sampleRate / static_cast<float>(kSnrFftLength);
    const auto guardBins = static_cast<size_t>(kSnrGuardHz / binHz) + 1;

    float aliasMag = 0.f;
    for (size_t i = 0; i < mag.size(); ++i)
    {
        const bool inGuardBand = i + guardBins >= peakIdx && i <= peakIdx + guardBins;
        if (!inGuardBand)
        {
            aliasMag = std::max(aliasMag, mag[i]);
        }
    }
    if (peakMag <= 0.f || aliasMag <= 0.f)
    {
        return 160.f; // no measurable second peak: treat as clean
    }
    return 20.f * std::log10(peakMag / aliasMag);
}

[[nodiscard]] std::vector<float> snrRatioSweep()
{
    std::vector<float> ratios;
    for (float r = 0.1f; r <= 1.0001f; r += 0.1f)
    {
        ratios.push_back(r);
    }
    for (float r = 1.5f; r <= 4.0001f; r += 0.5f)
    {
        ratios.push_back(r);
    }
    return ratios;
}

void writeSnrTable(const std::string& outDir)
{
    const auto filters = makeFilterSet();
    const auto ratios = snrRatioSweep();

    std::ofstream out(outDir + "/sr_snr_table.txt");
    out << "ratio";
    for (const auto& entry : filters)
    {
        out << " " << entry.name;
    }
    out << "\n";
    for (const float ratio : ratios)
    {
        const float outRate = kSampleRate * ratio;
        const float probeHz = snrProbeHzForRatio(ratio);
        out << ratio;
        for (const auto& entry : filters)
        {
            const auto audio = renderPushSweep(entry.filter, ratio, probeHz, probeHz, kSnrOutFrames);
            out << " " << measureSnrDb(audio, outRate);
        }
        out << "\n";
    }
}

// Spot-checks that SrPullConverter's SNR matches SrPushConverter's at a couple of
// ratios (printed, not plotted) - the SNR table above uses SrPushConverter throughout
// for readability (see README.md), and this confirms that choice doesn't hide a
// per-class difference.
void printPullPushSpotCheck()
{
    const auto filters = makeFilterSet();
    for (const float ratio : {0.1f, 2.0f})
    {
        const float outRate = kSampleRate * ratio;
        const float probeHz = snrProbeHzForRatio(ratio);
        const auto pushAudio = renderPushSweep(filters[0].filter, ratio, probeHz, probeHz, kSnrOutFrames);
        const auto pullAudio = renderPullSweep(filters[0].filter, ratio, probeHz, probeHz, kSnrOutFrames);
        std::cout << "spot check ratio=" << ratio << " sinc4 push SNR=" << measureSnrDb(pushAudio, outRate)
                  << "dB pull SNR=" << measureSnrDb(pullAudio, outRate) << "dB\n";
    }
}
}

int main(int argc, char* argv[])
{
    const std::string outDir = argc > 1 ? argv[1] : ".";

    writeAliasingSpectrograms(outDir);
    writeSnrTable(outDir);
    printPullPushSpotCheck();

    std::cout << "SamplerateConverterExplore: wrote spectrogram grids/WAVs and sr_snr_table.txt into " << outDir
              << std::endl;
}
