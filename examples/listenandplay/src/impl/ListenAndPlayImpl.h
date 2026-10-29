#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "DisplaySnapshots.h"
#include "EffectBase.h"
#include "KsInstrument.h"
#include "LessonCatalog.h"
#include "LessonProgress.h"
#include "ListenAndPlayScriptEngine.h"
#include "Numbers/Convert.h"
#include "SnapshotBuilders.h"
#include "TakeAnalysis.h"
#include "TakeExport.h"
#include "TakeSession.h"
#include "TripleBuffer.h"

/**
 * The ListenAndPlay engine. The audio thread runs a TakeSession; a worker thread analyses each
 * finished take; the message thread loads the lesson script and publishes its phrase. The
 * setters only raise flags or store atomics, so host automation may call them from any thread.
 */
template <size_t BlockSize>
class ListenAndPlayImpl final : public EffectBase
{
  public:
    static constexpr int kMidiNoteOff{0x80};
    static constexpr int kMidiNoteOn{0x90};

    explicit ListenAndPlayImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_session(sampleRate)
        , m_worker(m_session, sampleRate)
    {
    }

    void setBpm(const float value) noexcept
    {
        m_bpm.store(value, std::memory_order_relaxed);
    }

    void setClickOn(const bool value) noexcept
    {
        m_clickOn.store(value, std::memory_order_relaxed);
    }

    void setLatencyComp(const float value) noexcept
    {
        m_latencyMs.store(value, std::memory_order_relaxed);
    }

    void setInstrument(const size_t index) noexcept
    {
        m_instrument.store(static_cast<int>(std::min(index, KsInstrument::kPresets.size() - 1)),
                           std::memory_order_relaxed);
    }

    // 0 audio, 1 MIDI, in blueprint dropdown order.
    void setInput(const size_t index) noexcept
    {
        m_midiInput.store(index == 1, std::memory_order_relaxed);
    }

    void setOutputVolume(const float db) noexcept
    {
        m_outputGain.store(Convert::dbToGain<float>(db), std::memory_order_relaxed);
    }

    void setSaveTakes(const bool on) noexcept
    {
        m_saveTakes.store(on, std::memory_order_relaxed);
        m_worker.setKeepAudio(on);
    }

    [[nodiscard]] bool saveTakes() const noexcept
    {
        return m_saveTakes.load(std::memory_order_relaxed);
    }

    [[nodiscard]] TakeExport::Context takeExportContext() const
    {
        const auto id = static_cast<KsInstrument::Id>(m_instrument.load(std::memory_order_relaxed));
        return {std::string{KsInstrument::presetOf(id).name}, m_inputGainDb.load(std::memory_order_relaxed)};
    }

    void setInputGain(const float db) noexcept
    {
        m_inputGainDb.store(db, std::memory_order_relaxed);
        m_inputGain.store(Convert::dbToGain<float>(db), std::memory_order_relaxed);
    }

    void setInstrumentLevel(const float db) noexcept
    {
        m_instrumentLevel.store(Convert::dbToGain<float>(db), std::memory_order_relaxed);
    }

    void setClickLevel(const float db) noexcept
    {
        m_clickLevel.store(Convert::dbToGain<float>(db), std::memory_order_relaxed);
    }

    void setListen(const bool pressed) noexcept
    {
        raise(m_listenRequests, pressed);
    }

    void setPlay(const bool pressed) noexcept
    {
        raise(m_playRequests, pressed);
    }

    void setReplay(const bool pressed) noexcept
    {
        raise(m_replayRequests, pressed);
    }

    void setStop(const bool pressed) noexcept
    {
        raise(m_stopRequests, pressed);
    }

    void setReference(const bool pressed) noexcept
    {
        raise(m_referenceRequests, pressed);
    }

    void setNext(const bool pressed) noexcept
    {
        raise(m_nextRequests, pressed);
    }

    // Message thread. Runs the lesson script for the current instrument and publishes its phrase.
    bool setScript(const std::string_view source)
    {
        m_scriptSource = std::string{source};
        m_lessonStem = std::exchange(m_pendingStem, std::string{});
        m_worker.setLessonStem(m_lessonStem);
        const bool loaded = loadPhrase();
        if (loaded && !m_lessonStem.empty())
        {
            m_progress.setLastLesson(m_lessonStem);
            m_progressDirty = true;
        }
        ++m_rowsVersion;
        return loaded;
    }

    // Names the library lesson the next setScript() call loads; a script of the user's own has no such name.
    void prepareLesson(std::string stem)
    {
        m_pendingStem = std::move(stem);
    }

    [[nodiscard]] const std::string& lessonStem() const noexcept
    {
        return m_lessonStem;
    }

    void setLessonLister(std::function<std::vector<std::string>()> lister)
    {
        m_lister = std::move(lister);
        m_catalogDirty = true;
        ++m_rowsVersion;
    }

