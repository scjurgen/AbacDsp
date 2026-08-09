#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

#include "DroneScriptEngine.h"
#include "Generators/KarplusStrongEnsemble.h"

/**
 * Lookahead-driven sequencer for the drone sequencer's N-string ensemble: at a fixed
 * BPM/division interval, asks a DroneScriptEngine for the next notes a fixed lookahead
 * ahead of the nominal beat, then schedules each returned note sample-accurately
 * (trigger, plus an optional stop at note.lengthMs later). Transpose, detune/tuning,
 * and humanize timing/level are applied on top of whatever the script returns; pattern,
 * harmonics, and slide are the script's concern now, not the sequencer's.
 */
template <size_t MaxLength, size_t MaxVoices>
class DroneSequencer
{
  public:
    static constexpr size_t kMaxVoices{MaxVoices};

    explicit DroneSequencer(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void setTuning(const float tuningHz) noexcept
    {
        m_tuning = tuningHz;
    }

    void setTranspose(const float semitones) noexcept
    {
        m_transposeSemitones = semitones;
    }

    void setDetuneCents(const size_t voiceIndex, const float cents) noexcept
    {
        m_detuneCents[voiceIndex] = cents;
    }

    void setIntervalMs(const float ms) noexcept
    {
        m_intervalMs = ms;
    }

    void setHumanizeTiming(const float percent) noexcept
    {
        m_humanizeTimingPercent = std::clamp(percent, 0.f, 100.f);
    }

    void setHumanizeLevel(const float percent) noexcept
    {
        m_humanizeLevelPercent = std::clamp(percent, 0.f, 100.f);
    }

    void setPlaying(const bool playing) noexcept
    {
        if (playing && !m_playing)
        {
            resetClocks();
        }
        m_playing = playing;
    }

    void step(AbacDsp::KarplusStrongEnsemble<kMaxVoices, MaxLength>& ensemble, DroneScriptEngine& script) noexcept
    {
        firePendingEvents(ensemble);
        if (!m_playing)
        {
            return;
        }
        if (m_samplesUntilNextRequest > 0)
        {
            --m_samplesUntilNextRequest;
            return;
        }
        advanceTick(script);
    }

    [[nodiscard]] int64_t nominalPositionSamples() const noexcept
    {
        return m_nominalPositionSamples;
    }

  private:
    static constexpr float kLookaheadMs{30.f};
    static constexpr size_t kMaxPendingEvents{DroneScriptEngine::kMaxNotesPerRequest * 4};

    struct PendingEvent
    {
        size_t samplesUntil{0};
        size_t voiceIndex{0};
        float note{0.f};
        float gain{0.f};
        float tuning{440.f};
        bool isStop{false};
        bool active{false};
    };

    [[nodiscard]] size_t msToSamples(const float ms) const noexcept
    {
        return static_cast<size_t>(std::max(0.f, ms) * m_sampleRate * 0.001f);
    }

    [[nodiscard]] int64_t msToSamplesSigned(const float ms) const noexcept
    {
        return static_cast<int64_t>(ms * m_sampleRate * 0.001f);
    }

    [[nodiscard]] size_t clampedVoiceIndex(const size_t channel) const noexcept
    {
        return std::min(channel, MaxVoices - 1);
    }

    [[nodiscard]] float rollPluckGain(const float baseVelocity) noexcept
    {
        if (m_humanizeLevelPercent <= 0.f)
        {
            return baseVelocity;
        }
        const auto unit = m_uniformDist(m_rng) * 0.02f - 1.f; // 0..100 -> -1..+1
        return std::max(0.f, baseVelocity * (1.f + unit * m_humanizeLevelPercent * 0.01f));
    }

    // Gaussian, hard-clamped to +/- humanizeTiming% of the nominal interval so a rare
    // tail draw can't exceed the configured bound or push the scheduled wait negative.
    [[nodiscard]] int64_t rollTimingJitterSamples(const size_t nominalIntervalSamples) noexcept
    {
        if (m_humanizeTimingPercent <= 0.f)
        {
            return 0;
        }
        const auto boundSamples = static_cast<float>(nominalIntervalSamples) * m_humanizeTimingPercent * 0.01f;
        const auto jitter = m_normalDist(m_rng) * (boundSamples / 3.f);
        return static_cast<int64_t>(std::clamp(jitter, -boundSamples, boundSamples));
    }

    void resetClocks() noexcept
    {
        m_samplesUntilNextRequest = 0;
        m_nominalPositionSamples = 0;
        m_actualPositionSamples = 0;
        for (auto& event : m_pending)
        {
            event.active = false;
        }
    }

    [[nodiscard]] PendingEvent* findFreeSlot() noexcept
    {
        for (auto& event : m_pending)
        {
            if (!event.active)
            {
                return &event;
            }
        }
        return nullptr;
    }

    void enqueueTrigger(const int64_t samplesUntil, const DroneNote& note) noexcept
    {
        PendingEvent* slot = findFreeSlot();
        if (slot == nullptr)
        {
            return; // pending queue full; drop the note rather than corrupt scheduling state
        }
        slot->active = true;
        slot->isStop = false;
        slot->samplesUntil = static_cast<size_t>(std::max<int64_t>(0, samplesUntil));
        slot->voiceIndex = clampedVoiceIndex(note.channel);
        slot->note = note.noteHeight + m_transposeSemitones;
        slot->gain = rollPluckGain(note.velocity);
        slot->tuning = m_tuning;
    }

    void enqueueStop(const int64_t samplesUntil, const size_t voiceIndex) noexcept
    {
        PendingEvent* slot = findFreeSlot();
        if (slot == nullptr)
        {
            return;
        }
        slot->active = true;
        slot->isStop = true;
        slot->samplesUntil = static_cast<size_t>(std::max<int64_t>(0, samplesUntil));
        slot->voiceIndex = voiceIndex;
    }

    void requestNotes(DroneScriptEngine& script) noexcept
    {
        const auto result = script.nextNotes();
        const auto lookaheadSamples = static_cast<int64_t>(msToSamples(kLookaheadMs));
        for (size_t i = 0; i < result.count; ++i)
        {
            const auto& note = result.notes[i];
            const auto triggerOffset = lookaheadSamples + msToSamplesSigned(note.delayMs);
            enqueueTrigger(triggerOffset, note);
            if (note.lengthMs > 0.f)
            {
                const auto stopOffset = triggerOffset + static_cast<int64_t>(msToSamples(note.lengthMs));
                enqueueStop(stopOffset, clampedVoiceIndex(note.channel));
            }
        }
    }

    void advanceTick(DroneScriptEngine& script) noexcept
    {
        requestNotes(script);
        const auto nominalIntervalSamples = msToSamples(m_intervalMs);
        m_nominalPositionSamples += static_cast<int64_t>(nominalIntervalSamples);
        const auto targetActual = m_nominalPositionSamples + rollTimingJitterSamples(nominalIntervalSamples);
        const auto wait = std::max<int64_t>(1, targetActual - m_actualPositionSamples);
        m_samplesUntilNextRequest = static_cast<size_t>(wait);
        m_actualPositionSamples += wait;
    }

    void firePendingEvents(AbacDsp::KarplusStrongEnsemble<kMaxVoices, MaxLength>& ensemble) noexcept
    {
        for (auto& event : m_pending)
        {
            if (!event.active)
            {
                continue;
            }
            if (event.samplesUntil > 0)
            {
                --event.samplesUntil;
                continue;
            }
            auto& voice = ensemble.voice(event.voiceIndex);
            if (event.isStop)
            {
                voice.stopString();
            }
            else
            {
                voice.trigger(event.note, event.gain, event.tuning);
                voice.bendInCents(m_detuneCents[event.voiceIndex]);
            }
            event.active = false;
        }
    }

    float m_sampleRate;
    float m_tuning{440.f};
    float m_transposeSemitones{0.f};
    float m_intervalMs{600.f};
    float m_humanizeTimingPercent{0.f};
    float m_humanizeLevelPercent{0.f};
    std::array<float, kMaxVoices> m_detuneCents{};
    bool m_playing{false};

    size_t m_samplesUntilNextRequest{0};
    int64_t m_nominalPositionSamples{0};
    int64_t m_actualPositionSamples{0};
    std::array<PendingEvent, kMaxPendingEvents> m_pending{};

    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_uniformDist{0.f, 100.f};
    std::normal_distribution<float> m_normalDist{0.f, 1.f};
};
