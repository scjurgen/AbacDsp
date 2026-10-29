#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "Analysis/NoteSegmenter.h"
#include "NotePattern.h"

namespace NoteMatching
{

// What a take was played against. downbeatSeconds is where beat 0 of bar 1 lies on the time axis of the take.
struct TakeSetup
{
    NotePattern::Pattern pattern{};
    float bpm{100.f};
    float downbeatSeconds{0.f};
    bool clickOn{true};
    float latencyMs{0.f};
    bool scoreLength{false};
};

// Played time = offsetSeconds + scale * expected time. The scale is the played beat length over the expected one.
struct TimeModel
{
    double offsetSeconds{0.0};
    double scale{1.0};
};

// timingMs: played minus expected under the time model, positive = late. gridMs: played minus the nominal grid.
struct MatchedNote
{
    size_t expected{0};
    size_t played{0};
    float timingMs{0.f};
    float gridMs{0.f};
    float playedMidi{0.f};
    float pitchError{0.f};
    float levelDb{0.f};
    float lengthRatio{0.f};
    bool pitched{false};
    bool pitchRight{false};
};

struct MatchResult
{
    std::vector<MatchedNote> matched;
    std::vector<size_t> missed;
    std::vector<size_t> extras;
    TimeModel model;
    int octaveShift{0};
    float suggestedLatencyMs{0.f};
    bool latencyKnown{false};
};

inline constexpr double kMaxWindowSeconds{0.35};
inline constexpr double kMaxWindowBeats{0.6};
inline constexpr double kSearchWindowBeats{0.25};
inline constexpr double kMinScale{0.6};
inline constexpr double kMaxScale{1.6};
inline constexpr double kScaleStep{0.02};
inline constexpr double kMaxOffsetBeats{4.0};
inline constexpr size_t kRefinePasses{2};
inline constexpr float kPitchRightSemitones{0.5f};
inline constexpr float kOctaveSnapSemitones{3.f};
inline constexpr size_t kMinNotesForLatency{3};

namespace detail
{

struct Pair
{
    size_t expected{0};
    size_t played{0};
};

struct Times
{
    std::vector<double> expected;
    std::vector<double> played;
    double secondsPerBeat{0.6};
};

[[nodiscard]] inline Times timesOf(const TakeSetup& setup, const std::span<const AbacDsp::PlayedNote> played)
{
    Times times;
    times.secondsPerBeat = 60.0 / static_cast<double>(setup.bpm);
    const auto& pattern = setup.pattern;
    for (size_t i = 0; i < pattern.noteCount; ++i)
    {
        times.expected.push_back(static_cast<double>(setup.downbeatSeconds) +
                                 static_cast<double>(pattern.notes[i].positionBeats) * times.secondsPerBeat);
    }
    for (const auto& note : played)
    {
        times.played.push_back(static_cast<double>(note.onsetSeconds));
    }
    return times;
}

[[nodiscard]] inline double predicted(const TimeModel& model, const double expected) noexcept
{
    return model.offsetSeconds + model.scale * expected;
}

// Half the distance to the nearest neighbouring expected note, capped, so that one played note
// cannot answer for two expected ones.
[[nodiscard]] inline std::vector<double> windowsFor(const Times& times, const TimeModel& model)
{
    const double cap = std::min(kMaxWindowSeconds, kMaxWindowBeats * times.secondsPerBeat * model.scale);
    std::vector<double> windows(times.expected.size(), cap);
    for (size_t i = 0; i < windows.size(); ++i)
    {
        if (i > 0)
        {
            windows[i] = std::min(windows[i], 0.5 * model.scale * (times.expected[i] - times.expected[i - 1]));
        }
        if (i + 1 < windows.size())
        {
            windows[i] = std::min(windows[i], 0.5 * model.scale * (times.expected[i + 1] - times.expected[i]));
        }
    }
    return windows;
}

// Closest pairs first; each expected and each played note is used at most once.
[[nodiscard]] inline std::vector<Pair> pairNotes(const Times& times, const TimeModel& model,
                                                 const std::vector<double>& windows)
{
    struct Candidate
    {
        double distance;
        size_t expected;
        size_t played;
    };
    std::vector<Candidate> candidates;
    for (size_t i = 0; i < times.expected.size(); ++i)
    {
        for (size_t j = 0; j < times.played.size(); ++j)
        {
            const double distance = std::abs(times.played[j] - predicted(model, times.expected[i]));
            if (distance <= windows[i])
            {
                candidates.push_back({distance, i, j});
            }
        }
    }
    std::ranges::sort(candidates, {}, &Candidate::distance);
    std::vector<bool> expectedTaken(times.expected.size(), false);
    std::vector<bool> playedTaken(times.played.size(), false);
    std::vector<Pair> pairs;
    for (const auto& c : candidates)
    {
        if (!expectedTaken[c.expected] && !playedTaken[c.played])
        {
            expectedTaken[c.expected] = true;
            playedTaken[c.played] = true;
            pairs.push_back({c.expected, c.played});
        }
    }
    std::ranges::sort(pairs, {}, &Pair::expected);
    return pairs;
}

// How well a model explains the take: every pair adds up to one, less the closer it is to its window.
[[nodiscard]] inline double fitQuality(const Times& times, const TimeModel& model)
{
    const double searchWindow = kSearchWindowBeats * times.secondsPerBeat * model.scale;
    const std::vector<double> windows(times.expected.size(), searchWindow);
    double quality = 0.0;
    for (const auto& pair : pairNotes(times, model, windows))
    {
        quality +=
            1.0 - std::abs(times.played[pair.played] - predicted(model, times.expected[pair.expected])) / searchWindow;
    }
    return quality;
}

// Offset and scale from a least-squares line through the pairs; just an offset when they do not span time.
[[nodiscard]] inline TimeModel refineModel(const Times& times, const std::vector<Pair>& pairs, const TimeModel& start)
{
    if (pairs.size() < 2)
    {
        return start;
    }
    double meanE = 0.0;
    double meanP = 0.0;
    for (const auto& pair : pairs)
    {
        meanE += times.expected[pair.expected];
        meanP += times.played[pair.played];
    }
    meanE /= static_cast<double>(pairs.size());
    meanP /= static_cast<double>(pairs.size());
    double covariance = 0.0;
    double variance = 0.0;
    for (const auto& pair : pairs)
    {
        covariance += (times.expected[pair.expected] - meanE) * (times.played[pair.played] - meanP);
        variance += (times.expected[pair.expected] - meanE) * (times.expected[pair.expected] - meanE);
    }
    const double scale = variance > 0.0 ? std::clamp(covariance / variance, kMinScale, kMaxScale) : 1.0;
    return {meanP - scale * meanE, scale};
}

// Tries every offset that puts one played note on one expected note, for every scale.
[[nodiscard]] inline TimeModel searchModel(const Times& times)
{
    TimeModel best{};
    double bestQuality = -1.0;
    const double maxOffset = kMaxOffsetBeats * times.secondsPerBeat;
    for (double scale = kMinScale; scale <= kMaxScale + 1e-9; scale += kScaleStep)
    {
        for (const double expected : times.expected)
        {
            for (const double played : times.played)
            {
                const TimeModel candidate{played - scale * expected, scale};
                if (std::abs(candidate.offsetSeconds) > maxOffset)
                {
                    continue;
                }
                const double quality = fitQuality(times, candidate);
                const bool closerToNominal = std::abs(candidate.offsetSeconds) + std::abs(scale - 1.0) <
                                             std::abs(best.offsetSeconds) + std::abs(best.scale - 1.0);
                if (quality > bestQuality + 1e-9 || (quality > bestQuality - 1e-9 && closerToNominal))
                {
                    best = candidate;
                    bestQuality = quality;
                }
            }
        }
    }
    return best;
}

[[nodiscard]] inline double median(std::vector<double> values)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::ranges::sort(values);
    return values[values.size() / 2];
}

}

