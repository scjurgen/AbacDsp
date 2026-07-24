// Offline harness for tuning the loop Slicer against hand-labelled ground truth.
//
// Usage:
//   SlicerExplore <audiofile.wav> [settings.txt]
//
// The ground-truth file is the audio file with a .txt extension living next to
// it (guitar#1.wav -> guitar#1.txt). Each non-comment line describes one manually
// placed slice:
//
//   <name> <beginSamples> <lengthSamples>
//
// The optional settings file selects the slicing method and its tuning values
// (see loadSettings for the accepted keys). When omitted, defaults are used.
//
// The program runs the chosen Slicer over the file and prints detection
// statistics: precision, recall, F1 and onset timing error against the labels.

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Analysis/Slicer.h"
#include "AudioFile.h"

namespace
{

struct GroundTruthSlice
{
    std::string name;
    size_t begin{0};
    size_t length{0};
};

enum class Method
{
    Transient,
    Spectral,
    Adaptive,
    Grid
};

struct Settings
{
    Method method{Method::Spectral};
    size_t toleranceSamples{2000}; // half-window for counting a detection as a match

    AbacDsp::Slicer::TransientParams transient{};
    AbacDsp::Slicer::SpectralParams spectral{};
    AbacDsp::Slicer::AdaptiveParams adaptive{};
    size_t gridSliceCount{8};
};

[[nodiscard]] std::string trim(std::string_view sv)
{
    const auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    const auto begin = std::find_if(sv.begin(), sv.end(), notSpace);
    const auto end = std::find_if(sv.rbegin(), sv.rend(), notSpace).base();
    return (begin < end) ? std::string{begin, end} : std::string{};
}

[[nodiscard]] std::string stripComment(std::string_view line)
{
    const auto hash = line.find('#');
    return trim(hash == std::string_view::npos ? line : line.substr(0, hash));
}

constexpr std::string_view kFieldSeparators = ",;\t";

// Split on comma / semicolon / tab, trimming each field. Field values (in
// particular slice names) may contain spaces, so spaces are not separators here.
[[nodiscard]] std::vector<std::string> splitFields(std::string_view line)
{
    std::vector<std::string> fields;
    size_t start = 0;
    while (true)
    {
        const size_t sep = line.find_first_of(kFieldSeparators, start);
        fields.push_back(trim(line.substr(start, sep - start)));
        if (sep == std::string_view::npos)
        {
            break;
        }
        start = sep + 1;
    }
    return fields;
}

[[nodiscard]] std::optional<size_t> parseSize(const std::string& text)
{
    size_t value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last)
    {
        return std::nullopt;
    }
    return value;
}

// One label line: "<name><sep><begin><sep><length>" where <sep> is comma,
// semicolon or tab. When no such separator is present the line is whitespace
// tokenized and the name must be a single token.
[[nodiscard]] std::optional<GroundTruthSlice> parseGroundTruthLine(const std::string& line)
{
    std::vector<std::string> fields;
    if (line.find_first_of(kFieldSeparators) != std::string::npos)
    {
        fields = splitFields(line);
    }
    else
    {
        std::istringstream ss(line);
        std::string token;
        while (ss >> token)
        {
            fields.push_back(token);
        }
    }
    if (fields.size() < 3)
    {
        return std::nullopt;
    }
    const std::optional<size_t> begin = parseSize(fields[1]);
    const std::optional<size_t> length = parseSize(fields[2]);
    if (fields[0].empty() || !begin || !length)
    {
        return std::nullopt;
    }
    return GroundTruthSlice{fields[0], *begin, *length};
}

[[nodiscard]] std::vector<GroundTruthSlice> loadGroundTruth(const std::filesystem::path& path)
{
    std::vector<GroundTruthSlice> slices;
    std::ifstream in(path);
    if (!in)
    {
        return slices;
    }
    std::string raw;
    while (std::getline(in, raw))
    {
        const std::string line = stripComment(raw);
        if (line.empty())
        {
            continue;
        }
        if (std::optional<GroundTruthSlice> slice = parseGroundTruthLine(line))
        {
            slices.push_back(std::move(*slice));
        }
        else
        {
            std::cerr << "warning: skipping malformed ground-truth line: " << raw << '\n';
        }
    }
    std::sort(slices.begin(), slices.end(),
              [](const GroundTruthSlice& a, const GroundTruthSlice& b) { return a.begin < b.begin; });
    return slices;
}