    // Message thread: look the library up again, e.g. after a lesson script was added.
    void refreshLessons() noexcept
    {
        m_catalogDirty = true;
        ++m_rowsVersion;
    }

    // Message thread. The rows of the lesson browser, in curriculum order.
    [[nodiscard]] std::vector<ListenPlayDisplay::LessonRow> lessonRows()
    {
        ensureCatalog();
        const auto& lessons = m_catalog.lessons();
        const auto recommended = m_progress.recommended(lessons);
        std::vector<ListenPlayDisplay::LessonRow> rows;
        for (size_t i = 0; i < lessons.size(); ++i)
        {
            ListenPlayDisplay::LessonRow row;
            row.stem = lessons[i].stem;
            row.title = lessons[i].title;
            row.focus = std::string{NotePattern::kFocusNames[static_cast<size_t>(lessons[i].focus)]}.substr(5);
            row.group = lessons[i].stem[0];
            if (const auto* record = m_progress.find(row.stem))
            {
                row.best = record->best;
                row.attempts = record->attempts;
                row.passed = record->passed;
            }
            row.unlocked = m_progress.isUnlocked(lessons, i);
            row.recommended = recommended && *recommended == i;
            row.current = row.stem == m_lessonStem;
            rows.push_back(std::move(row));
        }
        return rows;
    }

    // Changes whenever the rows would look different.
    [[nodiscard]] uint64_t lessonRowsVersion() const noexcept
    {
        return m_rowsVersion;
    }

    [[nodiscard]] std::optional<std::string> recommendedLesson()
    {
        ensureCatalog();
        const auto index = m_progress.recommended(m_catalog.lessons());
        return index ? std::optional<std::string>{m_catalog.lessons()[*index].stem} : std::nullopt;
    }

    [[nodiscard]] std::string progressJson() const
    {
        return m_progress.toJson();
    }

    bool loadProgress(const std::string_view json)
    {
        ++m_rowsVersion;
        return m_progress.fromJson(json);
    }

    [[nodiscard]] const std::string& lastLessonStem() const noexcept
    {
        return m_progress.lastLesson();
    }

    [[nodiscard]] bool consumeProgressDirty() noexcept
    {
        return std::exchange(m_progressDirty, false);
    }

    // The roll tells nothing about the phrase until a take of it has been scored; then it shows it next to the take.
    [[nodiscard]] ListenPlayDisplay::PianoRollSnapshot pianoRollSnapshot() const
    {
        ListenPlayDisplay::PianoRollSnapshot roll;
        if (const auto result = revealedResult())
        {
            roll = ListenPlayDisplay::rollOfPhrase(m_phrase);
            ListenPlayDisplay::addPlayedNotes(roll, *result);
        }
        const auto phase = ListenPlayDisplay::phaseOf(m_session.state());
        roll.phase = phase;
        roll.hasPhrase = m_phrase.active && m_phrase.noteCount > 0;
        roll.hasReference = m_phrase.tonic != 0;
        roll.reference = m_session.referencePlaying();
        roll.progress = progressOf(phase);
        roll.countInBeat = m_session.countInBeat();
        roll.countInBeats = m_phrase.beatsPerBar;
        return roll;
    }

    [[nodiscard]] ListenPlayDisplay::ScoreSnapshot scoreSnapshot() const
    {
        const auto result = revealedResult();
        return result ? ListenPlayDisplay::scoreOfResult(*result) : ListenPlayDisplay::ScoreSnapshot{};
    }

    void setImportResolver(ListenAndPlayScriptEngine::ImportResolver resolver)
    {
        m_resolver = resolver;
        m_engine.setImportResolver(std::move(resolver));
        m_catalogDirty = true;
    }

    [[nodiscard]] bool hasScriptError() const noexcept
    {
        return m_engine.hasError();
    }

    [[nodiscard]] const std::string& scriptError() const noexcept
    {
        return m_engine.lastError();
    }

    [[nodiscard]] static std::string scriptSkeleton()
    {
        return std::string{ListenAndPlayScriptEngine::kSkeletonScript};
    }

