// Verification data for Analysis/NoteSegmenter.h: plucked-string takes with known notes are rendered,
// segmented and compared with what was played. Output is PyConPlot text, see README.md.
// Usage: NoteSegmenterExplore <phrase.txt> <errors.txt> <noise.txt> <ringing.txt>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "Analysis/NoteSegmenter.h"
#include "Generators/KarplusStrongVoice.h"

namespace
{
using namespace AbacDsp;
constexpr float kSampleRate{48000.f};
using Voice = KarplusStrongVoice<10000>;

struct NoteSpec
{
    double startSeconds{0.0};
    double durationSeconds{0.4};
    int midi{60};
    double levelDb{0.0};
    bool damped{true};
};

void configure(Voice& voice, const unsigned seed)
{
    voice.seed(seed);
    voice.setPluckType(PluckType::WhiteStatic);
    voice.setDecayByTime(1500.f);
    voice.setDecayOctaveFactor(0.5f);
    voice.setDamper(0.3f);
    voice.setFilterCutoffSemitones(24.f);
    voice.setKeyTracking(1.f);
    voice.setFilterResonance(0.1f);
    voice.setFilterEnvelope(1.f, 1.f, 1.f);
}

struct Event
{
    size_t sample{0};
    bool on{true};
    size_t voice{0};
    float note{0.f};
    float gain{0.f};
};

std::vector<Event> eventsFor(const std::vector<NoteSpec>& notes, const size_t voiceCount)
{
    std::vector<Event> events;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& n = notes[i];
        const auto on = static_cast<size_t>(std::llround(n.startSeconds * kSampleRate));
        const auto end = static_cast<size_t>(std::llround((n.startSeconds + n.durationSeconds) * kSampleRate));
        const double gap = std::min(0.025 * kSampleRate, 0.25 * static_cast<double>(end - on));
        events.push_back({on, true, i % voiceCount, static_cast<float>(n.midi),
                          static_cast<float>(std::pow(10.0, n.levelDb / 20.0))});
        if (n.damped)
        {
            events.push_back({end - static_cast<size_t>(gap), false, i % voiceCount, 0.f, 0.f});
        }
    }
    std::ranges::stable_sort(events, {}, &Event::sample);
    return events;
}

std::vector<float> renderTake(const std::vector<NoteSpec>& notes, const double seconds, const double noiseDb,
                              const unsigned noiseSeed)
{
    constexpr size_t kVoices{4};
    std::vector<Voice> voices;
    for (size_t i = 0; i < kVoices; ++i)
    {
        voices.emplace_back(kSampleRate);
        configure(voices.back(), static_cast<unsigned>(i + 1));
    }
    const auto events = eventsFor(notes, kVoices);
    std::vector<float> out(static_cast<size_t>(seconds * kSampleRate));
    std::mt19937 rng{noiseSeed};
    std::normal_distribution<float> noise{0.f, static_cast<float>(std::pow(10.0, noiseDb / 20.0))};
    size_t next = 0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        for (; next < events.size() && events[next].sample <= i; ++next)
        {
            auto& voice = voices[events[next].voice];
            if (events[next].on)
            {
                voice.trigger(events[next].note, events[next].gain);
            }
            else
            {
                voice.scheduleExcitation({0.f, 8.f, ExcitationType::Mute, 1.f, std::nullopt});
            }
        }
        float sum = 0.f;
        for (auto& voice : voices)
        {
            sum += voice.step();
        }
        out[i] = sum + (noiseDb > -200.0 ? noise(rng) : 0.f);
    }
    return out;
}

NoteSegmenterConfig defaultConfig()
{
    NoteSegmenterConfig config;
    config.minHz = 60.f;
    config.maxHz = 1400.f;
    return config;
}

std::vector<PlayedNote> segment(const std::vector<float>& audio)
{
    return NoteSegmenter{kSampleRate, defaultConfig()}.analyze(audio);
}

struct Match
{
    const NoteSpec* truth{nullptr};
    const PlayedNote* found{nullptr};
};