// The octave offset of the take, a multiple of 12 semitones, when the notes agree on one.
[[nodiscard]] inline int octaveShiftOf(const std::vector<double>& pitchDifferences)
{
    const double middle = detail::median(pitchDifferences);
    const double shift = 12.0 * std::round(middle / 12.0);
    return std::abs(middle - shift) <= static_cast<double>(kOctaveSnapSemitones) ? static_cast<int>(shift) : 0;
}

// Pairs every played note with at most one expected note and measures how it was played.
// Click on: the time model is the grid plus the latency. Click off: it is fitted to the take, so that
// a late start or another tempo does not count as an error.
[[nodiscard]] inline MatchResult matchNotes(const TakeSetup& setup, const std::span<const AbacDsp::PlayedNote> played)
{
    MatchResult result;
    const auto& pattern = setup.pattern;
    if (!pattern.active || setup.bpm <= 0.f || pattern.noteCount == 0)
    {
        result.extras.resize(played.size());
        std::ranges::generate(result.extras, [i = size_t{0}]() mutable { return i++; });
        return result;
    }
    const auto times = detail::timesOf(setup, played);
    const TimeModel grid{static_cast<double>(setup.latencyMs) * 0.001, 1.0};
    TimeModel model = grid;
    if (!setup.clickOn)
    {
        model = detail::searchModel(times);
        for (size_t pass = 0; pass < kRefinePasses; ++pass)
        {
            model =
                detail::refineModel(times, detail::pairNotes(times, model, detail::windowsFor(times, model)), model);
        }
    }
    result.model = model;
    const auto pairs = detail::pairNotes(times, model, detail::windowsFor(times, model));

    std::vector<double> pitchDifferences;
    std::vector<double> rawOffsetsMs;
    for (const auto& pair : pairs)
    {
        rawOffsetsMs.push_back((times.played[pair.played] - times.expected[pair.expected]) * 1000.0);
        if (played[pair.played].pitched())
        {
            pitchDifferences.push_back(static_cast<double>(played[pair.played].midiNote) -
                                       static_cast<double>(pattern.notes[pair.expected].pitch));
        }
    }
    result.octaveShift = pattern.scoring.octaveFree && !pitchDifferences.empty() ? octaveShiftOf(pitchDifferences) : 0;
    result.latencyKnown = setup.clickOn && pairs.size() >= kMinNotesForLatency;
    result.suggestedLatencyMs = static_cast<float>(detail::median(rawOffsetsMs));

    std::vector<bool> expectedUsed(pattern.noteCount, false);
    std::vector<bool> playedUsed(played.size(), false);
    for (const auto& pair : pairs)
    {
        const auto& note = played[pair.played];
        const auto& target = pattern.notes[pair.expected];
        MatchedNote matched;
        matched.expected = pair.expected;
        matched.played = pair.played;
        matched.timingMs = static_cast<float>(
            (times.played[pair.played] - detail::predicted(model, times.expected[pair.expected])) * 1000.0);
        matched.gridMs = static_cast<float>((times.played[pair.played] - times.expected[pair.expected]) * 1000.0);
        matched.pitched = note.pitched();
        matched.playedMidi = note.midiNote;
        matched.pitchError =
            matched.pitched ? note.midiNote - static_cast<float>(target.pitch) - static_cast<float>(result.octaveShift)
                            : 0.f;
        matched.pitchRight = matched.pitched && std::abs(matched.pitchError) < kPitchRightSemitones;
        matched.levelDb = note.attackRmsDb;
        const double expectedLength = static_cast<double>(target.durationBeats) * times.secondsPerBeat * model.scale;
        matched.lengthRatio = static_cast<float>(
            (static_cast<double>(note.offsetSeconds) - static_cast<double>(note.onsetSeconds)) / expectedLength);
        result.matched.push_back(matched);
        expectedUsed[pair.expected] = true;
        playedUsed[pair.played] = true;
    }
    for (size_t i = 0; i < expectedUsed.size(); ++i)
    {
        if (!expectedUsed[i])
        {
            result.missed.push_back(i);
        }
    }
    for (size_t j = 0; j < playedUsed.size(); ++j)
    {
        if (!playedUsed[j])
        {
            result.extras.push_back(j);
        }
    }
    return result;
}

}
