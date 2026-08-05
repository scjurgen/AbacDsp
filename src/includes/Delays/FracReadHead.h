#pragma once

#include <cmath>
#include <cstddef>

#include "Numbers/EasyingFunctions.h"

namespace AbacDsp
{

/// @ingroup delays
/// @brief Whether the head is tracking at unit rate or currently easing towards a new delay.
enum class TransitionPhase
{
    Idle,
    Ramping
};

/**
 * @ingroup delays
 * @brief Fractional read head that changes its delay by briefly running off-rate, never by jumping.
 *
 * Moving a read pointer discontinuously clicks. Instead the advance rate eases
 * away from 1.0 and back, so the head drifts to its new distance from the write
 * head. The audible cost is a transient pitch shift bounded by maxAdvance:
 * 1.5 caps it at a fifth up, and a shortening move uses its reciprocal.
 *
 * The ramp length follows from that budget. Easing bumps the rate by at most
 * c = maxAdvance - 1 but averages 2c/3 over the quadratic curve and 8c/15 over
 * the quartic, so covering a distance d needs 1.5*d/c or 1.875*d/c steps
 * respectively; see kQuadraticRampScale / kQuarticRampScale in setNewDelta().
 *
 * A request arriving mid-ramp is held and applied on completion, one deep.
 * @see https://ccrma.stanford.edu/~jos/pasp/Time_Varying_Delay_Effects.html
 */
template <size_t WrapSize, bool quartic = false>
class FracReadHead
{
  public:
    explicit FracReadHead([[maybe_unused]] const float sampleRate) noexcept {}

    /// @brief A request that arrived mid-ramp, replayed once the current one finishes.
    struct Scheduled
    {
        bool hasNewValues;
        float newTargetDelta;
        float maxAdvance;
    };

    /// @brief Requests a new distance behind the reference position, reached over a ramp.
    /// Moves under 0.001 samples are ignored; maxAdvance caps the rate excursion and so the ramp length.
    void setNewDelta(const float newTargetDelta, const float maxAdvance = 1.5f) noexcept
    {
        if (m_currentPhase != TransitionPhase::Idle)
        {
            m_scheduled.hasNewValues = true;
            m_scheduled.newTargetDelta = newTargetDelta;
            m_scheduled.maxAdvance = maxAdvance;
            return;
        }

        const float currentDelta = getCurrentDelta();
        const float deltaDifference = newTargetDelta - currentDelta;

        if (std::abs(deltaDifference) < 0.001f)
        {
            return;
        }

        const bool reducingDelta = deltaDifference > 0;
        m_maxAdvance = reducingDelta ? (1 / maxAdvance) : maxAdvance;

        const float advanceDeviation = m_maxAdvance - 1.0f;
        constexpr float kQuadraticRampScale = 3.0f / 2.0f;
        constexpr float kQuarticRampScale = 15.0f / 8.0f;
        constexpr float rampScale = quartic ? kQuarticRampScale : kQuadraticRampScale;
        m_totalSteps = static_cast<size_t>(std::ceil(std::abs(rampScale * deltaDifference / advanceDeviation)));

        if (m_totalSteps == 0)
        {
            return;
        }

        m_currentPhase = TransitionPhase::Ramping;
        m_currentStep = 0;
    }

    /// @brief Advances one sample and returns the new read position.
    /// Position and advance are kept in double: at large WrapSize a float loses the fractional part.
    float step(const float referencePosition) noexcept
    {
        m_referencePosition = referencePosition;

        if (m_currentPhase == TransitionPhase::Ramping)
        {
            const auto progress = static_cast<double>(m_currentStep) / static_cast<double>(m_totalSteps);
            const auto c = m_maxAdvance - 1.0;

            if constexpr (quartic)
            {
                m_advance = Easying::smoothStep4(progress, c);
            }
            else
            {
                m_advance = Easying::smoothStep2(progress, c);
            }

            m_position += m_advance;
            m_currentStep++;

            if (m_currentStep >= m_totalSteps)
            {
                m_currentPhase = TransitionPhase::Idle;
                m_advance = 1.0;

                if (m_scheduled.hasNewValues)
                {
                    m_scheduled.hasNewValues = false;
                    setNewDelta(m_scheduled.newTargetDelta, m_scheduled.maxAdvance);
                }
            }
        }
        else
        {
            m_position += m_advance;
        }

        m_position = std::fmod(m_position + WrapSize, static_cast<double>(WrapSize));
        return static_cast<float>(m_position);
    }

    /// @brief Distance behind the reference position last passed to step(), wrapped into [0, WrapSize).
    [[nodiscard]] float getCurrentDelta() const noexcept
    {
        double v = m_referencePosition - m_position;
        return static_cast<float>(std::fmod(v + WrapSize, static_cast<double>(WrapSize)));
    }

    [[nodiscard]] bool isAdjusting() const noexcept
    {
        return m_currentPhase != TransitionPhase::Idle;
    }

    [[nodiscard]] float getCurrentAdvance() const noexcept
    {
        return static_cast<float>(m_advance);
    }

    [[nodiscard]] float getCurrentPosition() const noexcept
    {
        return static_cast<float>(m_position);
    }

    [[nodiscard]] TransitionPhase getCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    [[nodiscard]] size_t totalSteps() const noexcept
    {
        return m_totalSteps;
    }

  private:
    double m_position{0.0};
    double m_advance{1.0};
    float m_maxAdvance{1.5f};
    size_t m_totalSteps{0};
    TransitionPhase m_currentPhase{TransitionPhase::Idle};
    size_t m_currentStep{0};
    float m_referencePosition{0.0f};
    Scheduled m_scheduled{false, 0.0f, 0.0f};
};

}