// Pairs every played note with the unused detected note nearest in onset time, within 50 ms.
std::vector<Match> matchNotes(const std::vector<NoteSpec>& truth, const std::vector<PlayedNote>& found)
{
    std::vector<Match> matches;
    std::vector<bool> taken(found.size(), false);
    for (const auto& note : truth)
    {
        Match match{&note, nullptr};
        double best = 0.05;
        size_t bestIndex = 0;
        for (size_t i = 0; i < found.size(); ++i)
        {
            const double distance = std::abs(static_cast<double>(found[i].onsetSeconds) - note.startSeconds);
            if (!taken[i] && distance < best)
            {
                best = distance;
                bestIndex = i;
                match.found = &found[i];
            }
        }
        if (match.found != nullptr)
        {
            taken[bestIndex] = true;
        }
        matches.push_back(match);
    }
    return matches;
}

double median(std::vector<double> values)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::ranges::sort(values);
    return values[values.size() / 2];
}

std::vector<NoteSpec> randomNotes(std::mt19937& rng, const size_t count)
{
    std::vector<NoteSpec> notes;
    double start = 0.3;
    for (size_t i = 0; i < count; ++i)
    {
        const double interval = std::uniform_real_distribution<double>{0.25, 0.6}(rng);
        const double length = interval * std::uniform_real_distribution<double>{0.5, 1.0}(rng);
        notes.push_back({start, length, std::uniform_int_distribution<int>{40, 84}(rng),
                         -std::uniform_real_distribution<double>{0.0, 30.0}(rng), true});
        start += interval;
    }
    return notes;
}

void writeHistogram(std::ofstream& out, const std::string& name, const std::vector<double>& values, const double low,
                    const double high, const size_t bins)
{
    std::vector<double> counts(bins, 0.0);
    for (const double v : values)
    {
        const double position = (v - low) / (high - low) * static_cast<double>(bins);
        if (position >= 0.0 && position < static_cast<double>(bins))
        {
            counts[static_cast<size_t>(position)] += 1.0;
        }
    }
    out << "#" << name << "\n";
    for (size_t b = 0; b < bins; ++b)
    {
        const double centre = low + (static_cast<double>(b) + 0.5) * (high - low) / static_cast<double>(bins);
        out << centre << " " << counts[b] / static_cast<double>(values.size()) * 100.0 << "\n";
    }
}

void writeSpikes(std::ofstream& out, const std::string& name, const std::vector<double>& times, const double height)
{
    out << "#" << name << "\n";
    for (const double t : times)
    {
        out << t - 0.0005 << " 0\n" << t << " " << height << "\n" << t + 0.0005 << " 0\n";
    }
}

void writePhrase(const std::string& path)
{
    const std::vector<NoteSpec> notes{{0.30, 0.40, 45, -6.0},  {0.80, 0.30, 52, -12.0}, {1.20, 0.45, 57, 0.0},
                                      {1.80, 0.20, 64, -18.0}, {2.10, 0.20, 62, -9.0},  {2.40, 0.50, 59, -3.0},
                                      {3.10, 0.35, 66, -15.0}, {3.60, 0.40, 71, -6.0}};
    const auto audio = renderTake(notes, 4.4, -80.0, 1);
    const auto found = segment(audio);

    std::ofstream out(path);
    out << "@New plot: title=\"Plucked phrase, played note starts and detected onsets\" labelx=\"time (s)\" "
           "labely=\"amplitude\"\n";
    constexpr size_t kBlock{96};
    for (const bool upper : {true, false})
    {
        out << (upper ? "#waveform, upper\n" : "#waveform, lower\n");
        for (size_t i = 0; i + kBlock <= audio.size(); i += kBlock)
        {
            const auto block = std::span<const float>{audio}.subspan(i, kBlock);
            out << static_cast<double>(i) / kSampleRate << " "
                << (upper ? *std::ranges::max_element(block) : *std::ranges::min_element(block)) << "\n";
        }
    }
    std::vector<double> playedTimes;
    std::vector<double> foundTimes;
    for (const auto& note : notes)
    {
        playedTimes.push_back(note.startSeconds);
    }
    for (const auto& note : found)
    {
        foundTimes.push_back(static_cast<double>(note.onsetSeconds));
    }
    writeSpikes(out, "played note start", playedTimes, 0.30);
    writeSpikes(out, "detected onset", foundTimes, 0.22);

    out << "\n@New plot: title=\"Pitch of the same phrase\" labelx=\"time (s)\" labely=\"MIDI note\"\n#detected\n";
    for (const auto& note : found)
    {
        out << note.onsetSeconds << " " << note.midiNote << "\n" << note.offsetSeconds << " " << note.midiNote << "\n";
    }
    out << "#played\n";
    for (const auto& note : notes)
    {
        out << note.startSeconds << " " << note.midi << "\n"
            << note.startSeconds + note.durationSeconds << " " << note.midi << "\n";
    }
}

