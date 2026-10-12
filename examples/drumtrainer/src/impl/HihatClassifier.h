#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

#include "DrumMap.h"

namespace DrumInput
{

enum class HihatArticulation : uint8_t
{
    Closed,
    HalfOpen,
    Open,
};

struct ClassifiedHihat
{
    int64_t time{0};
    uint8_t velocity{0};
    uint8_t pedal{0};
    HihatArticulation articulation{HihatArticulation::Closed};
};

/**
 * Decides closed, half-open or open for each hihat stick hit from the pedal position CC in
 * effect at the hit. A pedal change arriving within the grace window after the hit still
 * counts for it, because some modules send the CC just after the note it belongs to. A hit is
 * therefore emitted once its grace window has passed (or earlier, when a later event proves
 * the window over). Times are sample positions on one monotonic clock.
 */
class HihatClassifier
{
  public:
    void configure(const HihatSettings& settings, const float sampleRate) noexcept
    {
        m_settings = settings;
        m_graceSamples = static_cast<int64_t>(std::lround(settings.ccGraceMs * 0.001f * sampleRate));
    }

    [[nodiscard]] uint8_t pedal() const noexcept
    {
        return m_pedal;
    }

    [[nodiscard]] HihatArticulation classify(const uint8_t pedal) const noexcept
    {
        if (pedal > m_settings.closedAbove)
        {
            return HihatArticulation::Closed;
        }
        return pedal > m_settings.halfOpenAbove ? HihatArticulation::HalfOpen : HihatArticulation::Open;
    }

    template <typename Emit>
    void onPedal(const uint8_t value, const int64_t time, Emit&& emit)
    {
        if (m_pending && time <= deadline())
        {
            m_pending->pedal = value;
        }
        advance(time, emit);
        m_pedal = value;
    }

    template <typename Emit>
    void onStick(const uint8_t velocity, const int64_t time, Emit&& emit)
    {
        flush(emit);
        m_pending = ClassifiedHihat{time, velocity, m_pedal, HihatArticulation::Closed};
        advance(time, emit);
    }

    template <typename Emit>
    void advance(const int64_t now, Emit&& emit)
    {
        if (m_pending && now >= deadline() + (m_graceSamples > 0 ? 1 : 0))
        {
            flush(emit);
        }
    }

    void reset() noexcept
    {
        m_pending.reset();
    }

  private:
    [[nodiscard]] int64_t deadline() const noexcept
    {
        return m_pending->time + m_graceSamples;
    }

    template <typename Emit>
    void flush(Emit&& emit)
    {
        if (!m_pending)
        {
            return;
        }
        auto hit = *m_pending;
        m_pending.reset();
        hit.articulation = classify(hit.pedal);
        emit(hit);
    }

    HihatSettings m_settings{};
    int64_t m_graceSamples{0};
    // Closed until the first pedal CC arrives: a resting foot keeps the hihat closed.
    uint8_t m_pedal{127};
    std::optional<ClassifiedHihat> m_pending;
};

}
