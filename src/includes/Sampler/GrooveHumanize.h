#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Sampler/GrooveMidiFile.h"
#include "Sampler/GrooveNoteMap.h"
#include "Sampler/GrooveTiming.h"

namespace AbacDsp
{

/// @ingroup sampler
/// @brief The six drum-role buckets voice-role bias rules key off, collapsed
/// from GrooveNoteMap's ~40 GrooveTag values.
enum class GrooveVoice : uint8_t
{
    Kick,
    Snare,
    Hihat,
    Cymbal,
    Tom,
    Other,
};

using VoiceForGrooveNoteFn = GrooveVoice (*)(uint8_t note);

// Default voice classification: walks a note's tags most-specific-first (see
// GrooveNoteMap.h) and returns the first bucket it belongs to.
[[nodiscard]] inline GrooveVoice voiceForGrooveNote(const uint8_t note) noexcept
{
    for (const GrooveTag tag : tagsForGrooveNote(note))
    {
        switch (tag)
        {
            case GrooveTag::Kick:
                return GrooveVoice::Kick;
            case GrooveTag::Snare:
            case GrooveTag::SnareRoll:
            case GrooveTag::SnareAlt:
            case GrooveTag::Rimshot:
            case GrooveTag::Sidestick:
                return GrooveVoice::Snare;
            case GrooveTag::Hihat:
            case GrooveTag::HihatClosed:
            case GrooveTag::HihatOpen:
            case GrooveTag::HihatOpenTip:
            case GrooveTag::HihatClosedPedal:
            case GrooveTag::HihatClosedEdge:
            case GrooveTag::HihatOpenPedal:
            case GrooveTag::HihatOpen1:
            case GrooveTag::HihatOpen2:
            case GrooveTag::HihatOpen3:
            case GrooveTag::HihatStep:
            case GrooveTag::HihatHalfOpen:
            case GrooveTag::HihatStopped:
            case GrooveTag::HihatSoftStep:
            case GrooveTag::HihatGhost:
                return GrooveVoice::Hihat;
            case GrooveTag::Cymbal:
            case GrooveTag::Crash:
            case GrooveTag::CrashStopped:
            case GrooveTag::CrashLong:
            case GrooveTag::Ride:
            case GrooveTag::RideBell:
            case GrooveTag::China:
                return GrooveVoice::Cymbal;
            case GrooveTag::Tom:
            case GrooveTag::TomLow:
            case GrooveTag::Tom1:
            case GrooveTag::Tom2:
            case GrooveTag::Tom3:
            case GrooveTag::TomLeft:
            case GrooveTag::Timbale:
            case GrooveTag::Timbale1:
            case GrooveTag::Timbale2:
            case GrooveTag::Timbale3:
            case GrooveTag::Timbale4:
            case GrooveTag::TimbaleDamped:
                return GrooveVoice::Tom;
            case GrooveTag::None:
            case GrooveTag::Woodblock:
            case GrooveTag::Count:
            default:
                break;
        }
    }
    return GrooveVoice::Other;
}

/// @ingroup sampler
/// @brief One source note plus everything voice-role humanization needs to
/// know about it - grid geometry, role classification, and fill-bar status.
/// Doubles as the diagnostics record (see toCsv()).
struct GrooveAnalysisNote
{
    uint32_t sourceTick{0};
    uint8_t note{0};
    uint8_t velocity{0};
    GrooveVoice voice{GrooveVoice::Other};
    uint32_t gridTick{0};
    uint32_t patternPosition{0};
    GridStrength strength{GridStrength::Weak};
    bool isBackbeat{false};
    bool isGhost{false};
    bool isFillBar{false};
    bool isFillLanding{false};
};

/// @ingroup sampler
/// @brief One note's humanized output: absolute tick and velocity after
/// applyHumanize() (see its own doc comment for the formula).
struct HumanizedNote
{
    uint32_t tick{0};
    uint8_t note{0};
    uint8_t velocity{0};
};

namespace detail
{

// A snare hit at a weak position, or below ~60% of the file's own median
// snare velocity, reads as a ghost note.
inline void markGhostNotes(std::vector<GrooveAnalysisNote>& notes)
{
    std::vector<uint8_t> snareVelocities;
    for (const auto& note : notes)
    {
        if (note.voice == GrooveVoice::Snare)
        {
            snareVelocities.push_back(note.velocity);
        }
    }
    if (snareVelocities.empty())
    {
        return;
    }
    std::ranges::sort(snareVelocities);
    const uint8_t medianVelocity = snareVelocities[snareVelocities.size() / 2];
    const auto ghostThreshold = static_cast<uint8_t>(std::lround(static_cast<float>(medianVelocity) * 0.6f));
    for (auto& note : notes)
    {
        if (note.voice != GrooveVoice::Snare)
        {
            continue;
        }
        note.isGhost = note.strength == GridStrength::Weak || note.velocity < ghostThreshold;
    }
}

using Cell = std::pair<GrooveVoice, uint32_t>;

// Flags a bar as a fill when most of its hits land on (voice, patternPosition)
// cells fewer than half the file's bars ever use; the next bar's earliest
// hits are flagged as the landing.
inline void markFillBars(std::vector<GrooveAnalysisNote>& notes,
                         const std::vector<MidiTimeSignatureEvent>& timeSignatures, const uint16_t ticksPerQuarterNote)
{
    if (notes.empty())
    {
        return;
    }
    std::map<uint32_t, std::vector<size_t>> notesByBar;
    std::vector<uint32_t> barIndices(notes.size());
    for (size_t i = 0; i < notes.size(); ++i)
    {
        barIndices[i] = barIndexForTick(timeSignatures, ticksPerQuarterNote, notes[i].sourceTick);
        notesByBar[barIndices[i]].push_back(i);
    }

    std::map<Cell, size_t> barsUsingCell;
    for (const auto& [barIndex, indices] : notesByBar)
    {
        std::set<Cell> cellsThisBar;
        for (const size_t i : indices)
        {
            cellsThisBar.insert({notes[i].voice, notes[i].patternPosition});
        }
        for (const auto& cell : cellsThisBar)
        {
            ++barsUsingCell[cell];
        }
    }

    const size_t barCount = notesByBar.size();
    std::set<uint32_t> fillBars;
    for (const auto& [barIndex, indices] : notesByBar)
    {
        size_t rareHits = 0;
        for (const size_t i : indices)
        {
            const size_t usageCount = barsUsingCell.at({notes[i].voice, notes[i].patternPosition});
            if (usageCount * 2 <= barCount)
            {
                ++rareHits;
            }
        }
        if (rareHits * 2 > indices.size())
        {
            fillBars.insert(barIndex);
        }
    }

    for (size_t i = 0; i < notes.size(); ++i)
    {
        notes[i].isFillBar = fillBars.contains(barIndices[i]);
    }
    for (const uint32_t fillBarIndex : fillBars)
    {
        const auto it = notesByBar.find(fillBarIndex + 1);
        if (it == notesByBar.end())
        {
            continue;
        }
        uint32_t minTick = notes[it->second.front()].sourceTick;
        for (const size_t i : it->second)
        {
            minTick = std::min(minTick, notes[i].sourceTick);
        }
        for (const size_t i : it->second)
        {
            notes[i].isFillLanding = notes[i].sourceTick == minTick;
        }
    }
}

struct RoleSensitivity
{
    float pushFraction{0.f}; // fraction of one grid step, at push = +-1
};

// The per-voice-role push sensitivity table (see groove-aware-cleanup.md's
// Design section) - a fixed rule set, not a learned statistic.
[[nodiscard]] constexpr RoleSensitivity roleSensitivity(const GrooveVoice voice, const bool isBackbeat,
                                                        const bool isGhost, const GridStrength strength) noexcept
{
    switch (voice)
    {
        case GrooveVoice::Kick:
            return {0.15f};
        case GrooveVoice::Snare:
            if (isGhost)
            {
                return {0.15f};
            }
            return {isBackbeat ? 0.35f : 0.15f};
        case GrooveVoice::Hihat:
            return {strength == GridStrength::Weak ? 0.3f : 0.f};
        case GrooveVoice::Tom:
            return {0.25f};
        case GrooveVoice::Cymbal:
        case GrooveVoice::Other:
        default:
            return {0.f};
    }
}

}

// Deterministic per-role timing nudge, in ticks: negative/early = driving,
// positive/late = laid-back. Never gated by life - it is intentional bias,
// not preserved/discarded performance noise.
[[nodiscard]] inline float pushBias(const GrooveVoice voice, const bool isBackbeat, const bool isGhost,
                                    const GridStrength strength, const float push, const uint32_t stepTicks) noexcept
{
    return push * detail::roleSensitivity(voice, isBackbeat, isGhost, strength).pushFraction *
           static_cast<float>(stepTicks);
}

// The velocity target used at life = 0 (fully flattened/dead), itself
// shifted by push: driving punches up accents and mutes ghosts further,
// laid-back softens accents and lets ghosts sit more prominently.
[[nodiscard]] inline uint8_t roleFlatVelocity(const GrooveVoice voice, const bool isBackbeat, const bool isGhost,
                                              const GridStrength strength, const float push) noexcept
{
    float base = 80.f;
    float pushGain = -5.f;
    switch (voice)
    {
        case GrooveVoice::Kick:
            base = 100.f;
            pushGain = -5.f;
            break;
        case GrooveVoice::Snare:
            if (isGhost)
            {
                base = 40.f;
                pushGain = 15.f;
            }
            else if (isBackbeat)
            {
                base = 110.f;
                pushGain = -20.f;
            }
            else
            {
                base = 80.f;
                pushGain = -10.f;
            }
            break;
        case GrooveVoice::Hihat:
            base = strength == GridStrength::Weak ? 70.f : 90.f;
            pushGain = -10.f;
            break;
        case GrooveVoice::Cymbal:
            base = 100.f;
            pushGain = 0.f;
            break;
        case GrooveVoice::Tom:
            base = 90.f;
            pushGain = -10.f;
            break;
        case GrooveVoice::Other:
        default:
            base = 80.f;
            pushGain = -5.f;
            break;
    }
    const float value = std::clamp(base + push * pushGain, 1.f, 127.f);
    return static_cast<uint8_t>(std::lround(value));
}

// Grid/voice geometry, ghost detection and fill-bar detection for every note
// - independent of push/life, so it only needs recomputing when the source
// MIDI file itself changes (see GrooveKit's analyzed-groove cache).
[[nodiscard]] inline std::vector<GrooveAnalysisNote> analyzeNotes(
    const std::vector<GrooveNoteEvent>& events, const uint16_t ticksPerQuarterNote,
    const std::vector<MidiTimeSignatureEvent>& timeSignatures,
    const GridResolution resolution = GridResolution::Sixteenth,
    const VoiceForGrooveNoteFn voiceClassifier = &voiceForGrooveNote)
{
    std::vector<GrooveAnalysisNote> notes;
    notes.reserve(events.size());
    const uint32_t stepTicks = gridStepTicks(ticksPerQuarterNote, resolution);
    for (const auto& event : events)
    {
        GrooveAnalysisNote note;
        note.sourceTick = event.tick;
        note.note = event.note;
        note.velocity = event.velocity;
        note.voice = voiceClassifier(event.note);
        note.gridTick = inferredGridTick(event.tick, stepTicks);
        note.patternPosition = patternPositionInBar(timeSignatures, ticksPerQuarterNote, event.tick, resolution);
        note.strength = gridStrength(note.patternPosition, resolution);
        note.isBackbeat = note.voice == GrooveVoice::Snare && isBackbeatPosition(note.patternPosition, resolution);
        notes.push_back(note);
    }
    detail::markGhostNotes(notes);
    detail::markFillBars(notes, timeSignatures, ticksPerQuarterNote);
    return notes;
}

// outputTick = gridTick + pushBias(...) + life * (sourceTick - gridTick);
// outputVelocity mirrors it against roleFlatVelocity(...). Fill-bar-interior
// notes pass through source tick/velocity unconditionally.
[[nodiscard]] inline std::vector<HumanizedNote> applyHumanize(const std::vector<GrooveAnalysisNote>& notes,
                                                              const uint16_t ticksPerQuarterNote,
                                                              const GridResolution resolution, const float push,
                                                              const float life)
{
    const uint32_t stepTicks = gridStepTicks(ticksPerQuarterNote, resolution);
    std::vector<HumanizedNote> humanized;
    humanized.reserve(notes.size());
    for (const auto& note : notes)
    {
        if (note.isFillBar)
        {
            humanized.push_back({note.sourceTick, note.note, note.velocity});
            continue;
        }
        const float bias = pushBias(note.voice, note.isBackbeat, note.isGhost, note.strength, push, stepTicks);
        const float rawTick = static_cast<float>(note.gridTick) + bias +
                              life * (static_cast<float>(note.sourceTick) - static_cast<float>(note.gridTick));
        const auto outputTick = static_cast<uint32_t>(std::lround(std::max(0.f, rawTick)));

        const uint8_t flatVelocity = roleFlatVelocity(note.voice, note.isBackbeat, note.isGhost, note.strength, push);
        const float rawVelocity = static_cast<float>(flatVelocity) +
                                  life * (static_cast<float>(note.velocity) - static_cast<float>(flatVelocity));
        const auto outputVelocity = static_cast<uint8_t>(std::clamp(std::lround(rawVelocity), 1L, 127L));

        humanized.push_back({outputTick, note.note, outputVelocity});
    }
    return humanized;
}

[[nodiscard]] inline std::string_view voiceName(const GrooveVoice voice) noexcept
{
    switch (voice)
    {
        case GrooveVoice::Kick:
            return "kick";
        case GrooveVoice::Snare:
            return "snare";
        case GrooveVoice::Hihat:
            return "hihat";
        case GrooveVoice::Cymbal:
            return "cymbal";
        case GrooveVoice::Tom:
            return "tom";
        case GrooveVoice::Other:
        default:
            return "other";
    }
}

// One CSV row per note: voice/role flags plus source vs. output tick and
// velocity, for inspecting a push/life setting against a real groove.
[[nodiscard]] inline std::string toCsv(const std::vector<GrooveAnalysisNote>& notes,
                                       const std::vector<HumanizedNote>& humanized)
{
    std::ostringstream out;
    out << "voice,pattern_position,strength,backbeat,ghost,fill_bar,fill_landing,source_tick,output_tick,"
           "source_velocity,output_velocity\n";
    for (size_t i = 0; i < notes.size() && i < humanized.size(); ++i)
    {
        const auto& note = notes[i];
        out << voiceName(note.voice) << ',' << note.patternPosition << ','
            << (note.strength == GridStrength::Strong ? "strong" : "weak") << ',' << (note.isBackbeat ? 1 : 0) << ','
            << (note.isGhost ? 1 : 0) << ',' << (note.isFillBar ? 1 : 0) << ',' << (note.isFillLanding ? 1 : 0) << ','
            << note.sourceTick << ',' << humanized[i].tick << ',' << static_cast<int>(note.velocity) << ','
            << static_cast<int>(humanized[i].velocity) << '\n';
    }
    return out.str();
}

[[nodiscard]] inline bool writeCsvFile(const std::string& path, const std::vector<GrooveAnalysisNote>& notes,
                                       const std::vector<HumanizedNote>& humanized)
{
    std::ofstream out(path);
    if (!out)
    {
        return false;
    }
    out << toCsv(notes, humanized);
    return out.good();
}

}