struct ErrorSets
{
    std::vector<double> onsetMs;
    std::vector<double> pitchCents;
    std::vector<double> levelDb;
    size_t played{0};
    size_t missed{0};
    size_t extra{0};
};

// The level of a note alone at 0 dB is the reference, so the pitch-dependent level of the string drops out.
double levelError(const Match& match)
{
    const std::vector<NoteSpec> alone{{0.3, 0.45, match.truth->midi, 0.0}};
    const auto reference = segment(renderTake(alone, 1.2, -90.0, 1));
    return reference.size() == 1
               ? static_cast<double>(match.found->attackRmsDb - reference[0].attackRmsDb) - match.truth->levelDb
               : 0.0;
}

ErrorSets collectErrors()
{
    std::mt19937 rng{7};
    ErrorSets sets;
    constexpr size_t kTakes{150};
    for (size_t take = 0; take < kTakes; ++take)
    {
        const auto notes = randomNotes(rng, 8);
        const auto found = segment(renderTake(notes, notes.back().startSeconds + 1.2, -90.0, 1));
        size_t paired = 0;
        for (const auto& match : matchNotes(notes, found))
        {
            ++sets.played;
            if (match.found == nullptr)
            {
                ++sets.missed;
                continue;
            }
            ++paired;
            sets.onsetMs.push_back((static_cast<double>(match.found->onsetSeconds) - match.truth->startSeconds) *
                                   1000.0);
            sets.pitchCents.push_back((static_cast<double>(match.found->midiNote) - match.truth->midi) * 100.0);
            sets.levelDb.push_back(levelError(match));
        }
        sets.extra += found.size() - paired;
    }
    return sets;
}

void writeErrors(const std::string& path)
{
    const auto sets = collectErrors();
    std::ofstream out(path);
    out << "@New plot: title=\"Onset error, " << sets.onsetMs.size()
        << " plucked notes\" labelx=\"detected - played (ms)\" labely=\"share of notes (%)\"\n";
    writeHistogram(out, "onset error", sets.onsetMs, -6.0, 6.0, 96);
    out << "\n@New plot: title=\"Pitch error\" labelx=\"detected - played (cents)\" labely=\"share of notes (%)\"\n";
    writeHistogram(out, "pitch error", sets.pitchCents, -20.0, 20.0, 80);
    out << "\n@New plot: title=\"Level error, attack RMS against the commanded level\" "
           "labelx=\"measured - commanded (dB)\" labely=\"share of notes (%)\"\n";
    writeHistogram(out, "level error", sets.levelDb, -3.0, 3.0, 80);
    std::vector<double> absolute;
    std::ranges::transform(sets.onsetMs, std::back_inserter(absolute), [](const double v) { return std::abs(v); });
    std::cout << "errors: played " << sets.played << ", missed " << sets.missed << ", extra " << sets.extra
              << ", median |onset| " << median(absolute) << " ms\n";
}

struct NoiseRow
{
    int snr{0};
    double foundPercent{0.0};
    double pitchPercent{0.0};
    double medianOnsetMs{0.0};
};

