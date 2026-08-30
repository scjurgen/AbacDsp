// Verification plots for src/includes/Delays/: VariSpeedTapeDelay (wow/flutter baked into
// recorded pitch, the exponential/octave-based transport-speed glide) and MultiTapDelay
// (whole-sample-only, no-interpolation multi-tap spacing). PyConPlot.py output; see README.md.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "Analysis/ZeroCrossings.h"
#include "Delays/MultiTapDelay.h"
#include "Delays/VariSpeedTapeDelay.h"
#include "Filters/Sinc/sinc_4.h"

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
    const std::string pitchPath = argc > 1 ? argv[1] : "td_pitchwobble.txt";
    const std::string glidePath = argc > 2 ? argv[2] : "td_speedglide.txt";
    const std::string multiTapPath = argc > 3 ? argv[3] : "td_multitap.txt";

    std::ofstream pitchOut(pitchPath);
    std::ofstream glideOut(glidePath);
    std::ofstream multiTapOut(multiTapPath);
    if (!pitchOut || !glideOut || !multiTapOut)
    {
        std::cerr << "DelaysExplore: ERROR - failed to open output files for writing" << std::endl;
        return 1;
    }

    writePitchWobble(pitchOut);
    writeSpeedGlide(glideOut);
    writeMultiTapImpulse(multiTapOut);

    std::cout << "DelaysExplore: wrote " << pitchPath << ", " << glidePath << ", and " << multiTapPath << std::endl;
}
