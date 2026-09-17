// Verification plots for src/includes/Delays/: VariSpeedTapeDelay, OrganicChorusTransport (its
// read-head-modulated fork, plus a drift-tracking fix check and a sinc4-vs-sinc_69_768
// aliasing sweep), and MultiTapDelay. Writes into the directory given as argv[1]; see README.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Analysis/Spectrogram.h"
#include "Analysis/ZeroCrossings.h"
#include "Delays/MultiTapDelay.h"
#include "Delays/OrganicChorusTransport.h"
#include "Delays/VariSpeedTapeDelay.h"
#include "Filters/Sinc/sinc_4.h"
#include "Filters/Sinc/sinc_69_768.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"

namespace
{
constexpr float kSampleRate = 48000.f;

// ---- VariSpeedTapeDelay ----

constexpr size_t kTapeBufferSize = 48000; // 1s: comfortably above readDistance + 2*safety margin
constexpr size_t kTileSize = 64;
using Tape = AbacDsp::VariSpeedTapeDelay<kTapeBufferSize, 1, 1, kTileSize>;

[[nodiscard]] std::shared_ptr<AbacDsp::SincFilter> makeSincFilter()
{
    return std::make_shared<AbacDsp::SincFilter>(sinc4);
}

// Continuously feeds a probeHz test tone while reading back readDistance frames behind the
// write head, one tile at a time (read-then-write, same order VariSpeedTapeDelay's own
// callers use) - what a caller actually hears from material recorded while wow/flutter ran.
[[nodiscard]] std::vector<float> renderTapePlayback(const float probeHz, const float readDistance, const bool modulated,
                                                    const size_t numTiles)
{
    auto sincFilter = makeSincFilter();
    Tape tape(kSampleRate, sincFilter);
    tape.setReadHead(0, readDistance, true);
    if (modulated)
    {
        tape.setWowRate(1.5f);
        tape.setWowDepth(0.8f);
        tape.setWowVariance(0.3f);
        tape.setWowDrift(0.f); // isolate the periodic component from the slow random walk
        tape.setFlutterRate(8.f);
        tape.setFlutterDepth(0.8f);
    }
    else
    {
        tape.setWowDepth(0.f);
        tape.setFlutterDepth(0.f);
    }

    std::vector<float> out(numTiles * kTileSize);
    float phase = 0.f;
    const float phaseInc = 2.f * std::numbers::pi_v<float> * probeHz / kSampleRate;
    for (size_t t = 0; t < numTiles; ++t)
    {
        std::array<float, kTileSize> tileOut{};
        tape.readBlock(0, tileOut);
        std::copy(tileOut.begin(), tileOut.end(), out.begin() + static_cast<std::ptrdiff_t>(t * kTileSize));

        std::array<float, kTileSize> tileIn{};
        for (float& s : tileIn)
        {
            s = std::sin(phase);
            phase += phaseInc;
        }
        tape.feed(tileIn);
    }
    return out;
}

constexpr size_t kPitchWindow = 2000; // ~42ms: several periods of a 220Hz probe
constexpr size_t kPitchHop = 480;     // 10ms

void writePitchWobble(std::ofstream& out)
{
    out << "@New plot: title=\"VariSpeedTapeDelay: tracked playback pitch, wow+flutter on vs. "
           "off (220Hz probe)\"\n";
    constexpr float probeHz = 220.f;
    constexpr float readDistance = 24000.f; // 0.5s behind the write head
    const auto numTiles = static_cast<size_t>(5.f * kSampleRate) / kTileSize;

    for (const bool modulated : {false, true})
    {
        const auto rendered = renderTapePlayback(probeHz, readDistance, modulated, numTiles);
        out << "#" << (modulated ? "wow+flutter on" : "wow+flutter off") << "\n";
        for (size_t start = 0; start + kPitchWindow <= rendered.size(); start += kPitchHop)
        {
            const float period =
                AbacDsp::periodLengthByZeroCrossingAverage(rendered.data() + start, kPitchWindow, true);
            if (period <= 0.f)
            {
                continue;
            }
            const float t = static_cast<float>(start) / kSampleRate;
            out << t << " " << (kSampleRate / period) << "\n";
        }
    }
}

// Tracks writeHead() advance per tile, proportional to the instantaneous transport ratio
// (feed() resamples exactly kTileSize input frames by that ratio each call) - a non-invasive
// way to read the glide curve back without VariSpeedTapeDelay exposing the ratio directly.
void writeSpeedGlide(std::ofstream& out)
{
    out << "@New plot: title=\"VariSpeedTapeDelay setRatio() glide: octave-invariant timing, "
           "accel != brake\"\n";
    const std::array<std::pair<float, float>, 3> segments{{{1.f, 2.f}, {2.f, 1.f}, {0.5f, 1.f}}};
    const std::array<const char*, 3> names{"1.0 -> 2.0 (up, 1 octave)", "2.0 -> 1.0 (down, 1 octave)",
                                           "0.5 -> 1.0 (up, 1 octave)"};

    for (size_t s = 0; s < segments.size(); ++s)
    {
        auto sincFilter = makeSincFilter();
        Tape tape(kSampleRate, sincFilter);
        tape.setWowDepth(0.f);
        tape.setFlutterDepth(0.f);
        tape.setRatio(segments[s].first, true);

        const auto numTiles = static_cast<size_t>(1.f * kSampleRate) / kTileSize;
        std::array<float, kTileSize> silence{};
        std::array<float, kTileSize> scratch{};
        for (size_t t = 0; t < numTiles / 10; ++t) // settle at the starting ratio first
        {
            tape.readBlock(0, scratch);
            tape.feed(silence);
        }
        tape.setRatio(segments[s].second);

        out << "#" << names[s] << "\n";
        size_t lastWriteHead = tape.writeHead();
        for (size_t t = 0; t < numTiles; ++t)
        {
            tape.readBlock(0, scratch);
            tape.feed(silence);
            const auto writeHead = tape.writeHead();
            const auto advance = (writeHead + kTapeBufferSize - lastWriteHead) % kTapeBufferSize;
            lastWriteHead = writeHead;
            const float t_ = static_cast<float>(t * kTileSize) / kSampleRate;
            const float ratioEstimate = static_cast<float>(advance) / static_cast<float>(kTileSize);
            out << t_ << " " << ratioEstimate << "\n";
        }
    }
}

// ---- OrganicChorusTransport (organicchorus's own fork: Wow/Flutter on the read head) ----

using ReadHeadTape = AbacDsp::OrganicChorusTransport<kTapeBufferSize, 1, 1, kTileSize>;

// Mirrors renderTapePlayback() above, but Wow/Flutter live on the read head here, not the
// write clock - the write side stays clean regardless of the modulated flag.
[[nodiscard]] std::vector<float> renderReadHeadPlayback(const float probeHz, const float readDistance,
                                                        const bool modulated, const size_t numTiles)
{
    auto sincFilter = makeSincFilter();
    ReadHeadTape tape(kSampleRate, sincFilter);
    tape.setRatio(1.f, true);
    tape.setReadHead(0, readDistance, true);
    if (modulated)
    {
        tape.setWowRate(1.5f);
        tape.setWowDepth(0.8f);
        tape.setWowVariance(0.3f);
        tape.setWowDrift(0.f);
        tape.setFlutterRate(8.f);
        tape.setFlutterDepth(0.8f);
    }
    else
    {
        tape.setWowDepth(0.f);
        tape.setFlutterDepth(0.f);
    }

    std::vector<float> out(numTiles * kTileSize);
    float phase = 0.f;
    const float phaseInc = 2.f * std::numbers::pi_v<float> * probeHz / kSampleRate;
    for (size_t t = 0; t < numTiles; ++t)
    {
        std::array<float, kTileSize> tileOut{};
        tape.readBlock(0, tileOut);
        std::copy(tileOut.begin(), tileOut.end(), out.begin() + static_cast<std::ptrdiff_t>(t * kTileSize));

        std::array<float, kTileSize> tileIn{};
        for (float& s : tileIn)
        {
            s = std::sin(phase);
            phase += phaseInc;
        }
        tape.feed(tileIn);
    }
    return out;
}

void writeReadHeadWobble(std::ofstream& out)
{
    out << "@New plot: title=\"OrganicChorusTransport: tracked playback pitch, wow+flutter on "
           "vs. off (220Hz probe, read head modulated)\"\n";
    constexpr float probeHz = 220.f;
    constexpr float readDistance = 24000.f; // 0.5s behind the write head
    const auto numTiles = static_cast<size_t>(5.f * kSampleRate) / kTileSize;

    for (const bool modulated : {false, true})
    {
        const auto rendered = renderReadHeadPlayback(probeHz, readDistance, modulated, numTiles);
        out << "#" << (modulated ? "wow+flutter on" : "wow+flutter off") << "\n";
        for (size_t start = 0; start + kPitchWindow <= rendered.size(); start += kPitchHop)
        {
            const float period =
                AbacDsp::periodLengthByZeroCrossingAverage(rendered.data() + start, kPitchWindow, true);
            if (period <= 0.f)
            {
                continue;
            }
            const float t = static_cast<float>(start) / kSampleRate;
            out << t << " " << (kSampleRate / period) << "\n";
        }
    }
}

// ---- Aliasing check: extreme (1:10) vs. mild (1:2) resampling, sinc4 vs. sinc_69_768 ----

constexpr float kAliasRatioExtreme = 0.1f; // write at 4800 samples/sec, Nyquist 2400Hz
constexpr float kAliasRatioMild = 0.5f;    // write at 24000 samples/sec, Nyquist 12000Hz
constexpr float kAliasSweepStartHz = 50.f;
constexpr float kAliasSweepEndHz = 2500.f; // crosses the extreme case's 2400Hz Nyquist, stays
                                           // well under the mild case's 12000Hz - a control
constexpr float kAliasSweepSeconds = 1.5f; // short enough that even ratio=0.5 stays well under
                                           // kTapeBufferSize over the whole render
constexpr unsigned kSpecFftLength = 2048;

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

// ---- Aliasing check: constant 1000Hz probe, ratio glided from 1.0 down to 0.1 ----

constexpr float kRatioGlideProbeHz = 1000.f;
constexpr float kRatioGlideStart = 1.0f;
constexpr float kRatioGlideEnd = 0.1f;
constexpr int kRatioGlideSteps = 10;
// Per-step hold, comfortably longer than the slowest single step's own brake glide (the
// 0.2->0.1 step is a full octave, ~0.33s at brakePerSec=3) so the ratio actually settles.
constexpr float kRatioGlideStepSeconds = 1.f;
// ratio averages ~0.55 across the run - needs ~0.55*kRatioGlideSteps*kRatioGlideStepSeconds*kSampleRate frames.
constexpr size_t kRatioGlideBufferSize = 1u << 19;
using GlideTape = AbacDsp::VariSpeedTapeDelay<kRatioGlideBufferSize, 1, 1, kTileSize>;

// A fixed 1000Hz tone stays far below the shrinking effective Nyquist throughout. Ratio steps
// down once per hold period via the transport's own accel/brake glide (setRatio(..., false),
// not forced), each step held long enough to actually settle before the next one starts.
[[nodiscard]] std::vector<float> renderRatioGlideProbe(const std::shared_ptr<AbacDsp::SincFilter>& filter)
{
    constexpr float readDistance = 500.f;
    GlideTape tape(kSampleRate, filter);
    tape.setWowDepth(0.f);
    tape.setFlutterDepth(0.f);
    tape.setReadHeadSafetyMargin(200.f);
    tape.setRatio(kRatioGlideStart, true);
    tape.setReadHead(0, readDistance, true);

    const auto tilesPerStep = static_cast<size_t>(kRatioGlideStepSeconds * kSampleRate) / kTileSize;
    const auto numTiles = tilesPerStep * static_cast<size_t>(kRatioGlideSteps);
    std::vector<float> out(numTiles * kTileSize);
    float phase = 0.f;
    const float phaseInc = 2.f * std::numbers::pi_v<float> * kRatioGlideProbeHz / kSampleRate;
    for (int step = 0; step < kRatioGlideSteps; ++step)
    {
        const float frac = static_cast<float>(step) / static_cast<float>(kRatioGlideSteps - 1);
        tape.setRatio(kRatioGlideStart + (kRatioGlideEnd - kRatioGlideStart) * frac, false);

        for (size_t t = 0; t < tilesPerStep; ++t)
        {
            const auto tileIndex = static_cast<size_t>(step) * tilesPerStep + t;
            std::array<float, kTileSize> tileOut{};
            tape.readBlock(0, tileOut);
            std::copy(tileOut.begin(), tileOut.end(), out.begin() + static_cast<std::ptrdiff_t>(tileIndex * kTileSize));

            std::array<float, kTileSize> tileIn{};
            for (float& s : tileIn)
            {
                s = std::sin(phase);
                phase += phaseInc;
            }
            tape.feed(tileIn);
        }
    }
    return out;
}

// SimpleSpectrogram's worker queue silently drops a frame fed while full (fine for its
// realtime UI use case, not for a batch capture) - poll queueHasRoom() before every feed to
// avoid that. Grid format: "rows cols sampleRate fftLength hop" header, then one row per line.
void writeSpectrogramGrid(std::ofstream& out, const std::vector<float>& audio)
{
    AbacDsp::SimpleSpectrogram spec;
    spec.setSampleRate(kSampleRate);
    spec.setFftLength(kSpecFftLength);
    const size_t hop = spec.forwardLength();
    const size_t expectedFrames = audio.size() >= kSpecFftLength ? (audio.size() - kSpecFftLength) / hop + 1 : 0;
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
    std::ofstream sinc768Out(base + "_sinc69768.txt");
    writeSpectrogramGrid(sinc4Out, sinc4Audio);
    writeSpectrogramGrid(sinc768Out, sinc768Audio);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc4.wav", sinc4Audio, kSampleRate);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc69768.wav", sinc768Audio, kSampleRate);
}

