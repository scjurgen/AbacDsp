#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>

#include "Numbers/Approximation.h"

namespace AbacDsp
{

/**
 * @ingroup numbers
 * @brief Sample-counted equal-power crossfade between an outgoing and an incoming signal.
 *
 * The gains are cos and sin of a quarter turn, so their squares sum to 1 within the
 * approximation error (below 2e-4) and uncorrelated signals keep a constant power.
 * The position is an integer sample count: a fade of N steps ends exactly after N
 * samples, and an idle or finished crossfade yields gains {0, 1}, the incoming signal.
 * step() and mix() never allocate.
 */
class EqualPowerCrossfade
{
  public:
    struct Gains
    {
        float outgoing{0.f};
        float incoming{1.f};
    };

    /// Begins a fade over the given number of samples; zero finishes it immediately.
    void start(const size_t steps) noexcept
    {
        m_steps = steps;
        m_position = 0;
        m_advance = steps > 0 ? 1.f / static_cast<float>(steps) : 0.f;
    }

    /// Gains at a fade position from 0 (all outgoing) to 1 (all incoming).
    [[nodiscard]] static Gains gainsAt(const float progress) noexcept
    {
        return {Approximation::remezCosP6<Approximation::DomainMinusOneToOne>(progress),
                Approximation::remezSinP5<Approximation::DomainMinusOneToOne>(progress)};
    }

    [[nodiscard]] Gains step() noexcept
    {
        if (isDone())
        {
            return Gains{};
        }
        const auto gains = gainsAt(static_cast<float>(m_position) * m_advance);
        ++m_position;
        return gains;
    }

    /// Mixes two equally long signals into target, which may alias either input.
    void mix(const std::span<const float> outgoing, const std::span<const float> incoming,
             const std::span<float> target) noexcept
    {
        assert(outgoing.size() == incoming.size());
        assert(outgoing.size() == target.size());
        for (size_t i = 0; i < target.size(); ++i)
        {
            const auto gains = step();
            target[i] = outgoing[i] * gains.outgoing + incoming[i] * gains.incoming;
        }
    }

    /// Advances the position without producing gains, e.g. after mixing several channels.
    void skip(const size_t numSamples) noexcept
    {
        m_position = std::min(m_position + numSamples, m_steps);
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_position >= m_steps;
    }

    [[nodiscard]] size_t width() const noexcept
    {
        return m_steps;
    }

  private:
    size_t m_steps{0};
    size_t m_position{0};
    float m_advance{0.f};
};

}
