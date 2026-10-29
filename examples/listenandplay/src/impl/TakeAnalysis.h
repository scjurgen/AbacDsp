#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "Analysis/NoteSegmenter.h"
#include "KsInstrument.h"
#include "NoteAnalysisReport.h"
#include "NoteMatcher.h"
#include "NotePattern.h"
#include "TakeScore.h"
#include "TakeSession.h"

namespace TakeAnalysis
{

inline constexpr float kLowestPitchLimit{24.f};
inline constexpr float kHighestPitchLimit{108.f};
inline constexpr float kPitchMarginSemitones{1.f};
inline constexpr float kOctaveSemitones{12.f};
inline constexpr auto kPollInterval = std::chrono::milliseconds(5);

struct TakeResult
{
    uint64_t takeId{0};
    std::string lessonName;
    std::string lessonStem;
    NoteMatching::TakeSetup setup;
    std::vector<AbacDsp::PlayedNote> played;
    NoteMatching::MatchResult match;
    NoteScoring::TakeScore score;
    std::string reportHtml;
    float sampleRate{0.f};
    bool midi{false};
    std::vector<float> audio;
};

// The range the segmenter has to listen for: the phrase, an octave further out when the octave is free.
[[nodiscard]] inline AbacDsp::NoteSegmenterConfig segmenterConfigFor(const NotePattern::Pattern& pattern)
{
    float lowest = 127.f;
    float highest = 0.f;
    for (size_t i = 0; i < pattern.noteCount; ++i)
    {
        lowest = std::min(lowest, static_cast<float>(pattern.notes[i].pitch));
        highest = std::max(highest, static_cast<float>(pattern.notes[i].pitch));
    }
    const float reach = pattern.scoring.octaveFree ? kOctaveSemitones : 0.f;
    AbacDsp::NoteSegmenterConfig config;
    config.minHz = Convert::noteToFrequency<float>(std::max(kLowestPitchLimit, lowest - reach - kPitchMarginSemitones),
                                                   config.referenceA4);
    config.maxHz = Convert::noteToFrequency<float>(
        std::min(kHighestPitchLimit, highest + reach + kPitchMarginSemitones), config.referenceA4);
    return config;
}

// Each note-on up to its note-off (or the end of the take) becomes a played note; the velocity stands in for the level.
[[nodiscard]] inline std::vector<AbacDsp::PlayedNote> midiNotesOf(const std::span<const TakeSession::MidiEvent> events,
                                                                  const float sampleRate, const size_t takeSamples)
{
    std::vector<AbacDsp::PlayedNote> notes;
    std::vector<size_t> open;
    for (const auto& event : events)
    {
        if (event.on && event.velocity > 0)
        {
            AbacDsp::PlayedNote note;
            note.onsetSeconds = static_cast<float>(event.sample) / sampleRate;
            note.offsetSeconds = static_cast<float>(takeSamples) / sampleRate;
            note.midiNote = static_cast<float>(event.note);
            note.pitchConfidence = 1.f;
            note.attackRmsDb = KsInstrument::kReferenceLevelDb + KsInstrument::velocityDb(event.velocity);
            note.peakDb = note.attackRmsDb;
            open.push_back(notes.size());
            notes.push_back(note);
            continue;
        }
        const auto sounding = std::ranges::find_if(open, [&](const size_t i)
                                                   { return static_cast<uint8_t>(notes[i].midiNote) == event.note; });
        if (sounding != open.end())
        {
            notes[*sounding].offsetSeconds = static_cast<float>(event.sample) / sampleRate;
            open.erase(sounding);
        }
    }
    return notes;
}

[[nodiscard]] inline TakeResult scoreTake(const NoteMatching::TakeSetup& setup, std::vector<AbacDsp::PlayedNote> played,
                                          const std::string_view lessonName, const uint64_t takeId,
                                          const std::string_view lessonStem = {})
{
    TakeResult result;
    result.takeId = takeId;
    result.lessonName = std::string{lessonName};
    result.lessonStem = std::string{lessonStem};
    result.setup = setup;
    result.played = std::move(played);
    result.match = NoteMatching::matchNotes(result.setup, result.played);
    result.score = NoteScoring::scoreTake(result.setup, result.match);
    result.reportHtml = NoteReport::buildReportHtml(result.setup, result.match, result.score, result.lessonName);
    return result;
}

[[nodiscard]] inline TakeResult analyzeAudio(const std::span<const float> audio, const float sampleRate,
                                             const NoteMatching::TakeSetup& setup, const std::string_view lessonName,
                                             const uint64_t takeId, const std::string_view lessonStem = {})
{
    const AbacDsp::NoteSegmenter segmenter{sampleRate, segmenterConfigFor(setup.pattern)};
    return scoreTake(setup, segmenter.analyze(audio), lessonName, takeId, lessonStem);
}

[[nodiscard]] inline TakeResult analyzeMidi(const std::span<const TakeSession::MidiEvent> events,
                                            const size_t takeSamples, const float sampleRate,
                                            const NoteMatching::TakeSetup& setup, const std::string_view lessonName,
                                            const uint64_t takeId, const std::string_view lessonStem = {})
{
    return scoreTake(setup, midiNotesOf(events, sampleRate, takeSamples), lessonName, takeId, lessonStem);
}

// Analyses every finished take of a session on its own thread and keeps the newest result.
class Worker
{
  public:
    Worker(TakeSession& session, const float sampleRate)
        : m_session(session)
        , m_sampleRate(sampleRate)
        , m_thread([this](const std::stop_token& stop) { run(stop); })
    {
    }