void writeAliasingSpectrograms(const std::string& outDir)
{
    writeAliasingCase<Tape>(outDir, "td_aliasing_writeside", kAliasRatioExtreme, "1to10");
    writeAliasingCase<Tape>(outDir, "td_aliasing_writeside", kAliasRatioMild, "1to2");
    writeAliasingCase<ReadHeadTape>(outDir, "td_aliasing_readside", kAliasRatioExtreme, "1to10");
    writeAliasingCase<ReadHeadTape>(outDir, "td_aliasing_readside", kAliasRatioMild, "1to2");
}

void writeRatioGlideAliasing(const std::string& outDir)
{
    const auto sinc4Filter = makeSincFilter();
    const auto sinc768Filter = std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_69_768);

    const auto sinc4Audio = renderRatioGlideProbe(sinc4Filter);
    const auto sinc768Audio = renderRatioGlideProbe(sinc768Filter);

    const auto base = outDir + "/td_aliasing_ratioglide";
    std::ofstream sinc4Out(base + "_sinc4.txt");
    std::ofstream sinc768Out(base + "_sinc69768.txt");
    writeSpectrogramGrid(sinc4Out, sinc4Audio);
    writeSpectrogramGrid(sinc768Out, sinc768Audio);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc4.wav", sinc4Audio, kSampleRate);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc69768.wav", sinc768Audio, kSampleRate);
}

