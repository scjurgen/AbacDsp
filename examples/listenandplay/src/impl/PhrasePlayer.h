#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>

#include "Generators/KarplusStrongEnsemble.h"
#include "KsInstrument.h"
#include "NotePattern.h"

/**
 * Plays a NotePattern on a plucked-string instrument, sample accurate and without allocating:
 * start() turns the phrase into a sorted event list, process() renders it block by block. Time
 * zero is the start of the pickup, so the downbeat lies downbeatSample() later. A note is damped
 * a short gap before its end, which keeps rests silent however long the string would ring and
 * leaves a dip in front of a repeated note. The phrase is monophonic; the voices rotate so a
 * damped tail can overlap the attack of the next note.
 */
class PhrasePlayer
{
  public:
    static constexpr size_t kNumVoices{4};
    static constexpr float kReleaseFadeMs{8.f};
    static constexpr float kEndGapMs{25.f};
    static constexpr float kMaxGapFraction{0.25f};
    static constexpr float kTailMs{60.f};
    static constexpr float kStopFadeMs{5.f};

    explicit PhrasePlayer(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_ensemble(sampleRate)
    {
    }

    // delaySamples shifts the whole phrase, so a caller can start it inside a host block.
    void start(const NotePattern::Pattern& pattern, const float bpm, const KsInstrument::Id instrument,
               const size_t delaySamples = 0) noexcept
    {
        assert(bpm > 0.f);
        cutVoices();
        const auto& preset = KsInstrument::presetOf(instrument);
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            auto& voice = m_ensemble.voice(i);
            voice.seed(static_cast<std::mt19937::result_type>(i + 1));
            KsInstrument::apply(preset, voice);
        }
        buildEvents(pattern, bpm, preset, delaySamples);
        m_position = 0;
        m_nextEvent = 0;
        m_playing = true;
    }

    // Fades the sounding voices out over kStopFadeMs; a cut would click.
    void stop() noexcept
    {
        m_stopRequested = true;
        m_eventCount = 0;
        m_playing = false;
    }

    [[nodiscard]] bool isPlaying() const noexcept
    {
        return m_playing;
    }

    // Samples from start() to the end of the last note's fade and tail.
    [[nodiscard]] size_t lengthSamples() const noexcept
    {
        return m_lengthSamples;
    }

    // Samples from start() to the downbeat of bar 1.
    [[nodiscard]] size_t downbeatSample() const noexcept
    {
        return m_downbeatSample;
    }

    [[nodiscard]] size_t positionSamples() const noexcept
    {
        return m_position;
    }

    // Overwrites out with the next samples (mono); silence once the phrase is over.
    void process(const std::span<float> out) noexcept
    {
        for (float& sample : out)
        {
            fadeOutIfStopRequested();
            fireDueEvents();
            const bool sounding = m_playing || m_stopFadeLeft > 0;
            sample = sounding ? m_ensemble.step() : 0.f;
            m_stopFadeLeft -= m_stopFadeLeft > 0 ? 1 : 0;
            if (m_playing && ++m_position >= m_lengthSamples)
            {
                m_playing = false;
            }
        }
    }

  private:
    enum class Kind : uint8_t
    {
        NoteOn,
        NoteOff,
    };

    struct Event
    {
        size_t sample{0};
        Kind kind{Kind::NoteOn};
        uint8_t voice{0};
        float note{0.f};
        float gain{0.f};
    };

    [[nodiscard]] size_t samplesAt(const double beats, const double samplesPerBeat, const size_t delay) const noexcept
    {
        return delay + static_cast<size_t>(std::llround(std::max(0.0, beats) * samplesPerBeat));
    }

    void buildEvents(const NotePattern::Pattern& pattern, const float bpm, const KsInstrument::Preset& preset,
                     const size_t delaySamples) noexcept
    {
        const double samplesPerBeat = static_cast<double>(m_sampleRate) * 60.0 / static_cast<double>(bpm);
        const double pickup = static_cast<double>(pattern.pickupBeats);
        m_downbeatSample = samplesAt(pickup, samplesPerBeat, 0);
        m_eventCount = 0;
        const auto toSamples = [this](const float ms) { return static_cast<size_t>(ms * 0.001f * m_sampleRate); };
        size_t lastEnd = delaySamples;
        for (size_t i = 0; i < pattern.noteCount; ++i)
        {
            const auto& note = pattern.notes[i];
            const auto voice = static_cast<uint8_t>(i % kNumVoices);
            const double start = static_cast<double>(note.positionBeats) + pickup;
            const double end = start + static_cast<double>(note.durationBeats);
            const size_t onSample = samplesAt(start, samplesPerBeat, delaySamples);
            lastEnd = samplesAt(end, samplesPerBeat, delaySamples);
            const auto gate = static_cast<size_t>(
                std::min(static_cast<double>(toSamples(kEndGapMs)),
                         static_cast<double>(kMaxGapFraction) * static_cast<double>(lastEnd - onSample)));
            m_events[m_eventCount++] = {onSample, Kind::NoteOn, voice, static_cast<float>(note.pitch),
                                        KsInstrument::noteGain(preset, note.pitch, note.velocity)};
            m_events[m_eventCount++] = {lastEnd - gate, Kind::NoteOff, voice, 0.f, 0.f};
        }
        m_lengthSamples = lastEnd + toSamples(kTailMs);
    }

    // Restarting mid-phrase cuts: a fade would shift the start of the new phrase.
    void cutVoices() noexcept
    {
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            m_ensemble.voice(i).muteString();
        }
        m_stopRequested = false;
        m_stopFadeLeft = 0;
    }

    void fadeOutIfStopRequested() noexcept
    {
        if (!m_stopRequested)
        {
            return;
        }
        for (size_t i = 0; i < kNumVoices; ++i)
        {
            m_ensemble.voice(i).scheduleExcitation(
                {0.f, kStopFadeMs, AbacDsp::ExcitationType::Mute, 1.f, std::nullopt});
        }
        m_stopFadeLeft = static_cast<size_t>(kStopFadeMs * 0.001f * m_sampleRate) + kStopFadeMargin;
        m_stopRequested = false;
    }

    void fireDueEvents() noexcept
    {
        while (m_nextEvent < m_eventCount && m_events[m_nextEvent].sample <= m_position)
        {
            const Event& event = m_events[m_nextEvent++];
            auto& voice = m_ensemble.voice(event.voice);
            if (event.kind == Kind::NoteOn)
            {
                voice.trigger(event.note, event.gain);
            }
            else
            {
                voice.scheduleExcitation({0.f, kReleaseFadeMs, AbacDsp::ExcitationType::Mute, 1.f, std::nullopt});
            }
        }
    }

    static constexpr size_t kStopFadeMargin{64};

    float m_sampleRate;
    AbacDsp::KarplusStrongEnsemble<kNumVoices, KsInstrument::kMaxStringLength> m_ensemble;
    std::array<Event, 2 * NotePattern::kMaxNotes> m_events{};
    size_t m_eventCount{0};
    size_t m_nextEvent{0};
    size_t m_position{0};
    size_t m_lengthSamples{0};
    size_t m_downbeatSample{0};
    size_t m_stopFadeLeft{0};
    bool m_stopRequested{false};
    bool m_playing{false};
};