void applySetting(Settings& s, const std::string& key, const std::string& value)
{
    const auto asSize = [&]() { return static_cast<size_t>(std::stoul(value)); };
    const auto asFloat = [&]() { return std::stof(value); };

    if (key == "method")
    {
        if (value == "transient")
        {
            s.method = Method::Transient;
        }
        else if (value == "spectral")
        {
            s.method = Method::Spectral;
        }
        else if (value == "adaptive")
        {
            s.method = Method::Adaptive;
        }
        else if (value == "grid")
        {
            s.method = Method::Grid;
        }
        else
        {
            std::cerr << "warning: unknown method '" << value << "'\n";
        }
    }
    else if (key == "tolerance")
    {
        s.toleranceSamples = asSize();
    }
    else if (key == "transient.relativeThreshold")
    {
        s.transient.relativeThreshold = asFloat();
    }
    else if (key == "transient.minGapFrames")
    {
        s.transient.minGapFrames = asSize();
    }
    else if (key == "transient.envelopeWindow")
    {
        s.transient.envelopeWindow = asSize();
    }
    else if (key == "transient.snapMaxDistance")
    {
        s.transient.snapMaxDistance = asSize();
    }
    else if (key == "transient.zeroCrossRadius")
    {
        s.transient.zeroCrossRadius = asSize();
    }
    else if (key == "spectral.fftSize")
    {
        s.spectral.fftSize = asSize();
    }
    else if (key == "spectral.hopSize")
    {
        s.spectral.hopSize = asSize();
    }
    else if (key == "spectral.relativeThreshold")
    {
        s.spectral.relativeThreshold = asFloat();
    }
    else if (key == "spectral.minGapFrames")
    {
        s.spectral.minGapFrames = asSize();
    }
    else if (key == "adaptive.fftSize")
    {
        s.adaptive.fftSize = asSize();
    }
    else if (key == "adaptive.hopSize")
    {
        s.adaptive.hopSize = asSize();
    }
    else if (key == "adaptive.localWindow")
    {
        s.adaptive.localWindow = asSize();
    }
    else if (key == "adaptive.lambda")
    {
        s.adaptive.lambda = asFloat();
    }
    else if (key == "adaptive.delta")
    {
        s.adaptive.delta = asFloat();
    }
    else if (key == "adaptive.minGapFrames")
    {
        s.adaptive.minGapFrames = asSize();
    }
    else if (key == "grid.sliceCount")
    {
        s.gridSliceCount = asSize();
    }
    else
    {
        std::cerr << "warning: unknown setting '" << key << "'\n";
    }
}