// ---- Drift-tracking fix verification ----

// Renders the fixed read-head-tracking behavior's write/read distance error over a long run
// under sustained Drift (see test/Delays/OrganicChorusTransport_test.cpp's regression test for
// the bug/fix) - a positive confirmation it stays bounded, not a before/after comparison.
void writeDriftStability(std::ofstream& out)
{
    out << "@New plot: title=\"OrganicChorusTransport: write/read distance error under "
           "sustained Drift (post-fix)\"\n";
    constexpr size_t kBufferSize = 8192;
    constexpr float sigma = 0.2f * 0.06f;
    constexpr float targetDistance = 672.f;
    constexpr float durationSeconds = 30.f;
    constexpr size_t hop = 480; // 10ms at 48kHz, matches the pitch-wobble plots' cadence

    AbacDsp::OrganicChorusTransport<kBufferSize, 1, 1, kTileSize> tape(kSampleRate, makeSincFilter());
    tape.setRatio(1.f, true);
    tape.setReadHeadSafetyMargin(280.f);
    tape.setReadHeadCorrectionThreshold(0, 220.f);
    tape.setReadHead(0, targetDistance, true);
    tape.setWowRate(3.3f);
    tape.setWowDepth(0.45f);
    tape.setFlutterRate(3.3f);
    tape.setFlutterDepth(0.225f);

    AbacDsp::OrnsteinUhlenbeckProcess speedDrift(kSampleRate / static_cast<float>(kTileSize));
    speedDrift.setSigma(sigma);

    out << "#distance error (samples)\n";
    const auto numTiles = static_cast<size_t>(durationSeconds * kSampleRate) / kTileSize;
    for (size_t t = 0; t < numTiles; ++t)
    {
        tape.setExternalRatioPerturbation(speedDrift.step() - sigma);
        std::array<float, kTileSize> silence{};
        tape.feed(silence);
        std::array<float, kTileSize> discard{};
        tape.readBlock(0, discard);

        if (t % hop == 0)
        {
            auto delta = static_cast<double>(tape.writeHead()) - tape.readHead(0);
            while (delta < 0.0)
            {
                delta += kBufferSize;
            }
            while (delta >= kBufferSize)
            {
                delta -= kBufferSize;
            }
            const float time = static_cast<float>(t * kTileSize) / kSampleRate;
            out << time << " " << (static_cast<float>(delta) - targetDistance) << "\n";
        }
    }
}

