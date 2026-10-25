// Verifies a real recording against a score with NoteVerifier: prints the report and writes the plot data.
// usage: NoteVerifierExplore <recording.wav> <score file> <output directory>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "Analysis/NoteVerifier.h"
#include "AudioFile.h"

namespace
{
using Points = std::vector<std::pair<double, double>>;

constexpr float kWindowBeats{0.45f};
constexpr double kFloorDb{-90.0};
constexpr double kFrameSeconds{0.005};
constexpr double kHopSeconds{0.0025};
constexpr double kCloseUpSeconds{0.3};
constexpr double kGapLeadSeconds{0.1};
constexpr double kGapTailSeconds{0.6};

struct Score
{
    double bpm{120.0};
    std::vector<AbacDsp::ExpectedNote> notes;
};

// Lines: "bpm <value>" and "note <midi> <beat> <length in beats>"; everything else is a comment.
Score readScore(const std::string& path)
{
    Score score;
    std::ifstream in{path};
    std::string key;
    while (in >> key)
    {
        if (key == "bpm")
        {
            in >> score.bpm;
        }
        else if (key == "note")
        {
            double midi{0.0};
            double beat{0.0};
            double length{0.0};
            in >> midi >> beat >> length;
            score.notes.push_back({static_cast<float>(beat * 60.0 / score.bpm), static_cast<float>(midi)});
        }
        else
        {
            in.ignore(1 << 16, '\n');
        }
    }
    return score;
}

std::vector<float> loadMono(const AudioFile<float>& audio)
{
    const auto frames = static_cast<size_t>(audio.getNumSamplesPerChannel());
    const auto channels = static_cast<size_t>(audio.getNumChannels());
    std::vector<float> mono(frames, 0.f);
    for (size_t c = 0; c < channels; ++c)
    {
        for (size_t i = 0; i < frames; ++i)
        {
            mono[i] += audio.samples[c][i] / static_cast<float>(channels);
        }
    }
    return mono;
}

std::string noteName(const double midi)
{
    constexpr std::array<const char*, 12> names{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const auto n = static_cast<int>(std::lround(midi));
    return std::string{names[static_cast<size_t>(((n % 12) + 12) % 12)]} + std::to_string(n / 12 - 1);
}

const char* verdictName(const AbacDsp::PitchVerdict verdict)
{
    switch (verdict)
    {
        case AbacDsp::PitchVerdict::Right:
            return "right";
        case AbacDsp::PitchVerdict::Sharp:
            return "sharp";
        case AbacDsp::PitchVerdict::Flat:
            return "flat";
        case AbacDsp::PitchVerdict::WrongNote:
            return "wrong note";
        case AbacDsp::PitchVerdict::NoPitch:
            return "no pitch";
        case AbacDsp::PitchVerdict::Missing:
            return "missing";
    }
    return "";
}

void printReport(const AbacDsp::VerificationReport& report, const std::vector<AbacDsp::ExpectedNote>& expected)
{
    std::printf("alignment %+.0f ms, median note level %.1f dB, unexpected-sound threshold %.1f dB\n",
                static_cast<double>(report.alignmentSeconds) * 1000.0, static_cast<double>(report.medianNoteDb),
                static_cast<double>(report.thresholdDb));
    std::printf(" #  want  expected s  onset s  onset error ms  heard  cents  peak dB  attack dB  verdict\n");
    for (size_t i = 0; i < report.notes.size(); ++i)
    {
        const auto& n = report.notes[i];
        const std::string want = noteName(static_cast<double>(expected[i].midiNote));
        if (!n.found)
        {
            std::printf("%2zu  %-4s  %9.3f  missing\n", i + 1, want.c_str(), static_cast<double>(n.expectedSeconds));
            continue;
        }
        std::printf("%2zu  %-4s  %9.3f  %7.3f  %+14.0f  %5s  %+5.0f  %7.1f  %9.1f  %s\n", i + 1, want.c_str(),
                    static_cast<double>(n.expectedSeconds), static_cast<double>(n.onsetSeconds),
                    static_cast<double>(n.onsetErrorSeconds()) * 1000.0,
                    n.midiNote > 0.f ? noteName(static_cast<double>(n.midiNote)).c_str() : "-",
                    static_cast<double>(n.pitchErrorCents), static_cast<double>(n.peakDb),
                    static_cast<double>(n.attackRmsDb), verdictName(n.verdict));
    }
    std::printf("\nunexpected sound: %zu event(s), %zu short burst(s) above the threshold\n", report.unexpected.size(),
                report.shortBursts);
    for (const auto& u : report.unexpected)
    {
        std::printf("  %.3f - %.3f s  peak %.1f dB  pitch %s\n", static_cast<double>(u.fromSeconds),
                    static_cast<double>(u.toSeconds), static_cast<double>(u.peakDb),
                    u.midiNote > 0.f ? noteName(static_cast<double>(u.midiNote)).c_str() : "-");
    }
    std::printf("loudest ignored sound %.1f dB, %.1f dB under the threshold\n",
                static_cast<double>(report.loudestIgnoredDb), static_cast<double>(report.marginDb()));
}

Points levelEnvelope(const std::vector<float>& mono, const float rate)
{
    const auto frame = static_cast<size_t>(kFrameSeconds * static_cast<double>(rate));
    const auto hop = static_cast<size_t>(kHopSeconds * static_cast<double>(rate));
    Points points;
    for (size_t start = 0; start + frame <= mono.size(); start += hop)
    {
        double sum = 0.0;
        for (size_t i = start; i < start + frame; ++i)
        {
            sum += static_cast<double>(mono[i]) * static_cast<double>(mono[i]);
        }
        const double db = 20.0 * std::log10(std::sqrt(sum / static_cast<double>(frame)) + 1e-9);
        points.emplace_back((static_cast<double>(start) + 0.5 * static_cast<double>(frame)) / static_cast<double>(rate),
                            std::max(db, kFloorDb));
    }
    return points;
}

Points spikes(const std::vector<double>& times, const double high, const double from, const double to)
{
    Points points;
    for (const double t : times)
    {
        if (t >= from && t <= to)
        {
            points.emplace_back(t - 0.0005, kFloorDb);
            points.emplace_back(t, high);
            points.emplace_back(t + 0.0005, kFloorDb);
        }
    }
    if (points.empty())
    {
        points.emplace_back(from, kFloorDb);
    }
    return points;
}

void writeSeries(std::ofstream& out, const std::string& name, const Points& points)
{
    out << "#" << name << "\n";
    for (const auto& [x, y] : points)
    {
        out << x << " " << y << "\n";
    }
}

struct PlotData
{
    Points level;
    std::vector<double> expected;
    std::vector<double> onsets;
    std::vector<double> ends;
    std::vector<double> events;
    double thresholdDb{0.0};
};

PlotData plotDataOf(const AbacDsp::VerificationReport& report, Points level)
{
    PlotData data;
    data.level = std::move(level);
    data.thresholdDb = static_cast<double>(report.thresholdDb);
    for (const auto& n : report.notes)
    {
        data.expected.push_back(static_cast<double>(n.expectedSeconds));
        if (n.found)
        {
            data.onsets.push_back(static_cast<double>(n.onsetSeconds));
            data.ends.push_back(static_cast<double>(n.offsetSeconds));
        }
    }
    for (const auto& u : report.unexpected)
    {
        data.events.push_back(0.5 * static_cast<double>(u.fromSeconds + u.toSeconds));
    }
    return data;
}

void writePlot(std::ofstream& out, const PlotData& data, const std::string& title, const double from, const double to)
{
    out << "@New plot: title=\"" << title << "\" labelx=\"time (s)\" labely=\"level (dB)\" miny=-90 maxy=0\n";
    Points level;
    std::ranges::copy_if(data.level, std::back_inserter(level),
                         [&](const auto& p) { return p.first >= from && p.first <= to; });
    writeSeries(out, "level, 5 ms frames", level);
    writeSeries(out, "unexpected-sound threshold", {{from, data.thresholdDb}, {to, data.thresholdDb}});
    writeSeries(out, "expected onset (fitted)", spikes(data.expected, -20.0, from, to));
    writeSeries(out, "verified onset", spikes(data.onsets, -5.0, from, to));
    writeSeries(out, "end of the note", spikes(data.ends, -35.0, from, to));
    writeSeries(out, "unexpected event", spikes(data.events, -10.0, from, to));
}

// The score index in front of the longest pause, and the indices of the close-up notes around it.
size_t longestGap(const std::vector<AbacDsp::ExpectedNote>& expected)
{
    size_t best = 0;
    for (size_t i = 0; i + 1 < expected.size(); ++i)
    {
        const auto gap = [&](const size_t k) { return expected[k + 1].onsetSeconds - expected[k].onsetSeconds; };
        best = gap(i) > gap(best) ? i : best;
    }
    return best;
}

void writePlots(const std::string& directory, const AbacDsp::VerificationReport& report,
                const std::vector<AbacDsp::ExpectedNote>& expected, const PlotData& data, const double duration)
{
    {
        std::ofstream out{directory + "/nv_full.txt"};
        writePlot(out, data, "Level with the expected and the verified onsets", 0.0, duration);
    }
    const size_t gap = longestGap(expected);
    {
        std::ofstream out{directory + "/nv_empty.txt"};
        const double from = static_cast<double>(expected[gap].onsetSeconds) - kGapLeadSeconds;
        const double to = static_cast<double>(expected[gap + 1].onsetSeconds) + kGapTailSeconds;
        writePlot(out, data, "Level across the longest pause of the score", from, to);
    }
    std::ofstream out{directory + "/nv_close.txt"};
    std::vector<size_t> picks{0, 1, expected.size() / 2, gap, gap + 1, expected.size() - 1};
    std::ranges::sort(picks);
    picks.erase(std::unique(picks.begin(), picks.end()), picks.end());
    for (const size_t i : picks)
    {
        const double at = static_cast<double>(report.notes[i].expectedSeconds);
        writePlot(out, data,
                  "Note " + std::to_string(i + 1) + ", " + noteName(static_cast<double>(expected[i].midiNote)),
                  at - kCloseUpSeconds, at + kCloseUpSeconds);
    }
}
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cout << "usage: NoteVerifierExplore <recording.wav> <score file> <output directory>\n";
        return 1;
    }
    AudioFile<float> audio;
    if (!audio.load(argv[1]))
    {
        std::cout << "cannot read " << argv[1] << "\n";
        return 1;
    }
    const Score score = readScore(argv[2]);
    if (score.notes.empty())
    {
        std::cout << "no notes in " << argv[2] << "\n";
        return 1;
    }
    const auto rate = static_cast<float>(audio.getSampleRate());
    const auto mono = loadMono(audio);
    AbacDsp::NoteVerifierConfig config;
    config.windowSeconds = kWindowBeats * static_cast<float>(60.0 / score.bpm);
    const auto report = AbacDsp::NoteVerifier{rate, config}.verify(mono, score.notes);
    std::printf("%s: %zu expected notes, %.3f s, %.0f Hz\n", argv[1], score.notes.size(),
                static_cast<double>(mono.size()) / static_cast<double>(rate), static_cast<double>(rate));
    printReport(report, score.notes);
    writePlots(argv[3], report, score.notes, plotDataOf(report, levelEnvelope(mono, rate)),
               static_cast<double>(mono.size()) / static_cast<double>(rate));
    return 0;
}
