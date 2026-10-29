#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "Generators/ClickGenerator.h"
#include "KsInstrument.h"
#include "NoteMatcher.h"
#include "NotePattern.h"
#include "PhrasePlayer.h"

/**
 * One practice take on the audio thread: the phrase plays, a count-in follows, the input is recorded
 * while the user plays it back, and the recording waits for the analysis. All times are sample
 * counts since the take was started. Nothing here allocates after construction. The recording and
 * the setup are read by the analysis thread only while the state is Analysing.
 */
class TakeSession
{
  public:
    enum class State : uint8_t
    {
        Idle,
        Listening,
        CountIn,
        Capturing,
        Analysing,
        Review,
        Replaying,
    };

    struct Settings
    {
        float bpm{100.f};
        KsInstrument::Id instrument{KsInstrument::Id::Guitar};
        bool clickOn{true};
        bool midiInput{false};
        float latencyMs{0.f};
        float clickVolumeDb{-12.f};
    };

    struct Timeline
    {
        size_t listenEnd{0};
        size_t countInStart{0};
        size_t downbeat{0};
        size_t captureStart{0};
        size_t captureEnd{0};
        size_t countInBeats{4};
        double samplesPerBeat{0.0};
        bool withTake{false};
    };

    struct MidiEvent
    {
        size_t sample{0};
        uint8_t note{0};
        uint8_t velocity{0};
        bool on{true};
    };

    static constexpr float kMaxCaptureSeconds{90.f};
    static constexpr size_t kMaxMidiEvents{1024};
    static constexpr float kReferenceSeconds{1.2f};
    static constexpr uint8_t kReferenceVelocity{100};
    static constexpr double kPauseBeats{1.0};
    static constexpr double kLeadBeats{1.0};
    static constexpr double kTailBeats{1.0};