    // Message thread; the name is that of the lesson the next takes belong to.
    void setLessonName(std::string name)
    {
        const std::lock_guard lock{m_mutex};
        m_lessonName = std::move(name);
    }

    // Message thread; the file name of the lesson the next takes belong to, empty for a script of the user's own.
    void setLessonStem(std::string stem)
    {
        const std::lock_guard lock{m_mutex};
        m_lessonStem = std::move(stem);
    }

    // Any thread; while on, the results of audio takes carry a copy of the recorded audio.
    void setKeepAudio(const bool keep) noexcept
    {
        m_keepAudio.store(keep, std::memory_order_relaxed);
    }

    // Message thread: the newest result, or null; each result is handed out once.
    [[nodiscard]] std::shared_ptr<const TakeResult> takeNewResult()
    {
        const std::lock_guard lock{m_mutex};
        return std::exchange(m_unseen, nullptr);
    }

    [[nodiscard]] std::shared_ptr<const TakeResult> latestResult() const
    {
        const std::lock_guard lock{m_mutex};
        return m_latest;
    }

    // One analysis pass; the thread runs it on a timer, tests call it directly.
    void pollOnce()
    {
        const uint64_t id = m_session.takeId();
        if (id == m_analysed || m_session.state() != TakeSession::State::Analysing)
        {
            return;
        }
        std::string name;
        std::string stem;
        {
            const std::lock_guard lock{m_mutex};
            name = m_lessonName;
            stem = m_lessonStem;
        }
        const auto& setup = m_session.setup();
        const bool midi = m_session.midiTake();
        auto result =
            midi ? analyzeMidi(m_session.midiEvents(), m_session.captured().size(), m_sampleRate, setup, name, id, stem)
                 : analyzeAudio(m_session.captured(), m_sampleRate, setup, name, id, stem);
        result.sampleRate = m_sampleRate;
        result.midi = midi;
        if (!midi && m_keepAudio.load(std::memory_order_relaxed))
        {
            result.audio.assign(m_session.captured().begin(), m_session.captured().end());
        }
        {
            const std::lock_guard lock{m_mutex};
            m_latest = std::make_shared<const TakeResult>(std::move(result));
            m_unseen = m_latest;
        }
        m_analysed = id;
        m_session.finishAnalysis(id);
    }

  private:
    void run(const std::stop_token& stop)
    {
        while (!stop.stop_requested())
        {
            pollOnce();
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    TakeSession& m_session;
    float m_sampleRate;
    mutable std::mutex m_mutex;
    std::string m_lessonName;
    std::string m_lessonStem;
    std::shared_ptr<const TakeResult> m_latest;
    std::shared_ptr<const TakeResult> m_unseen;
    uint64_t m_analysed{0};
    std::atomic<bool> m_keepAudio{false};
    std::jthread m_thread;
};

}