[[nodiscard]] Settings loadSettings(const std::optional<std::filesystem::path>& path)
{
    Settings s{};
    if (!path)
    {
        return s;
    }
    std::ifstream in(*path);
    if (!in)
    {
        std::cerr << "warning: cannot open settings file " << *path << ", using defaults\n";
        return s;
    }
    std::string raw;
    while (std::getline(in, raw))
    {
        const std::string line = stripComment(raw);
        if (line.empty())
        {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            std::cerr << "warning: skipping malformed setting line: " << raw << '\n';
            continue;
        }
        applySetting(s, trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
    return s;
}

[[nodiscard]] std::vector<float> loadMono(const AudioFile<float>& audio)
{
    const size_t frames = static_cast<size_t>(audio.getNumSamplesPerChannel());
    const size_t channels = static_cast<size_t>(audio.getNumChannels());
    std::vector<float> mono(frames, 0.f);
    for (size_t ch = 0; ch < channels; ++ch)
    {
        for (size_t i = 0; i < frames; ++i)
        {
            mono[i] += audio.samples[ch][i];
        }
    }
    return mono;
}

[[nodiscard]] std::vector<AbacDsp::Slice> runSlicer(const Settings& s, std::span<const float> mono, size_t loopLength)
{
    switch (s.method)
    {
        case Method::Transient:
            return AbacDsp::Slicer::transientSlices(mono, loopLength, s.transient);
        case Method::Spectral:
            return AbacDsp::Slicer::spectralTransientSlices(mono, loopLength, s.spectral, s.transient);
        case Method::Adaptive:
            return AbacDsp::Slicer::adaptiveTransientSlices(mono, loopLength, s.adaptive, s.transient);
        case Method::Grid:
            return AbacDsp::Slicer::gridSlices(loopLength, s.gridSliceCount);
    }
    return {};
}

struct MatchStats
{
    size_t truePositives{0};
    size_t falsePositives{0};
    size_t falseNegatives{0};
    std::vector<long long> errors;                   // signed onset error (detected - truth) for matches
    std::vector<std::optional<size_t>> matchedOnset; // per ground-truth: matched detected onset
};

// Greedy nearest matching of detected onsets to ground-truth onsets within tolerance.
[[nodiscard]] MatchStats matchOnsets(const std::vector<GroundTruthSlice>& truth, const std::vector<size_t>& detected,
                                     size_t tolerance)
{
    MatchStats stats{};
    stats.matchedOnset.assign(truth.size(), std::nullopt);
    std::vector<bool> usedDetection(detected.size(), false);

    for (size_t t = 0; t < truth.size(); ++t)
    {
        // The boundary at frame 0 is the loop head: implied on both sides and
        // never a detected interior onset, so it is not scored.
        if (truth[t].begin == 0)
        {
            continue;
        }
        const size_t target = truth[t].begin;
        std::optional<size_t> bestIdx;
        size_t bestDist = tolerance + 1;
        for (size_t d = 0; d < detected.size(); ++d)
        {
            if (usedDetection[d])
            {
                continue;
            }
            const size_t dist = (detected[d] > target) ? detected[d] - target : target - detected[d];
            if (dist <= tolerance && dist < bestDist)
            {
                bestDist = dist;
                bestIdx = d;
            }
        }
        if (bestIdx)
        {
            usedDetection[*bestIdx] = true;
            stats.matchedOnset[t] = detected[*bestIdx];
            stats.errors.push_back(static_cast<long long>(detected[*bestIdx]) - static_cast<long long>(target));
            ++stats.truePositives;
        }
        else
        {
            ++stats.falseNegatives;
        }
    }
    stats.falsePositives = detected.size() - stats.truePositives;
    return stats;
}

void printReport(const Settings& s, double sampleRate, const std::vector<GroundTruthSlice>& truth,
                 const std::vector<AbacDsp::Slice>& detectedSlices, const std::vector<size_t>& detectedOnsets,
                 const MatchStats& stats)
{
    const auto samplesToMs = [&](long long n) { return sampleRate > 0.0 ? 1000.0 * n / sampleRate : 0.0; };

    const char* methodName = s.method == Method::Transient  ? "transient"
                             : s.method == Method::Spectral ? "spectral"
                             : s.method == Method::Adaptive ? "adaptive"
                                                            : "grid";

    std::cout << "\n=== Slicer report ===\n";
    std::cout << "method:            " << methodName << '\n';
    std::cout << "tolerance:         " << s.toleranceSamples << " samples ("
              << samplesToMs(static_cast<long long>(s.toleranceSamples)) << " ms)\n";
    std::cout << "ground-truth:      " << truth.size() << " slices\n";
    std::cout << "detected:          " << detectedSlices.size() << " slices (" << detectedOnsets.size()
              << " interior onsets)\n\n";

    const double tp = static_cast<double>(stats.truePositives);
    const double fp = static_cast<double>(stats.falsePositives);
    const double fn = static_cast<double>(stats.falseNegatives);
    const double precision = (tp + fp > 0.0) ? tp / (tp + fp) : 0.0;
    const double recall = (tp + fn > 0.0) ? tp / (tp + fn) : 0.0;
    const double f1 = (precision + recall > 0.0) ? 2.0 * precision * recall / (precision + recall) : 0.0;

    std::cout << "true positives:    " << stats.truePositives << '\n';
    std::cout << "false positives:   " << stats.falsePositives << '\n';
    std::cout << "false negatives:   " << stats.falseNegatives << '\n';
    std::cout << "precision:         " << precision << '\n';
    std::cout << "recall:            " << recall << '\n';
    std::cout << "F1:                " << f1 << '\n';

    if (!stats.errors.empty())
    {
        long long sumAbs = 0;
        long long maxAbs = 0;
        for (const long long e : stats.errors)
        {
            const long long a = std::llabs(e);
            sumAbs += a;
            maxAbs = std::max(maxAbs, a);
        }
        const double meanAbs = static_cast<double>(sumAbs) / static_cast<double>(stats.errors.size());
        std::cout << "mean |onset err|:  " << meanAbs << " samples (" << samplesToMs(static_cast<long long>(meanAbs))
                  << " ms)\n";
        std::cout << "max  |onset err|:  " << maxAbs << " samples (" << samplesToMs(maxAbs) << " ms)\n";
    }

    std::cout << "\n--- per ground-truth slice ---\n";
    std::cout << "name            truth-begin   matched-onset       delta\n";
    for (size_t t = 0; t < truth.size(); ++t)
    {
        std::cout.width(16);
        std::cout << std::left << truth[t].name;
        std::cout.width(12);
        std::cout << std::right << truth[t].begin << "   ";
        if (truth[t].begin == 0)
        {
            std::cout.width(12);
            std::cout << "(loop head)" << "  ";
            std::cout.width(10);
            std::cout << "-" << '\n';
        }
        else if (stats.matchedOnset[t])
        {
            const long long delta =
                static_cast<long long>(*stats.matchedOnset[t]) - static_cast<long long>(truth[t].begin);
            std::cout.width(12);
            std::cout << *stats.matchedOnset[t] << "  ";
            std::cout.width(10);
            std::cout << delta << '\n';
        }
        else
        {
            std::cout.width(12);
            std::cout << "(missed)" << "  ";
            std::cout.width(10);
            std::cout << "-" << '\n';
        }
    }
    std::cout << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " <audiofile.wav> [settings.txt]\n";
        return 1;
    }

    const std::filesystem::path audioPath{argv[1]};
    const std::optional<std::filesystem::path> settingsPath =
        (argc >= 3) ? std::optional<std::filesystem::path>{argv[2]} : std::nullopt;

    std::error_code cwdEc;
    const std::filesystem::path cwd = std::filesystem::current_path(cwdEc);

    std::error_code existsEc;
    if (!std::filesystem::exists(audioPath, existsEc))
    {
        std::cerr << "error: audio file not found: " << audioPath << '\n';
        std::cerr << "       looked at absolute path: " << std::filesystem::absolute(audioPath, existsEc) << '\n';
        std::cerr << "       current working dir:     " << cwd << '\n';
        std::cerr << "       (pass an absolute path, or run from the directory that contains the file)\n";
        return 1;
    }

    AudioFile<float> audio;
    if (!audio.load(audioPath.string()))
    {
        std::cerr << "error: file exists but could not be decoded as audio: " << audioPath << '\n';
        std::cerr << "       (unsupported or corrupt format; SlicerExplore reads WAV/AIFF)\n";
        return 1;
    }

    std::filesystem::path truthPath = audioPath;
    truthPath.replace_extension(".txt");
    const std::vector<GroundTruthSlice> truth = loadGroundTruth(truthPath);
    if (truth.empty())
    {
        std::cerr << "warning: no ground-truth slices read from " << truthPath << '\n';
    }

    const Settings settings = loadSettings(settingsPath);

    const std::vector<float> mono = loadMono(audio);
    const size_t loopLength = mono.size();
    const std::vector<AbacDsp::Slice> slices = runSlicer(settings, mono, loopLength);

    // Interior onsets are slice starts excluding the implied 0 at the loop head,
    // which is what maps to a hand-labelled transient.
    std::vector<size_t> onsets;
    onsets.reserve(slices.size());
    for (const AbacDsp::Slice& s : slices)
    {
        if (s.startFrame > 0)
        {
            onsets.push_back(s.startFrame);
        }
    }

    const MatchStats stats = matchOnsets(truth, onsets, settings.toleranceSamples);
    printReport(settings, audio.getSampleRate(), truth, slices, onsets, stats);

    return 0;
}