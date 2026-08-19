#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Numbers/Interpolation.h"

namespace AbacDsp
{

/**
 * @ingroup filters
 * @brief Tuned delay-loop resonance: a feedback comb with loop damping and a soft limiter.
 *
 * y[n] = x[n] + headroom*tanh(g*damped(y[n-D])/headroom), D the loop length in samples,
 * g from setByDecay()'s decay time (Karplus-Strong's own relation); negative=true flips
 * g's sign, moving the resonant peaks to odd harmonics of half the tuned frequency. The
 * limiter is headroom-scaled, not a plain tanh(g*damped): the loop is already
 * unconditionally stable for |g|<1, so it only needs to catch genuine extremes, not
 * compress ordinary peaks. The buffer carries one guard sample past MaxSizeInSamples,
 * mirrored on every write to index 0, so Interpolation::linearPt2 can read across the
 * wrap with no branch/modulo, matching ModulationDelay.h's wrap-padding convention.
 * @see https://ccrma.stanford.edu/~jos/pasp/Karplus_Strong_Algorithm.html
 */
template <size_t MaxSizeInSamples>
class CombResonator
{
  public:
    explicit CombResonator(const float sampleRate = 48000.f) noexcept
        : m_sampleRate(sampleRate)
        , m_buffer(MaxSizeInSamples + 1, 0.f)
    {
    }

    void setSampleRate(const float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }

    /// @brief Loop damping (one-pole in the feedback path), 0 = none, 1 = fully damped.
    void setDamping(const float coefficient) noexcept
    {
        m_dampingCoeff = std::clamp(coefficient, 0.f, 1.f);
    }

    /// @brief Tunes the loop to a fundamental frequency and a T60-ish decay time; negative
    /// flips the feedback sign (peaks move to odd harmonics of half the frequency instead).
    void setByDecay(const float frequencyHz, const float decaySeconds, const bool negative = false) noexcept
    {
        const float minFreq = m_sampleRate / static_cast<float>(MaxSizeInSamples - 1);
        const float clampedFreq = std::clamp(frequencyHz, minFreq, m_sampleRate * 0.45f);
        m_delaySamples = std::clamp(m_sampleRate / clampedFreq, 1.f, static_cast<float>(MaxSizeInSamples - 1));
        const float safeDecay = std::max(decaySeconds, 0.001f);
        const float magnitude = std::clamp(std::exp(-m_delaySamples / (safeDecay * m_sampleRate)), 0.f, 0.999f);
        m_feedback = negative ? -magnitude : magnitude;
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        const float readPos =
            std::fmod(static_cast<float>(m_writeHead) - m_delaySamples + static_cast<float>(MaxSizeInSamples) * 2.f,
                      static_cast<float>(MaxSizeInSamples));
        const auto idx0 = static_cast<size_t>(readPos);
        const float frac = readPos - static_cast<float>(idx0);
        const float delayed = Interpolation::linearPt2(&m_buffer[idx0], frac);

        // dampingCoeff 0 tracks delayed instantly (no filtering); 1 never updates (fully
        // damped) - the coefficient is how much of the *new* value is rejected.
        m_damped += (1.f - m_dampingCoeff) * (delayed - m_damped);
        const float driven = m_feedback * m_damped;
        const float fedback = kLimiterHeadroom * std::tanh(driven / kLimiterHeadroom);
        const float y = in + fedback;
        write(y);
        return y;
    }

    void reset() noexcept
    {
        std::ranges::fill(m_buffer, 0.f);
        m_damped = 0.f;
    }

  private:
    void write(const float value) noexcept
    {
        m_buffer[m_writeHead] = value;
        if (m_writeHead == 0)
        {
            m_buffer[MaxSizeInSamples] = value;
        }
        m_writeHead = (m_writeHead + 1) % MaxSizeInSamples;
    }

    // How far the feedback signal can swing before the soft limiter really compresses it -
    // see the class doc for why this needs headroom, not a plain tanh(driven).
    static constexpr float kLimiterHeadroom{16.f};

    float m_sampleRate{48000.f};
    float m_delaySamples{100.f};
    float m_feedback{0.f};
    float m_dampingCoeff{0.2f};
    float m_damped{0.f};
    size_t m_writeHead{0};
    std::vector<float> m_buffer;
};

}
