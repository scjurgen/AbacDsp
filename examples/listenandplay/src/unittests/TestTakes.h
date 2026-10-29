#pragma once

#include <cstddef>
#include <vector>

#include "Analysis/NoteSegmenter.h"
#include "impl/KsInstrument.h"
#include "impl/NoteMatcher.h"
#include "impl/NotePattern.h"

namespace TestTakes
{

constexpr float kBpm{100.f};
constexpr float kSecondsPerBeat{0.6f};

inline void addNote(NotePattern::Pattern& pattern, const float position, const float duration, const uint8_t pitch,
                    const uint8_t velocity = 90)
{
    pattern.notes[pattern.noteCount++] = NotePattern::Note{position, duration, pitch, velocity};
    pattern.active = true;
}

// Eight notes with rests, varied velocities, two bars.
inline NotePattern::Pattern makePattern(const NotePattern::Focus focus = NotePattern::Focus::Combined)
{
    NotePattern::Pattern pattern{};
    pattern.patternBars = 2;
    pattern.bpm = kBpm;
    pattern.focus = focus;
    const float positions[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.5f, 7.f};
    const float lengths[] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.5f, 1.f};
    const uint8_t pitches[] = {60, 62, 64, 65, 67, 65, 64, 60};
    const uint8_t velocities[] = {70, 90, 110, 80, 100, 60, 90, 120};
    for (size_t i = 0; i < 8; ++i)
    {
        addNote(pattern, positions[i], lengths[i], pitches[i], velocities[i]);
    }
    return pattern;
}

inline NoteMatching::TakeSetup makeSetup(const NotePattern::Pattern& pattern, const bool clickOn = true)
{
    NoteMatching::TakeSetup setup;
    setup.pattern = pattern;
    setup.bpm = kBpm;
    setup.downbeatSeconds = 1.f;
    setup.clickOn = clickOn;
    return setup;
}

// What a perfect player would have been heard doing.
inline std::vector<AbacDsp::PlayedNote> perfectTake(const NoteMatching::TakeSetup& setup)
{
    std::vector<AbacDsp::PlayedNote> played;
    for (size_t i = 0; i < setup.pattern.noteCount; ++i)
    {
        const auto& note = setup.pattern.notes[i];
        AbacDsp::PlayedNote heard;
        heard.onsetSeconds = setup.downbeatSeconds + note.positionBeats * kSecondsPerBeat;
        heard.offsetSeconds = heard.onsetSeconds + note.durationBeats * kSecondsPerBeat;
        heard.midiNote = static_cast<float>(note.pitch);
        heard.pitchConfidence = 0.9f;
        heard.attackRmsDb = KsInstrument::kReferenceLevelDb + KsInstrument::velocityDb(note.velocity);
        heard.peakDb = heard.attackRmsDb + 8.f;
        played.push_back(heard);
    }
    return played;
}

}
