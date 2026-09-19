// Verification plots for VariSpeedTapeDelay: wow/flutter-shaped recorded pitch, the
// exponential/octave-based transport-speed glide, and the write-side half of the
// sinc4-vs-sinc_69_768 aliasing sweep (both the fixed-ratio sweep and the settling
// ratio-glide probe). See README.md.

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Analysis/ZeroCrossings.h"
#include "Delays/VariSpeedTapeDelay.h"
#include "DelaysExploreCommon.h"

namespace
{
constexpr size_t kTapeBufferSize = 48000; // 1s: comfortably above readDistance + 2*safety margin
using Tape = AbacDsp::VariSpeedTapeDelay<kTapeBufferSize, 1, 1, kTileSize>;

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

void writeWritesideAliasing(const std::string& outDir)
{
    for (auto ratio : kAliasRatios)
    {
        std::stringstream ss;
        ss << ratio;
        writeAliasingCase<Tape>(outDir, "td_aliasing_writeside", ratio, ss.str());
    }
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

void writeRatioGlideAliasing(const std::string& outDir)
{
    const auto sinc4Filter = makeSincFilter();
    const auto sinc768Filter = std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_69_768);

    const auto sinc4Audio = renderRatioGlideProbe(sinc4Filter);
    const auto sinc768Audio = renderRatioGlideProbe(sinc768Filter);

    const auto base = outDir + "/td_aliasing_ratioglide";
    std::ofstream sinc4Out(base + "_sinc4.txt");
    std::ofstream sinc768Out(base + "_sinc69.txt");
    writeSpectrogramGrid(sinc4Out, sinc4Audio);
    writeSpectrogramGrid(sinc768Out, sinc768Audio);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc4.wav", sinc4Audio, kSampleRate);
    AudioUtility::SaveWav::saveMonoAs(base + "_sinc69.wav", sinc768Audio, kSampleRate);
}
}

int main(int argc, char* argv[])
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    const auto path = [&](const std::string& name) { return outDir + "/" + name; };

    std::ofstream pitchWobbleOut(path("td_pitchwobble.txt"));
    std::ofstream speedGlideOut(path("td_speedglide.txt"));
    if (!pitchWobbleOut || !speedGlideOut)
    {
        std::cerr << "VariSpeedTapeDelayExplore: ERROR - failed to open output files in " << outDir << std::endl;
        return 1;
    }

    writePitchWobble(pitchWobbleOut);
    writeSpeedGlide(speedGlideOut);
    writeWritesideAliasing(outDir);
    writeRatioGlideAliasing(outDir);

    std::cout << "VariSpeedTapeDelayExplore: wrote plot data and aliasing grids/WAVs into " << outDir << std::endl;
}
