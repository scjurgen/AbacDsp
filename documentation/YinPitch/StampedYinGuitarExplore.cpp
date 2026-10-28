// Runs StampedYinPitchDetector and PitchNoteTracker over a guitar recording and compares the notes with its score.
// Environment: GUITAR_WAV (recording), GUITAR_SCORE (score file of NoteSegmenter/scores), EXPLORE_OUT_DIR (output).
// Prints a table and writes yin_guitar.txt in PyConPlot.py's format; see README.md.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "Analysis/PitchNoteTracker.h"
#include "Analysis/StampedYinPitchDetector.h"
#include "AudioFile.h"

namespace
{
using namespace AbacDsp;

struct ScoreNote
{
    double seconds;
    int midi;
};

struct Heard
{
    double seconds;
    int midi;
    float hz;
};

[[nodiscard]] std::vector<ScoreNote> readScore(const std::string& path)
{
    std::vector<ScoreNote> notes;
    std::ifstream in{path};
    std::string key;
    double bpm = 120.0;
    while (in >> key)
    {
        if (key == "bpm")
        {
            in >> bpm;
        }
        else if (key == "note")
        {
            double midi = 0.0;
            double beat = 0.0;
            double length = 0.0;
            in >> midi >> beat >> length;
            notes.push_back({beat * 60.0 / bpm, static_cast<int>(midi)});
        }
        else
        {
            in.ignore(1 << 16, '\n');
        }
    }
    return notes;
}

[[nodiscard]] std::vector<float> loadMono(const AudioFile<float>& audio)
{
    const auto frames = static_cast<size_t>(audio.getNumSamplesPerChannel());
    const auto channels = static_cast<size_t>(audio.getNumChannels());
    std::vector<float> mono(frames, 0.0f);
    for (size_t c = 0; c < channels; ++c)
    {
        for (size_t i = 0; i < frames; ++i)
        {
            mono[i] += audio.samples[c][i] / static_cast<float>(channels);
        }
    }
    return mono;
}

[[nodiscard]] std::vector<Heard> listen(const std::vector<float>& mono, const float rate, const size_t decimation)
{
    StampedYinPitchDetector detector(rate, 40.0f, 2000.0f, 50.0f, {decimation});
    PitchNoteTracker tracker;
    std::vector<Heard> heard;
    for (const float x : mono)
    {
        std::ignore = detector.step(x);
        if (!detector.hasNewPitch())
        {
            continue;
        }
        const auto event = tracker.update({detector.getCurrentPitch(), detector.getLastConfidence(),
                                           detector.getLastLevel(), detector.getLastWindowEnd()});
        if (event && !event->released)
        {
            heard.push_back({static_cast<double>(event->stamp) / static_cast<double>(rate), event->note, event->hz});
        }
    }
    return heard;
}

[[nodiscard]] const char* requireEnv(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
    {
        std::cerr << "set " << name << "\n";
        std::exit(1);
    }
    return value;
}

void report(const std::vector<ScoreNote>& score, const std::vector<Heard>& heard, std::ofstream& out)
{
    std::printf(" #  expected  heard  expected s  heard s  after ms\n");
    int right = 0;
    out << "@New plot: title=\"guitar scale: the score and the notes heard (time of the report)\"\n#score\n";
    for (const ScoreNote& note : score)
    {
        out << note.seconds << " " << note.midi << "\n" << note.seconds + 0.4 << " " << note.midi << "\n";
    }
    out << "#heard\n";
    for (size_t i = 0; i < score.size(); ++i)
    {
        const auto match =
            std::ranges::find_if(heard, [&](const Heard& h)
                                 { return h.seconds >= score[i].seconds && h.seconds < score[i].seconds + 0.45; });
        if (match == heard.end())
        {
            std::printf("%2zu  %8d      -  %10.3f        -         -\n", i + 1, score[i].midi, score[i].seconds);
            continue;
        }
        right += match->midi == score[i].midi ? 1 : 0;
        std::printf("%2zu  %8d  %5d  %10.3f  %7.3f  %8.0f\n", i + 1, score[i].midi, match->midi, score[i].seconds,
                    match->seconds, (match->seconds - score[i].seconds) * 1000.0);
        out << match->seconds << " " << match->midi << "\n";
    }
    std::printf("%d of %zu expected notes heard with the right number; %zu events in all\n", right, score.size(),
                heard.size());
}
}

int main()
{
    AudioFile<float> audio;
    if (!audio.load(requireEnv("GUITAR_WAV")))
    {
        std::cerr << "cannot read the recording\n";
        return 1;
    }
    const auto score = readScore(requireEnv("GUITAR_SCORE"));
    const char* folder = std::getenv("EXPLORE_OUT_DIR");
    std::ofstream out(std::string(folder != nullptr ? folder : ".") + "/yin_guitar.txt");
    const auto mono = loadMono(audio);
    const auto heard = listen(mono, static_cast<float>(audio.getSampleRate()), 4);
    report(score, heard, out);
    return 0;
}