    [[nodiscard]] const ListenAndPlayScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_engine.uiParamSlots();
    }

    // Message thread, once per UI tick: draws a new phrase on request and when the instrument changed.
    void poll()
    {
        const uint32_t next = m_nextRequests.load(std::memory_order_acquire);
        const int instrument = m_instrument.load(std::memory_order_relaxed);
        if (next == m_nextHandled && instrument == m_instrumentLoaded)
        {
            return;
        }
        if (next != m_nextHandled)
        {
            ++m_seed;
        }
        m_nextHandled = next;
        static_cast<void>(loadPhrase());
    }

    [[nodiscard]] std::string lessonName() const
    {
        return m_lessonName;
    }

    // Message thread: the result of a take that just finished; a lesson's progress is kept from it.
    [[nodiscard]] std::shared_ptr<const TakeAnalysis::TakeResult> takeNewResult()
    {
        auto result = m_worker.takeNewResult();
        if (result && !result->lessonStem.empty())
        {
            m_progress.record(result->lessonStem, result->score.overall, result->score.passed);
            m_progressDirty = true;
            ++m_rowsVersion;
        }
        return result;
    }

    [[nodiscard]] std::string statusText() const
    {
        using State = TakeSession::State;
        switch (m_session.state())
        {
            case State::Idle:
                return idleText();
            case State::Listening:
                return m_session.referencePlaying() ? "Reference tone" : "Listen";
            case State::CountIn:
                return std::format("Count-in {}", m_session.countInBeat());
            case State::Capturing:
                return "Play!";
            case State::Analysing:
                return "Analysing...";
            case State::Replaying:
                return "Replaying your take";
            case State::Review:
                return reviewText();
        }
        return {};
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        handleRequests();
        if (m_session.isResting())
        {
            if (const auto* phrase = m_phrases.acquireNewest())
            {
                m_session.setPhrase(*phrase);
            }
        }
        const float inputGain = m_inputGain.load(std::memory_order_relaxed);
        std::array<float, BlockSize> mono{};
        std::array<float, BlockSize> sound{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            mono[i] = 0.5f * inputGain * (in(i, 0) + in(i, 1));
        }
        m_session.setLevels(m_instrumentLevel.load(std::memory_order_relaxed),
                            m_clickLevel.load(std::memory_order_relaxed));
        m_session.process(mono, sound);
        const float gain = m_outputGain.load(std::memory_order_relaxed);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = (sound[i] + inputGain * in(i, 0)) * gain;
            out(i, 1) = (sound[i] + inputGain * in(i, 1)) * gain;
        }
    }

    // Called before the host block's audio; the offset counts from that block's first sample.
    void processMidiEvent(const std::span<const uint8_t> bytes, const int sampleOffset) noexcept
    {
        if (bytes.size() < 3)
        {
            return;
        }
        const int status = bytes[0] & 0xF0;
        if (status == kMidiNoteOn && bytes[2] > 0)
        {
            m_session.recordMidi(bytes[1], bytes[2], true, sampleOffset);
        }
        else if (status == kMidiNoteOff || status == kMidiNoteOn)
        {
            m_session.recordMidi(bytes[1], 0, false, sampleOffset);
        }
    }

    [[nodiscard]] TakeSession& session() noexcept
    {
        return m_session;
    }

    [[nodiscard]] TakeAnalysis::Worker& worker() noexcept
    {
        return m_worker;
    }

  private:
    static void raise(std::atomic<uint32_t>& counter, const bool pressed) noexcept
    {
        if (pressed)
        {
            counter.fetch_add(1, std::memory_order_release);
        }
    }

    [[nodiscard]] static bool take(const std::atomic<uint32_t>& counter, uint32_t& handled) noexcept
    {
        const uint32_t seen = counter.load(std::memory_order_acquire);
        const bool pending = seen != handled;
        handled = seen;
        return pending;
    }

    [[nodiscard]] TakeSession::Settings settings() const noexcept
    {
        TakeSession::Settings s;
        s.bpm = m_bpm.load(std::memory_order_relaxed);
        s.instrument = static_cast<KsInstrument::Id>(m_instrument.load(std::memory_order_relaxed));
        s.clickOn = m_clickOn.load(std::memory_order_relaxed);
        s.midiInput = m_midiInput.load(std::memory_order_relaxed);
        s.latencyMs = m_latencyMs.load(std::memory_order_relaxed);
        return s;
    }

    void handleRequests() noexcept
    {
        if (take(m_referenceRequests, m_referenceHandled))
        {
            const uint8_t tonic = m_tonic.load(std::memory_order_relaxed);
            m_referenceUnavailable.store(tonic == 0, std::memory_order_relaxed);
            m_session.playReference(settings(), tonic);
        }
        if (take(m_stopRequests, m_stopHandled))
        {
            m_session.stop();
        }
        if (take(m_listenRequests, m_listenHandled))
        {
            m_session.listen(settings());
        }
        if (take(m_replayRequests, m_replayHandled))
        {
            m_session.replayTake();
        }
        if (take(m_playRequests, m_playHandled))
        {
            m_referenceUnavailable.store(false, std::memory_order_relaxed);
            m_session.startTake(settings());
        }
    }

    // The newest result, when it is of the phrase on screen, the take is over and nothing else is being played.
    [[nodiscard]] std::shared_ptr<const TakeAnalysis::TakeResult> revealedResult() const
    {
        if (!m_session.hasTake() || m_session.takeActive())
        {
            return nullptr;
        }
        auto result = m_worker.latestResult();
        return result && ListenPlayDisplay::samePhrase(result->setup.pattern, m_phrase) ? result : nullptr;
    }

    [[nodiscard]] float progressOf(const ListenPlayDisplay::Phase phase) const noexcept
    {
        using Phase = ListenPlayDisplay::Phase;
        const float total = m_phrase.patternBeats() + m_phrase.pickupBeats;
        if (phase == Phase::Analysing)
        {
            return 1.f;
        }
        if ((phase != Phase::Listening && phase != Phase::Capturing) || m_session.referencePlaying() || total <= 0.f)
        {
            return 0.f;
        }
        return std::clamp((m_session.playheadBeats() + m_phrase.pickupBeats) / total, 0.f, 1.f);
    }

    [[nodiscard]] std::string idleText() const
    {
        if (!m_session.hasPhrase())
        {
            return "No lesson loaded";
        }
        return m_referenceUnavailable.load(std::memory_order_relaxed) ? "This lesson has no reference tone"
                                                                      : "Press Play to hear the phrase";
    }

    bool loadPhrase()
    {
        m_instrumentLoaded = m_instrument.load(std::memory_order_relaxed);
        const auto& preset = KsInstrument::presetOf(static_cast<KsInstrument::Id>(m_instrumentLoaded));
        m_engine.setRange(preset.lowestNote, preset.highestNote());
        if (!m_engine.loadPattern(m_scriptSource, ListenAndPlayScriptEngine::kDefaultBarBeats,
                                  static_cast<long long>(m_seed)))
        {
            return false;
        }
        m_lessonName = m_engine.result().name;
        m_phrase = m_engine.result().pattern;
        m_tonic.store(m_phrase.tonic, std::memory_order_relaxed);
        m_referenceUnavailable.store(false, std::memory_order_relaxed);
        m_worker.setLessonName(m_lessonName);
        m_phrases.backSlot() = m_engine.result().pattern;
        m_phrases.publish();
        return true;
    }

    void ensureCatalog()
    {
        if (m_catalogDirty && m_lister && m_resolver)
        {
            const auto names = m_lister();
            m_catalog.rebuild(names, m_resolver);
            m_catalogDirty = false;
        }
    }

    [[nodiscard]] std::string reviewText() const
    {
        const auto result = m_worker.latestResult();
        if (!result)
        {
            return "Review";
        }
        const auto& score = result->score;
        return std::format("{}: {:.0f} % {} (notes {:.0f}, rhythm {:.0f}, dynamics {:.0f})", result->lessonName,
                           score.overall, score.passed ? "passed" : "not yet", score.notes, score.rhythm,
                           score.dynamics);
    }

    TakeSession m_session;
    TakeAnalysis::Worker m_worker;
    ListenAndPlayScriptEngine m_engine;
    TripleBuffer<NotePattern::Pattern> m_phrases;

    std::atomic<float> m_bpm{100.f};
    std::atomic<float> m_latencyMs{0.f};
    std::atomic<float> m_outputGain{1.f};
    std::atomic<float> m_inputGain{1.f};
    std::atomic<float> m_inputGainDb{0.f};
    std::atomic<bool> m_saveTakes{false};
    std::atomic<float> m_instrumentLevel{1.f};
    std::atomic<float> m_clickLevel{1.f};
    std::atomic<int> m_instrument{0};
    std::atomic<bool> m_clickOn{true};
    std::atomic<bool> m_midiInput{false};
    std::atomic<uint32_t> m_listenRequests{0};
    std::atomic<uint32_t> m_playRequests{0};
    std::atomic<uint32_t> m_replayRequests{0};
    std::atomic<uint32_t> m_stopRequests{0};
    std::atomic<uint32_t> m_referenceRequests{0};
    std::atomic<uint8_t> m_tonic{0};
    std::atomic<bool> m_referenceUnavailable{false};
    std::atomic<uint32_t> m_nextRequests{0};

    uint32_t m_listenHandled{0};
    uint32_t m_playHandled{0};
    uint32_t m_replayHandled{0};
    uint32_t m_stopHandled{0};
    uint32_t m_referenceHandled{0};
    uint32_t m_nextHandled{0};

    ListenAndPlayScriptEngine::ImportResolver m_resolver;
    std::function<std::vector<std::string>()> m_lister;
    LessonCatalog m_catalog;
    LessonProgress m_progress;
    NotePattern::Pattern m_phrase{};
    std::string m_scriptSource;
    std::string m_lessonStem;
    std::string m_pendingStem;
    std::string m_lessonName;
    uint64_t m_seed{1};
    uint64_t m_rowsVersion{1};
    bool m_catalogDirty{true};
    bool m_progressDirty{false};
    int m_instrumentLoaded{0};
};
