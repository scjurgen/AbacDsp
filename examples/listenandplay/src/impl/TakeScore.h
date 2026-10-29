#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "KsInstrument.h"
#include "NoteMatcher.h"
#include "NotePattern.h"

namespace NoteScoring
{

inline constexpr float kLengthGoodOctaves{0.3f};
inline constexpr float kLengthZeroOctaves{1.f};
inline constexpr float kLengthShare{0.4f};

struct Weights
{
    float notes{0.f};
    float rhythm{0.f};
    float dynamics{0.f};
};

// Indexed by NotePattern::Focus.
inline constexpr auto kWeights = std::to_array<Weights>({
    {0.7f, 0.3f, 0.f},
    {0.2f, 0.8f, 0.f},
    {0.2f, 0.2f, 0.6f},
    {0.4f, 0.3f, 0.3f},
});

[[nodiscard]] constexpr Weights weightsOf(const NotePattern::Focus focus) noexcept
{
    return kWeights[static_cast<size_t>(focus)];
}

// Full credit up to the good value, none from the zero value on, linear in between.
[[nodiscard]] constexpr float bandCredit(const float error, const float good, const float zero) noexcept
{
    const float magnitude = error < 0.f ? -error : error;
    if (magnitude <= good)
    {
        return 1.f;
    }
    return magnitude >= zero ? 0.f : (zero - magnitude) / (zero - good);
}

// Scores run from 0 to 100. Per-note vectors run parallel to MatchResult::matched.
struct TakeScore
{
    float notes{0.f};
    float rhythm{0.f};
    float dynamics{0.f};
    float overall{0.f};
    bool passed{false};
    size_t expectedCount{0};
    size_t matchedCount{0};
    size_t missedCount{0};
    size_t extraCount{0};
    float meanTimingMs{0.f};
    float timingSpreadMs{0.f};
    float meanIntonationCents{0.f};
    float intonationSpreadCents{0.f};
    float tempoRatio{1.f};
    int octaveShift{0};
    float suggestedLatencyMs{0.f};
    bool latencyKnown{false};
    std::vector<float> timingCredit;
    std::vector<float> dynamicsCredit;
    std::vector<float> levelErrorDb;
};

namespace detail
{

struct Spread
{
    float mean{0.f};
    float deviation{0.f};
};

[[nodiscard]] inline Spread spreadOf(const std::vector<float>& values)
{
    if (values.empty())
    {
        return {};
    }
    double sum = 0.0;
    for (const float v : values)
    {
        sum += static_cast<double>(v);
    }
    const double mean = sum / static_cast<double>(values.size());
    double squares = 0.0;
    for (const float v : values)
    {
        squares += (static_cast<double>(v) - mean) * (static_cast<double>(v) - mean);
    }
    return {static_cast<float>(mean), static_cast<float>(std::sqrt(squares / static_cast<double>(values.size())))};
}

// Level of each matched note against the commanded level, with the take's overall loudness removed.
[[nodiscard]] inline std::vector<float> relativeLevelErrors(const NotePattern::Pattern& pattern,
                                                            const NoteMatching::MatchResult& match)
{
    std::vector<double> raw;
    for (const auto& note : match.matched)
    {
        raw.push_back(static_cast<double>(note.levelDb) -
                      static_cast<double>(KsInstrument::velocityDb(pattern.notes[note.expected].velocity)));
    }
    const double offset = NoteMatching::detail::median(raw);
    std::vector<float> errors;
    for (const double value : raw)
    {
        errors.push_back(static_cast<float>(value - offset));
    }
    return errors;
}

[[nodiscard]] inline float timingCreditOf(const NoteMatching::MatchedNote& note, const NotePattern::Scoring& scoring,
                                          const bool scoreLength)
{
    const float onset = bandCredit(note.timingMs, scoring.timingGoodMs, scoring.timingZeroMs);
    if (!scoreLength || note.lengthRatio <= 0.f)
    {
        return onset;
    }
    const float length = bandCredit(std::log2(note.lengthRatio), kLengthGoodOctaves, kLengthZeroOctaves);
    return (1.f - kLengthShare) * onset + kLengthShare * length;
}

}

[[nodiscard]] inline TakeScore scoreTake(const NoteMatching::TakeSetup& setup, const NoteMatching::MatchResult& match)
{
    const auto& pattern = setup.pattern;
    const auto& scoring = pattern.scoring;
    TakeScore score;
    score.expectedCount = pattern.noteCount;
    score.matchedCount = match.matched.size();
    score.missedCount = match.missed.size();
    score.extraCount = match.extras.size();
    score.octaveShift = match.octaveShift;
    score.suggestedLatencyMs = match.suggestedLatencyMs;
    score.latencyKnown = match.latencyKnown;
    score.tempoRatio = static_cast<float>(1.0 / match.model.scale);
    if (pattern.noteCount == 0)
    {
        return score;
    }
    const auto denominator = static_cast<float>(pattern.noteCount + match.extras.size());
    score.levelErrorDb = detail::relativeLevelErrors(pattern, match);
    std::vector<float> timings;
    std::vector<float> cents;
    float pitchCredit = 0.f;
    float rhythmCredit = 0.f;
    float dynamicsCredit = 0.f;
    for (size_t i = 0; i < match.matched.size(); ++i)
    {
        const auto& note = match.matched[i];
        score.timingCredit.push_back(detail::timingCreditOf(note, scoring, setup.scoreLength));
        score.dynamicsCredit.push_back(
            bandCredit(score.levelErrorDb[i], scoring.dynamicsGoodDb, scoring.dynamicsZeroDb));
        rhythmCredit += score.timingCredit.back();
        dynamicsCredit += score.dynamicsCredit.back();
        pitchCredit += note.pitchRight ? 1.f : 0.f;
        timings.push_back(note.timingMs);
        if (note.pitchRight)
        {
            cents.push_back(note.pitchError * 100.f);
        }
    }
    score.notes = 100.f * pitchCredit / denominator;
    score.rhythm = 100.f * rhythmCredit / denominator;
    score.dynamics = 100.f * dynamicsCredit / static_cast<float>(pattern.noteCount);
    const auto timing = detail::spreadOf(timings);
    score.meanTimingMs = timing.mean;
    score.timingSpreadMs = timing.deviation;
    const auto intonation = detail::spreadOf(cents);
    score.meanIntonationCents = intonation.mean;
    score.intonationSpreadCents = intonation.deviation;
    const Weights weights = weightsOf(pattern.focus);
    score.overall = weights.notes * score.notes + weights.rhythm * score.rhythm + weights.dynamics * score.dynamics;
    score.passed = score.overall >= scoring.passScore;
    return score;
}

}