// ---- MultiTapDelay ----

constexpr size_t kMultiTapMaxSize = 4096;
constexpr size_t kNumTaps = 5;
constexpr std::array<size_t, kNumTaps> kTapDelays{200, 500, 900, 1400, 2000};

void writeMultiTapImpulse(std::ofstream& out)
{
    out << "@New plot: title=\"MultiTapDelay: whole-sample-only impulse response, 5 taps\"\n";
    AbacDsp::MultiTapDelay<kMultiTapMaxSize, kNumTaps> delay;
    for (size_t i = 0; i < kNumTaps; ++i)
    {
        delay.setTapDelay(i, kTapDelays[i]);
    }

    constexpr size_t kTotalSamples = kMultiTapMaxSize;
    std::array<std::vector<float>, kNumTaps> tapOut;
    for (auto& v : tapOut)
    {
        v.resize(kTotalSamples);
    }
    for (size_t n = 0; n < kTotalSamples; ++n)
    {
        delay.write(n == 0 ? 1.f : 0.f);
        for (size_t i = 0; i < kNumTaps; ++i)
        {
            // A simple external per-tap decay gain, the way resonik/spectraltap apply their
            // own gain after readTap() - MultiTapDelay itself has no built-in decay.
            const float gain = 1.f / (1.f + static_cast<float>(i));
            tapOut[i][n] = delay.readTap(i) * gain;
        }
    }

    for (size_t i = 0; i < kNumTaps; ++i)
    {
        out << "#tap " << i << " (delay=" << kTapDelays[i] << ")\n";
        for (size_t n = 0; n < kTotalSamples; ++n)
        {
            out << n << " " << tapOut[i][n] << "\n";
        }
    }
}
}

