// Shared pieces used by this folder's explore programs (VariSpeedTapeDelay/,
// WobbleDelay/): sample rate/tile size, the sinc4 filter factory, the spectrogram-grid
// writer, and the shared-ratio aliasing sweep (renderRatioSweep/writeAliasingCase).
// See each subfolder's README.md for what its outputs mean.
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <memory>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Analysis/Spectrogram.h"
#include "Filters/Sinc/sinc_4.h"
#include "Filters/Sinc/sinc_69_768.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kTileSize = 64;

// Zero-crossing pitch-tracking window/hop shared by the pitch-wobble plots: ~42ms window,
// 10ms hop.
constexpr size_t kPitchWindow = 2000;
constexpr size_t kPitchHop = 480;

[[nodiscard, maybe_unused]] std::shared_ptr<AbacDsp::SincFilter> makeSincFilter()
{
    return std::make_shared<AbacDsp::SincFilter>(sinc4);
}

// SimpleSpectrogram's worker queue silently drops a frame fed while full (fine for its
// realtime UI use case, not for a batch capture) - poll queueHasRoom() before every feed to
// avoid that. Grid format: "rows cols sampleRate fftLength hop" header, then one row per line.
[[maybe_unused]] void writeSpectrogramGrid(std::ofstream& out, const std::vector<float>& audio,
                                           const unsigned fftLength = 2048)
{
    AbacDsp::SimpleSpectrogram spec;
    spec.setSampleRate(kSampleRate);
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

// ---- Aliasing check: extreme vs. mild resampling ratios, sinc4 vs. sinc_69_768 ----
// Driven through VariSpeedTapeDelay's write-side transport.

constexpr std::array<float, 5> kAliasRatios{0.1f, 0.25f, 0.5f, 0.99f, 1.5f};

constexpr float kAliasSweepStartHz = 50.f;
constexpr float kAliasSweepEndHz = 2500.f; // crosses the extreme case's 2400Hz Nyquist, stays
                                           // well under the mild case's 12000Hz - a control
constexpr float kAliasSweepSeconds = 1.5f; // short enough that even ratio=0.5 stays well under
                                           // the tape buffer size over the whole render

// Wow/Flutter and Drift stay off throughout - the base-ratio resampler is the only thing
// under test.
template <typename Tape>
[[nodiscard]] std::vector<float> renderRatioSweep(const std::shared_ptr<AbacDsp::SincFilter>& filter, const float ratio)
{
    constexpr float readDistance = 500.f;
    Tape tape(kSampleRate, filter);
    tape.setWowDepth(0.f);
    tape.setFlutterDepth(0.f);
    tape.setReadHeadSafetyMargin(200.f);
    tape.setRatio(ratio, true);
    tape.setReadHead(0, readDistance, true);

    const auto numTiles = static_cast<size_t>(kAliasSweepSeconds * kSampleRate) / kTileSize;
    std::vector<float> out(numTiles * kTileSize);
    float phase = 0.f;
    for (size_t t = 0; t < numTiles; ++t)
    {
        std::array<float, kTileSize> tileOut{};
        tape.readBlock(0, tileOut);
        std::copy(tileOut.begin(), tileOut.end(), out.begin() + static_cast<std::ptrdiff_t>(t * kTileSize));

        std::array<float, kTileSize> tileIn{};
        for (size_t i = 0; i < kTileSize; ++i)
        {
            const auto sampleIndex = t * kTileSize + i;
            const float frac = static_cast<float>(sampleIndex) / static_cast<float>(numTiles * kTileSize);
            const float hz = kAliasSweepStartHz + (kAliasSweepEndHz - kAliasSweepStartHz) * frac;
            tileIn[i] = std::sin(phase);
            phase += 2.f * std::numbers::pi_v<float> * hz / kSampleRate;
        }
        tape.feed(tileIn);
    }
    return out;
}

// One transport x one ratio: renders both filters, writes each as a spectrogram grid (for
// spectrogram_plot.py) and a WAV (for listening), named "<prefix>_<ratioTag>_<filter>.*".
template <typename Tape>
void writeAliasingCase(const std::string& outDir, const std::string& prefix, const float ratio,
                       const std::string& ratioTag)
{
    const auto sinc4Filter = makeSincFilter();
    const auto sinc768Filter = std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_69_768);

    const auto sinc4Audio = renderRatioSweep<Tape>(sinc4Filter, ratio);
    const auto sinc768Audio = renderRatioSweep<Tape>(sinc768Filter, ratio);

    const auto base = outDir + "/" + prefix + "_" + ratioTag;
    std::ofstream sinc4Out(base + "_sinc4.txt");
    std::ofstream sinc768Out(base + "_sinc69.txt");
    writeSpectrogramGrid(sinc4Out, sinc4Audio);
    writeSpectrogramGrid(sinc768Out, sinc768Audio);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc4.wav", sinc4Audio, kSampleRate);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc69.wav", sinc768Audio, kSampleRate);
}
}