    explicit TakeSession(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_player(sampleRate)
        , m_click(sampleRate)
        , m_capture(static_cast<size_t>(kMaxCaptureSeconds * sampleRate), 0.f)
    {
    }

    // Takes a new phrase only between takes. The take of the old phrase is gone with it, so the new phrase can
    // only be heard inside a take.
    bool setPhrase(const NotePattern::Pattern& pattern) noexcept
    {
        if (!isResting())
        {
            return false;
        }
        m_pattern = pattern;
        m_hasTake = false;
        m_hasTakePublished.store(false, std::memory_order_relaxed);
        enter(State::Idle);
        return true;
    }

    [[nodiscard]] bool hasPhrase() const noexcept
    {
        return m_pattern.active && m_pattern.noteCount > 0;
    }

    [[nodiscard]] const NotePattern::Pattern& phrase() const noexcept
    {
        return m_pattern;
    }

    // Plays the phrase again after a take of it, for comparing; before a take it is refused.
    bool listen(const Settings& settings) noexcept
    {
        return m_hasTake && begin(settings, false);
    }

    // Plays one note, the key of the phrase, and nothing else.
    bool playReference(const Settings& settings, const uint8_t pitch) noexcept
    {
        if (pitch == 0 || !isResting() || settings.bpm <= 0.f)
        {
            return false;
        }
        m_settings = settings;
        NotePattern::Pattern reference{};
        reference.active = true;
        reference.noteCount = 1;
        reference.notes[0] = {0.f, static_cast<float>(static_cast<double>(kReferenceSeconds) * settings.bpm / 60.0),
                              pitch, kReferenceVelocity};
        m_player.start(reference, settings.bpm, settings.instrument);
        m_timeline = Timeline{};
        m_timeline.samplesPerBeat = static_cast<double>(m_sampleRate) * 60.0 / static_cast<double>(settings.bpm);
        m_timeline.listenEnd = m_player.lengthSamples();
        m_clock = 0;
        m_reference.store(true, std::memory_order_relaxed);
        enter(State::Listening);
        return true;
    }

    // Plays the phrase, counts in and records the answer.
    bool startTake(const Settings& settings) noexcept
    {
        return begin(settings, true);
    }

    bool replayTake() noexcept
    {
        if (!m_hasTake || !isResting())
        {
            return false;
        }
        m_replayPosition = 0;
        enter(State::Replaying);
        return true;
    }

    // Ends playing, counting in or recording; an analysis already running is left alone.
    void stop() noexcept
    {
        const State current = m_state;
        if (current == State::Analysing || isResting())
        {
            return;
        }
        if (current == State::Listening)
        {
            m_player.stop();
        }
        enter(m_hasTake ? State::Review : State::Idle);
    }

    // Called from the analysis thread once it is done with the take of that id.
    void finishAnalysis(const uint64_t takeId) noexcept
    {
        m_analysisDoneId.store(takeId, std::memory_order_release);
    }

    void process(const std::span<const float> in, const std::span<float> out) noexcept
    {
        consumeAnalysisDone();
        size_t done = 0;
        while (done < out.size())
        {
            const size_t chunk = std::min(out.size() - done, samplesToNextEvent());
            processChunk(in.subspan(done, chunk), out.subspan(done, chunk));
            done += chunk;
            advance(chunk);
        }
    }

    // Audio thread: linear gains for the plucked phrase and for the click.
    void setLevels(const float instrumentGain, const float clickGain) noexcept
    {
        m_instrumentGain = instrumentGain;
        m_clickGain = clickGain;
    }

    // Any thread.
    [[nodiscard]] State state() const noexcept
    {
        return m_published.load(std::memory_order_acquire);
    }

    // Beats from the downbeat of the phrase; negative before it. Meaningful while listening, counting in and capturing.
    [[nodiscard]] float playheadBeats() const noexcept
    {
        return m_playheadBeats.load(std::memory_order_relaxed);
    }

    // A take is under way: its phrase must stay hidden until the result is in.
    [[nodiscard]] bool takeActive() const noexcept
    {
        return m_takeActive.load(std::memory_order_relaxed);
    }

    // A take of the current phrase has been recorded.
    [[nodiscard]] bool hasTake() const noexcept
    {
        return m_hasTakePublished.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool referencePlaying() const noexcept
    {
        return m_reference.load(std::memory_order_relaxed);
    }

    [[nodiscard]] int countInBeat() const noexcept
    {
        return m_countInBeat.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t takeId() const noexcept
    {
        return m_takeId.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool isResting() const noexcept
    {
        return m_state == State::Idle || m_state == State::Review;
    }

    [[nodiscard]] const Timeline& timeline() const noexcept
    {
        return m_timeline;
    }

    [[nodiscard]] const NoteMatching::TakeSetup& setup() const noexcept
    {
        return m_setup;
    }

    [[nodiscard]] std::span<const float> captured() const noexcept
    {
        return std::span<const float>{m_capture}.first(m_captureLength);
    }

    [[nodiscard]] std::span<const MidiEvent> midiEvents() const noexcept
    {
        return std::span<const MidiEvent>{m_midi}.first(m_midiCount);
    }

    [[nodiscard]] bool midiTake() const noexcept
    {
        return m_settings.midiInput;
    }

    // Offsets count from the first sample of the host block that is about to be processed.
    void recordMidi(const uint8_t note, const uint8_t velocity, const bool on, const int sampleOffset) noexcept
    {
        if (m_state != State::Capturing && m_state != State::CountIn)
        {
            return;
        }
        const auto at = static_cast<int64_t>(m_clock) + sampleOffset;
        if (at < static_cast<int64_t>(m_timeline.captureStart) || m_midiCount >= kMaxMidiEvents)
        {
            return;
        }
        m_midi[m_midiCount++] = {static_cast<size_t>(at) - m_timeline.captureStart, note, velocity, on};
    }

  private:
    static constexpr size_t kForever{std::numeric_limits<size_t>::max()};

    bool begin(const Settings& settings, const bool withTake) noexcept
    {
        if (!hasPhrase() || !isResting() || settings.bpm <= 0.f)
        {
            return false;
        }
        m_settings = settings;
        m_click.setVolumeDb(settings.clickVolumeDb);
        m_click.reset();
        m_player.start(m_pattern, settings.bpm, settings.instrument);
        planTimeline(withTake);
        m_reference.store(false, std::memory_order_relaxed);
        m_clock = 0;
        m_clickIndex = 0;
        m_midiCount = 0;
        enter(State::Listening);
        return true;
    }

    void planTimeline(const bool withTake) noexcept
    {
        auto& t = m_timeline;
        t = Timeline{};
        t.withTake = withTake;
        t.samplesPerBeat = static_cast<double>(m_sampleRate) * 60.0 / static_cast<double>(m_settings.bpm);
        t.listenEnd = m_player.lengthSamples();
        if (!withTake)
        {
            return;
        }
        const auto beats = [&t](const double count)
        { return static_cast<size_t>(std::llround(count * t.samplesPerBeat)); };
        t.countInBeats = m_pattern.beatsPerBar;
        t.countInStart = t.listenEnd + beats(kPauseBeats);
        t.downbeat = t.countInStart + beats(static_cast<double>(t.countInBeats));
        const size_t lead = beats(static_cast<double>(m_pattern.pickupBeats) + kLeadBeats);
        t.captureStart = std::max(t.countInStart, t.downbeat > lead ? t.downbeat - lead : size_t{0});
        t.captureEnd = t.downbeat + beats(static_cast<double>(m_pattern.patternBeats()) + kTailBeats);
        m_setup = NoteMatching::TakeSetup{};
        m_setup.pattern = m_pattern;
        m_setup.bpm = m_settings.bpm;
        m_setup.clickOn = m_settings.clickOn;
        m_setup.latencyMs = m_settings.latencyMs;
        m_setup.downbeatSeconds =
            static_cast<float>(static_cast<double>(t.downbeat - t.captureStart) / static_cast<double>(m_sampleRate));
    }

    void enter(const State next) noexcept
    {
        m_state = next;
        m_takeActive.store(next == State::CountIn || next == State::Capturing || next == State::Analysing ||
                               (next == State::Listening && m_timeline.withTake),
                           std::memory_order_relaxed);
        if (next != State::Listening)
        {
            m_reference.store(false, std::memory_order_relaxed);
        }
        m_published.store(next, std::memory_order_release);
        if (next != State::CountIn)
        {
            m_countInBeat.store(0, std::memory_order_relaxed);
        }
    }

    void consumeAnalysisDone() noexcept
    {
        if (m_state == State::Analysing && m_analysisDoneId.load(std::memory_order_acquire) == m_takeId.load())
        {
            enter(State::Review);
        }
    }

    [[nodiscard]] bool clicking() const noexcept
    {
        return m_state == State::CountIn || m_state == State::Capturing;
    }

    // The click at index i; the first bar is the count-in, the rest only plays when the click is on.
    [[nodiscard]] size_t clickSample(const size_t index) const noexcept
    {
        const bool wanted = index < m_timeline.countInBeats || m_settings.clickOn;
        const auto at = m_timeline.countInStart +
                        static_cast<size_t>(std::llround(static_cast<double>(index) * m_timeline.samplesPerBeat));
        return wanted && at < m_timeline.captureEnd ? at : kForever;
    }

    [[nodiscard]] size_t stateEnd() const noexcept
    {
        switch (m_state)
        {
            case State::Listening:
                return m_timeline.withTake ? m_timeline.countInStart : m_timeline.listenEnd;
            case State::CountIn:
                return m_timeline.captureStart;
            case State::Capturing:
                return m_timeline.captureEnd;
            case State::Idle:
            case State::Analysing:
            case State::Review:
            case State::Replaying:
                return kForever;
        }
    }

    [[nodiscard]] size_t samplesToNextEvent() const noexcept
    {
        size_t until = stateEnd();
        if (m_state == State::Replaying)
        {
            return std::max<size_t>(1, m_captureLength - m_replayPosition);
        }
        if (clicking())
        {
            until = std::min(until, clickSample(m_clickIndex));
        }
        return until == kForever ? kForever : std::max<size_t>(1, until > m_clock ? until - m_clock : 1);
    }

    void triggerDueClicks() noexcept
    {
        while (clickSample(m_clickIndex) <= m_clock)
        {
            const bool accent = m_clickIndex % m_timeline.countInBeats == 0;
            m_click.trigger(accent ? AbacDsp::ClickAccent::Downbeat : AbacDsp::ClickAccent::Beat);
            ++m_clickIndex;
        }
    }

    void processChunk(const std::span<const float> in, const std::span<float> out) noexcept
    {
        switch (m_state)
        {
            case State::Listening:
                playPhrase(out);
                break;
            case State::CountIn:
            case State::Capturing:
                triggerDueClicks();
                std::ranges::generate(out, [this] { return m_clickGain * m_click.step0(); });
                if (m_state == State::Capturing)
                {
                    recordInput(in);
                }
                break;
            case State::Replaying:
                std::ranges::copy(captured().subspan(m_replayPosition, out.size()), out.begin());
                break;
            case State::Idle:
            case State::Analysing:
            case State::Review:
                playPhrase(out);
                break;
        }
    }

    void playPhrase(const std::span<float> out) noexcept
    {
        m_player.process(out);
        std::ranges::transform(out, out.begin(), [gain = m_instrumentGain](const float v) { return v * gain; });
    }

    void recordInput(const std::span<const float> in) noexcept
    {
        const size_t at = m_clock - m_timeline.captureStart;
        for (size_t i = 0; i < in.size() && at + i < m_capture.size(); ++i)
        {
            m_capture[at + i] = in[i];
        }
    }

    void advance(const size_t chunk) noexcept
    {
        m_clock += chunk;
        if (m_state == State::Replaying)
        {
            m_replayPosition += chunk;
        }
        while (transition())
        {
        }
        publishPlayhead();
        if (m_state == State::CountIn && m_timeline.samplesPerBeat > 0.0)
        {
            const auto beat = static_cast<double>(m_clock - m_timeline.countInStart) / m_timeline.samplesPerBeat;
            m_countInBeat.store(static_cast<int>(beat) + 1, std::memory_order_relaxed);
        }
    }

    void publishPlayhead() noexcept
    {
        if (m_timeline.samplesPerBeat <= 0.0)
        {
            return;
        }
        const auto clock = static_cast<double>(m_clock);
        const double beats = m_state == State::Listening
                                 ? clock / m_timeline.samplesPerBeat - static_cast<double>(m_pattern.pickupBeats)
                                 : (clock - static_cast<double>(m_timeline.downbeat)) / m_timeline.samplesPerBeat;
        m_playheadBeats.store(static_cast<float>(beats), std::memory_order_relaxed);
    }

    [[nodiscard]] bool transition() noexcept
    {
        switch (m_state)
        {
            case State::Listening:
                if (m_clock < stateEnd())
                {
                    return false;
                }
                if (m_timeline.withTake)
                {
                    enter(State::CountIn);
                }
                else
                {
                    enter(m_hasTake ? State::Review : State::Idle);
                }
                return true;
            case State::CountIn:
                if (m_clock < m_timeline.captureStart)
                {
                    return false;
                }
                enter(State::Capturing);
                return true;
            case State::Capturing:
                if (m_clock < m_timeline.captureEnd)
                {
                    return false;
                }
                finishCapture();
                return true;
            case State::Replaying:
                if (m_replayPosition < m_captureLength)
                {
                    return false;
                }
                enter(State::Review);
                return true;
            case State::Idle:
            case State::Analysing:
            case State::Review:
                return false;
        }
    }

    void finishCapture() noexcept
    {
        m_captureLength = std::min(m_timeline.captureEnd - m_timeline.captureStart, m_capture.size());
        m_hasTake = true;
        m_hasTakePublished.store(true, std::memory_order_relaxed);
        m_takeId.fetch_add(1, std::memory_order_release);
        enter(State::Analysing);
    }

    float m_sampleRate;
    PhrasePlayer m_player;
    AbacDsp::ClickGenerator m_click;
    float m_instrumentGain{1.f};
    float m_clickGain{1.f};
    std::vector<float> m_capture;
    std::array<MidiEvent, kMaxMidiEvents> m_midi{};
    NotePattern::Pattern m_pattern{};
    Settings m_settings{};
    Timeline m_timeline{};
    NoteMatching::TakeSetup m_setup{};
    State m_state{State::Idle};
    size_t m_clock{0};
    size_t m_clickIndex{0};
    size_t m_captureLength{0};
    size_t m_replayPosition{0};
    size_t m_midiCount{0};
    bool m_hasTake{false};
    std::atomic<State> m_published{State::Idle};
    std::atomic<int> m_countInBeat{0};
    std::atomic<bool> m_takeActive{false};
    std::atomic<bool> m_hasTakePublished{false};
    std::atomic<bool> m_reference{false};
    std::atomic<float> m_playheadBeats{0.f};
    std::atomic<uint64_t> m_takeId{0};
    std::atomic<uint64_t> m_analysisDoneId{0};
};
