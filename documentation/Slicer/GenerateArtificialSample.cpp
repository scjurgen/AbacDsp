// Regenerates the artificial percussion fixture used by SlicerExplore.
//
// Usage:
//   GenerateArtificialSample [outputDir]
//
// Writes <outputDir>/artificial-percussion.wav and .txt (default outputDir is
// "AudioSamples"). The WAV holds six sharp-attack decaying tone bursts at known
// onsets; the .txt is the matching hand-labelled ground truth in the format
// SlicerExplore expects (name begin length).

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "AudioFile.h"

namespace
{

struct Hit
{
    std::string name;
    size_t begin;
    float freq;  // fundamental of the tone burst
    float decay; // exponential decay rate across the burst
};

constexpr int kSampleRate = 48000;
constexpr size_t kTotalFrames = 112000; // ~2.33 s
constexpr size_t kBurstFrames = 11000;
constexpr size_t kClickFrames = 40;

[[nodiscard]] std::array<Hit, 6> makeHits()
{
    return {{
        {"kick", 0, 90.f, 6.f},
        {"snare", 16000, 240.f, 8.f},
        {"tom", 34000, 160.f, 7.f},
        {"hat", 55000, 1200.f, 14.f},
        {"kick", 74000, 90.f, 6.f},
        {"snare", 92000, 240.f, 8.f},
    }};
}

void renderHit(AudioFile<float>& audio, const Hit& h)
{
    for (size_t i = 0; i < kBurstFrames && h.begin + i < kTotalFrames; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        const float env = std::exp(-h.decay * static_cast<float>(i) / static_cast<float>(kBurstFrames));
        const float body = std::sin(2.f * 3.14159265f * h.freq * t);
        const float click =
            (i < kClickFrames) ? 0.4f * (1.f - static_cast<float>(i) / static_cast<float>(kClickFrames)) : 0.f;
        audio.samples[0][h.begin + i] += 0.6f * env * body + click;
    }
}

void writeGroundTruth(const std::filesystem::path& path, const std::array<Hit, 6>& hits)
{
    std::ofstream gt(path);
    gt << "# Artificial percussion: sharp-attack decaying tone bursts.\n";
    gt << "# name  begin(samples)  length(samples)\n";
    for (size_t k = 0; k < hits.size(); ++k)
    {
        const size_t begin = hits[k].begin;
        const size_t end = (k + 1 < hits.size()) ? hits[k + 1].begin : kTotalFrames;
        gt << hits[k].name << "  " << begin << "  " << (end - begin) << "\n";
    }
}

} // namespace

int main(int argc, char* argv[])
{
    const std::filesystem::path outputDir =
        (argc >= 2) ? std::filesystem::path{argv[1]} : std::filesystem::path{"AudioSamples"};
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);

    const std::array<Hit, 6> hits = makeHits();

    AudioFile<float> audio;
    audio.setSampleRate(kSampleRate);
    audio.setBitDepth(16);
    audio.setAudioBufferSize(1, static_cast<int>(kTotalFrames));
    for (const Hit& h : hits)
    {
        renderHit(audio, h);
    }

    const std::filesystem::path wavPath = outputDir / "artificial-percussion.wav";
    const std::filesystem::path txtPath = outputDir / "artificial-percussion.txt";
    if (!audio.save(wavPath.string()))
    {
        std::cerr << "error: cannot write " << wavPath << '\n';
        return 1;
    }
    writeGroundTruth(txtPath, hits);

    std::cout << "wrote " << wavPath << " and " << txtPath << '\n';
    return 0;
}