// Level 0 dB notes have a first-100-ms RMS of about -24 dB, which the noise level is set against.
NoiseRow noiseRow(const int snr)
{
    constexpr size_t kTakes{20};
    std::mt19937 rng{11};
    size_t played = 0;
    size_t hits = 0;
    size_t pitchHits = 0;
    std::vector<double> errors;
    for (size_t take = 0; take < kTakes; ++take)
    {
        auto notes = randomNotes(rng, 8);
        for (auto& note : notes)
        {
            note.levelDb = 0.0;
        }
        const auto audio =
            renderTake(notes, notes.back().startSeconds + 1.2, -24.0 - snr, static_cast<unsigned>(take + 1));
        const auto result = segment(audio);
        for (const auto& match : matchNotes(notes, result))
        {
            ++played;
            if (match.found != nullptr)
            {
                ++hits;
                errors.push_back(
                    std::abs((static_cast<double>(match.found->onsetSeconds) - match.truth->startSeconds) * 1000.0));
                pitchHits += std::abs(static_cast<double>(match.found->midiNote) - match.truth->midi) < 0.5 ? 1 : 0;
            }
        }
    }
    return {snr, 100.0 * static_cast<double>(hits) / static_cast<double>(played),
            100.0 * static_cast<double>(pitchHits) / static_cast<double>(played), median(errors)};
}

void writeNoise(const std::string& path)
{
    std::vector<NoiseRow> rows;
    for (int snr = 40; snr >= 0; snr -= 2)
    {
        rows.push_back(noiseRow(snr));
    }
    std::ofstream out(path);
    out << "@New plot: title=\"Notes found and pitch within 50 cents, white noise\" "
           "labelx=\"signal to noise ratio (dB)\" labely=\"share of played notes (%)\" miny=0 maxy=105\n#note found\n";
    for (const auto& row : rows)
    {
        out << row.snr << " " << row.foundPercent << "\n";
    }
    out << "#pitch right\n";
    for (const auto& row : rows)
    {
        out << row.snr << " " << row.pitchPercent << "\n";
    }
    out << "\n@New plot: title=\"Median onset error\" labelx=\"signal to noise ratio (dB)\" "
           "labely=\"|detected - played| (ms)\"\n#median onset error\n";
    for (const auto& row : rows)
    {
        out << row.snr << " " << row.medianOnsetMs << "\n";
    }
}

// Percentage of 12 pitches for which the second, undamped note is found.
double ringingHitPercent(const double interval, const double relativeDb)
{
    constexpr int kPitches{12};
    size_t hits = 0;
    for (int p = 0; p < kPitches; ++p)
    {
        const std::vector<NoteSpec> notes{{0.3, 2.0, 45 + 3 * p, -6.0, false},
                                          {0.3 + interval, 1.0, 45 + 3 * p, -6.0 + relativeDb, false}};
        for (const auto& note : segment(renderTake(notes, 2.5, -90.0, 1)))
        {
            hits += std::abs(static_cast<double>(note.onsetSeconds) - (0.3 + interval)) < 0.03 ? 1 : 0;
        }
    }
    return 100.0 * static_cast<double>(hits) / kPitches;
}

void writeRinging(const std::string& path)
{
    std::ofstream out(path);
    out << "@New plot: title=\"Second note on the same pitch while the first still rings\" "
           "labelx=\"level of the second note against the first (dB)\" labely=\"second note found (%)\" miny=0 "
           "maxy=105\n";
    for (const double interval : {0.125, 0.25, 0.5})
    {
        out << "#" << interval * 1000.0 << " ms apart\n";
        for (double relative = -12.0; relative <= 12.0; relative += 1.0)
        {
            out << relative << " " << ringingHitPercent(interval, relative) << "\n";
        }
    }
}
}

int main(const int argc, const char* const argv[])
{
    if (argc != 5)
    {
        std::cerr << "usage: NoteSegmenterExplore <phrase.txt> <errors.txt> <noise.txt> <ringing.txt>\n";
        return 1;
    }
    writePhrase(argv[1]);
    writeErrors(argv[2]);
    writeNoise(argv[3]);
    writeRinging(argv[4]);
    return 0;
}