int main(int argc, char* argv[])
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    const auto path = [&](const std::string& name) { return outDir + "/" + name; };

    const std::array<std::string, 4> names{"td_pitchwobble.txt", "td_speedglide.txt", "td_readheadwobble.txt",
                                           "td_driftstability.txt"};
    std::array<std::ofstream, 4> outs;
    for (size_t i = 0; i < names.size(); ++i)
    {
        outs[i].open(path(names[i]));
        if (!outs[i])
        {
            std::cerr << "DelaysExplore: ERROR - failed to open " << path(names[i]) << " for writing" << std::endl;
            return 1;
        }
    }
    std::ofstream multiTapOut(path("td_multitap.txt"));
    if (!multiTapOut)
    {
        std::cerr << "DelaysExplore: ERROR - failed to open " << path("td_multitap.txt") << " for writing" << std::endl;
        return 1;
    }

    writePitchWobble(outs[0]);
    writeSpeedGlide(outs[1]);
    writeReadHeadWobble(outs[2]);
    writeDriftStability(outs[3]);
    writeAliasingSpectrograms(outDir);
    writeRatioGlideAliasing(outDir);
    writeMultiTapImpulse(multiTapOut);

    std::cout << "DelaysExplore: wrote plot data and aliasing grids/WAVs into " << outDir << std::endl;
}
