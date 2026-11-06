#pragma once

#include <cstddef>

#include "Filters/Biquad.h"

namespace AbacDsp
{

/// @ingroup filters
/// @brief Normalised biquad coefficients (a0 = 1) of the difference equation used by Biquad.
struct BiquadSection
{
    float b0{1.0f};
    float b1{0.0f};
    float b2{0.0f};
    float a1{0.0f};
    float a2{0.0f};
};

/**
 * @ingroup filters
 * @brief Biquad whose coefficients move linearly, sample by sample, to a new target over a set number of samples.
 *
 * For filters that are redesigned at control rate: a new design applied at once changes the output instantly by the
 * coefficient change times the state and input, which steps and clicks; spread over the samples up to the next design
 * the change is gradual. The interpolation keeps a stable filter stable: the stable (a1, a2) of a second-order section
 * form a triangle, which is convex, so every point between two stable designs is stable. At the end of a ramp the
 * coefficients are exactly the target. A target set during a ramp starts a new ramp from where the coefficients are.
 * A ramp length of 0 applies targets at once. Real-time safe.
 */
class RampedBiquad
{
  public:
    void setRampSamples(const size_t samples) noexcept
    {
        m_rampSamples = samples;
    }

    void setTarget(const BiquadSection& target) noexcept
    {
        m_target = target;
        if (m_rampSamples == 0)
        {
            snapToTarget();
            return;
        }
        const auto steps = static_cast<float>(m_rampSamples);
        m_increment = BiquadSection{(target.b0 - m_current.b0) / steps, (target.b1 - m_current.b1) / steps,
                                    (target.b2 - m_current.b2) / steps, (target.a1 - m_current.a1) / steps,
                                    (target.a2 - m_current.a2) / steps};
        m_remaining = m_rampSamples;
    }

    /// Ends a ramp: the coefficients jump to the target.
    void snapToTarget() noexcept
    {
        apply(m_target);
        m_remaining = 0;
    }

    [[nodiscard]] float process(const float in) noexcept
    {
        if (m_remaining > 0)
        {
            advanceRamp();
        }
        return m_filter.singleStepGeneric(in);
    }

    /// Clears the state; the coefficients stay as they are.
    void reset() noexcept
    {
        m_filter.reset();
    }

    [[nodiscard]] const BiquadSection& getTarget() const noexcept
    {
        return m_target;
    }

    [[nodiscard]] const BiquadSection& getCurrent() const noexcept
    {
        return m_current;
    }

    [[nodiscard]] bool isRamping() const noexcept
    {
        return m_remaining > 0;
    }

  private:
    void advanceRamp() noexcept
    {
        --m_remaining;
        if (m_remaining == 0)
        {
            apply(m_target);
            return;
        }
        apply(BiquadSection{m_current.b0 + m_increment.b0, m_current.b1 + m_increment.b1, m_current.b2 + m_increment.b2,
                            m_current.a1 + m_increment.a1, m_current.a2 + m_increment.a2});
    }

    void apply(const BiquadSection& section) noexcept
    {
        m_current = section;
        m_filter.setCoefficients(section.b0, section.b1, section.b2, section.a1, section.a2);
    }

    Biquad<BiquadFilterType::FreeCoefficients> m_filter;
    BiquadSection m_current{};
    BiquadSection m_target{};
    BiquadSection m_increment{};
    size_t m_rampSamples{0};
    size_t m_remaining{0};
};

} // namespace AbacDsp
