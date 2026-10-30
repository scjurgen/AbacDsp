#pragma once

#include <cmath>
#include <functional>

namespace AbacDsp
{

/**
 * @ingroup parameters
 * @brief Per-sample one-pole parameter smoother with a target, y[n] = t + (y[n-1] - t) c, c = exp(-1 / (tau fs)).
 *
 * The recursion of OnePoleFilter<LowPass> with its feedback set to c, plus what a control value needs: a target, a
 * jump (snapTo) and a settled test. Exponential approach without overshoot: after tau the remaining distance is
 * 1/e (36.8 %), after 2.2 tau it is 10 %. The largest step per sample is (1 - c) times the distance, so a jump of
 * the target becomes a ramp whose steepness is set by tau alone, however often new values arrive.
 *
 * The value is set exactly to the target once the remaining distance is below kSnapDistance or a step no longer
 * changes it. Without that the float update stalls at a distance of about ulp / (2 (1 - c)) (3e-5 for tau = 20 ms
 * at 48 kHz) and never reaches the target, and a decay towards 0 ends in denormals.
 *
 * The time constant is in the domain of the smoothed value: smooth linear gain for gains, log frequency for
 * cutoffs. A time of 0 means no smoothing. Real-time safe.
 * @see https://ccrma.stanford.edu/~jos/filters/One_Pole.html
 * @see https://en.wikipedia.org/wiki/Exponential_smoothing
 */
class OnePoleSmoother
{
  public:
    static constexpr float kSnapDistance{1e-7f};

    void setSampleRate(const float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
        updateCoefficient();
    }

    /// Time constant tau in milliseconds; 0 means no smoothing.
    void setTimeMs(const float milliseconds) noexcept
    {
        m_timeMs = milliseconds;
        updateCoefficient();
    }

    void setTarget(const float target) noexcept
    {
        m_target = target;
    }

    /// Jumps to a value without smoothing (initialisation, reset).
    void snapTo(const float value) noexcept
    {
        m_target = value;
        m_current = value;
    }

    [[nodiscard]] float next() noexcept
    {
        const float distance = m_current - m_target;
        const float stepped = m_target + distance * m_coefficient;
        m_current =
            (std::equal_to<float>{}(stepped, m_current) || std::fabs(distance) < kSnapDistance) ? m_target : stepped;
        return m_current;
    }

    [[nodiscard]] float getCurrent() const noexcept
    {
        return m_current;
    }

    [[nodiscard]] float getTarget() const noexcept
    {
        return m_target;
    }

    [[nodiscard]] bool isSettled() const noexcept
    {
        return std::equal_to<float>{}(m_current, m_target);
    }

    [[nodiscard]] float getCoefficient() const noexcept
    {
        return m_coefficient;
    }

  private:
    void updateCoefficient() noexcept
    {
        m_coefficient = m_timeMs > 0.0f ? std::exp(-1.0f / (m_timeMs * 0.001f * m_sampleRate)) : 0.0f;
    }

    float m_sampleRate{48000.0f};
    float m_timeMs{20.0f};
    float m_coefficient{std::exp(-1.0f / (0.02f * 48000.0f))};
    float m_target{0.0f};
    float m_current{0.0f};
};

}
