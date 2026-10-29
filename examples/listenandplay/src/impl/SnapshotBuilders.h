#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "DisplaySnapshots.h"
#include "NotePattern.h"
#include "TakeAnalysis.h"
#include "TakeSession.h"

namespace ListenPlayDisplay
{

[[nodiscard]] constexpr Phase phaseOf(const TakeSession::State state) noexcept
{
    switch (state)
    {
        case TakeSession::State::Idle:
            return Phase::Idle;
        case TakeSession::State::Listening:
            return Phase::Listening;
        case TakeSession::State::CountIn:
            return Phase::CountIn;
        case TakeSession::State::Capturing:
            return Phase::Capturing;
        case TakeSession::State::Analysing:
            return Phase::Analysing;
        case TakeSession::State::Review:
            return Phase::Review;
        case TakeSession::State::Replaying:
            return Phase::Replaying;
    }
    return Phase::Idle;
}

[[nodiscard]] inline bool nearlyEqual(const float a, const float b) noexcept
{
    return std::abs(a - b) < NotePattern::kEpsilonBeats;
}

[[nodiscard]] inline bool samePhrase(const NotePattern::Pattern& a, const NotePattern::Pattern& b) noexcept
{
    if (a.noteCount != b.noteCount || !nearlyEqual(a.pickupBeats, b.pickupBeats) || a.patternBars != b.patternBars ||
        a.beatsPerBar != b.beatsPerBar)
    {
        return false;
    }
    return std::equal(a.notes.begin(), a.notes.begin() + static_cast<std::ptrdiff_t>(a.noteCount), b.notes.begin(),
                      [](const NotePattern::Note& x, const NotePattern::Note& y)
                      {
                          return nearlyEqual(x.positionBeats, y.positionBeats) &&
                                 nearlyEqual(x.durationBeats, y.durationBeats) && x.pitch == y.pitch &&
                                 x.velocity == y.velocity;
                      });
}

[[nodiscard]] inline PianoRollSnapshot rollOfPhrase(const NotePattern::Pattern& pattern)
{
    PianoRollSnapshot roll;
    roll.pickupBeats = pattern.pickupBeats;
    roll.patternBeats = pattern.patternBeats();
    roll.beatsPerBar = pattern.beatsPerBar;
    roll.targetCount = pattern.noteCount;
    for (size_t i = 0; i < pattern.noteCount; ++i)
    {
        const auto& note = pattern.notes[i];
        roll.target[i] = {note.positionBeats, note.durationBeats, note.pitch, note.velocity, false};
    }
    return roll;
}

// Played notes on the grid of the phrase: the time model of the take is taken out, so a late start or another
// tempo does not shift them.
inline void addPlayedNotes(PianoRollSnapshot& roll, const TakeAnalysis::TakeResult& result)
{
    const auto& model = result.match.model;
    const double secondsPerBeat = 60.0 / static_cast<double>(result.setup.bpm);
    const auto gridBeats = [&](const AbacDsp::PlayedNote& note)
    {
        const double onset = (static_cast<double>(note.onsetSeconds) - model.offsetSeconds) / model.scale;
        const double length = static_cast<double>(note.offsetSeconds - note.onsetSeconds) / model.scale;
        return std::pair{
            static_cast<float>((onset - static_cast<double>(result.setup.downbeatSeconds)) / secondsPerBeat),
            static_cast<float>(length / secondsPerBeat)};
    };
    const auto add = [&roll](const RollPlayed& note)
    {
        if (roll.playedCount < roll.played.size())
        {
            roll.played[roll.playedCount++] = note;
        }
    };
    for (size_t i = 0; i < result.match.matched.size(); ++i)
    {
        const auto& matched = result.match.matched[i];
        const auto& note = result.played[matched.played];
        const auto [start, length] = gridBeats(note);
        const float midi = matched.pitched ? matched.playedMidi - static_cast<float>(result.match.octaveShift)
                                           : static_cast<float>(result.setup.pattern.notes[matched.expected].pitch);
        add({start, length, midi, matched.pitchRight ? result.score.timingCredit[i] : 0.f, matched.pitchRight, false});
    }
    for (const size_t index : result.match.extras)
    {
        const auto [start, length] = gridBeats(result.played[index]);
        add({start, length, result.played[index].midiNote, 0.f, false, true});
    }
    for (const size_t index : result.match.missed)
    {
        roll.target[index].missed = true;
    }
    roll.revealed = true;
}

[[nodiscard]] inline ScoreSnapshot scoreOfResult(const TakeAnalysis::TakeResult& result)
{
    const auto& score = result.score;
    ScoreSnapshot snapshot;
    snapshot.notes = score.notes;
    snapshot.rhythm = score.rhythm;
    snapshot.dynamics = score.dynamics;
    snapshot.overall = score.overall;
    snapshot.passScore = result.setup.pattern.scoring.passScore;
    snapshot.meanTimingMs = score.meanTimingMs;
    snapshot.timingSpreadMs = score.timingSpreadMs;
    snapshot.meanIntonationCents = score.meanIntonationCents;
    snapshot.tempoRatio = score.tempoRatio;
    snapshot.suggestedLatencyMs = score.suggestedLatencyMs;
    snapshot.expected = score.expectedCount;
    snapshot.matched = score.matchedCount;
    snapshot.missed = score.missedCount;
    snapshot.extras = score.extraCount;
    snapshot.octaveShift = score.octaveShift;
    snapshot.latencyKnown = score.latencyKnown;
    snapshot.clickOn = result.setup.clickOn;
    snapshot.passed = score.passed;
    snapshot.hasResult = true;
    return snapshot;
}

}
