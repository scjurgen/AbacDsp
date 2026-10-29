#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "DrumMap.h"
#include "Generators/ClickGenerator.h"

namespace DrumOutput
{

/**
 * The click as MIDI notes for the drum module: the downbeat note on beat 1, the beat note on the
 * other beats and, softer, on subdivisions, each followed by its note-off after a fixed length.
 * Events wait in a fixed queue, stamped with sample times on the engine's clock, until drain()
 * hands them out relative to the start of the host block. Allocation-free.
 */
class MidiClickOut
{
  public:
    static constexpr float kNoteLengthMs{30.f};
    static constexpr uint8_t kDownbeatVelocity{120};
    static constexpr uint8_t kBeatVelocity{100};
    static constexpr uint8_t kSubVelocity{70};

    void configure(const DrumInput::ClickSettings& settings, const float sampleRate) noexcept
    {
        m_settings = settings;
        m_noteLengthSamples = static_cast<int64_t>(kNoteLengthMs * 0.001f * sampleRate);
    }

    void trigger(const AbacDsp::ClickAccent accent, const int64_t time) noexcept
    {
        if (accent == AbacDsp::ClickAccent::None)
        {
            return;
        }
        const bool downbeat = accent == AbacDsp::ClickAccent::Downbeat;
        const uint8_t note = downbeat ? m_settings.downbeatNote : m_settings.beatNote;
        const uint8_t velocity =
            downbeat ? kDownbeatVelocity : (accent == AbacDsp::ClickAccent::Beat ? kBeatVelocity : kSubVelocity);
        const auto channelBits = static_cast<uint8_t>((m_settings.outputChannel - 1) & 0x0F);
        releaseEarly(note, channelBits, time);
        enqueue(time, {static_cast<uint8_t>(0x90 | channelBits), note, velocity});
        holdNote(note, channelBits, time);
    }

    // Queues every note-off due before `until`, at its own due time.
    void advance(const int64_t until) noexcept
    {
        for (auto& held : m_held)
        {
            if (held.active && held.offTime < until)
            {
                release(held, held.offTime);
            }
        }
    }

    template <typename Emit>
    void drain(const int64_t blockStart, Emit&& emit)
    {
        std::ranges::sort(std::span{m_queue.data(), m_queueCount}, {}, &Event::time);
        for (size_t i = 0; i < m_queueCount; ++i)
        {
            const auto& event = m_queue[i];
            emit(std::span<const uint8_t>{event.bytes}, static_cast<int>(event.time - blockStart));
        }
        m_queueCount = 0;
    }

  private:
    struct Event
    {
        int64_t time{0};
        std::array<uint8_t, 3> bytes{};
    };

    struct HeldNote
    {
        int64_t offTime{0};
        uint8_t note{0};
        uint8_t channelBits{0};
        bool active{false};
    };

    static constexpr size_t kQueueCapacity{64};
    static constexpr size_t kMaxHeldNotes{8};

    void enqueue(const int64_t time, const std::array<uint8_t, 3> bytes) noexcept
    {
        if (m_queueCount < kQueueCapacity)
        {
            m_queue[m_queueCount++] = {time, bytes};
        }
    }

    void release(HeldNote& held, const int64_t time) noexcept
    {
        enqueue(time, {static_cast<uint8_t>(0x80 | held.channelBits), held.note, 0});
        held.active = false;
    }

    // A note retriggered before its note-off is released first, so the module sees on, off, on.
    void releaseEarly(const uint8_t note, const uint8_t channelBits, const int64_t time) noexcept
    {
        for (auto& held : m_held)
        {
            if (held.active && held.note == note && held.channelBits == channelBits)
            {
                release(held, time);
            }
        }
    }

    void holdNote(const uint8_t note, const uint8_t channelBits, const int64_t onTime) noexcept
    {
        auto slot = std::ranges::find_if(m_held, [](const HeldNote& held) { return !held.active; });
        if (slot == m_held.end())
        {
            slot = std::ranges::min_element(m_held, {}, &HeldNote::offTime);
            release(*slot, onTime);
        }
        *slot = {onTime + m_noteLengthSamples, note, channelBits, true};
    }

    DrumInput::ClickSettings m_settings{};
    int64_t m_noteLengthSamples{1440};
    std::array<Event, kQueueCapacity> m_queue{};
    size_t m_queueCount{0};
    std::array<HeldNote, kMaxHeldNotes> m_held{};
};

}
