#pragma once

#include <cmath>
#include <algorithm>
#include <iostream>

namespace AbacDsp
{

template <size_t WrapSize>
class FracReadHead
{
  public:
    enum class TransitionPhase
    {
        Idle,
        Ramping
    };

    explicit FracReadHead(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    struct Scheduled
    {
        bool hasNewValues;
        float newTargetDelta;
        float maxAdvance;
    };

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

        m_targetDelta = newTargetDelta;
        m_reducingDelta = deltaDifference > 0;

        m_maxAdvance = m_reducingDelta ? (2.0f - maxAdvance) : maxAdvance;

        const float advanceDeviation = m_maxAdvance - 1.0f;

        m_totalSteps = static_cast<size_t>(std::ceil(std::abs(1.5f * deltaDifference / advanceDeviation)));

        if (m_totalSteps == 0)
            return;

        m_currentPhase = TransitionPhase::Ramping;
        m_currentStep = 0;

        std::cout << "setNewDelta: current=" << currentDelta << " target=" << newTargetDelta
                  << " maxAdvance=" << m_maxAdvance << " steps=" << m_totalSteps << "\n";
    }

    float step(const float referencePosition) noexcept
    {
        m_referencePosition = referencePosition;

        if (m_currentPhase == TransitionPhase::Ramping)
        {
            const double progress = static_cast<double>(m_currentStep) / static_cast<double>(m_totalSteps);
            const double deviation = (m_maxAdvance - 1.0) * 4.0 * (progress - progress * progress);
            m_advance = 1.0 + deviation;

            m_position += m_advance;
            m_currentStep++;

            if (m_currentStep >= m_totalSteps)
            {
                m_currentPhase = TransitionPhase::Idle;
                m_advance = 1.0;

                const float deltaBeforeSnap = getCurrentDelta();

                float targetPosition = m_referencePosition - m_targetDelta;
                m_position = std::fmod(targetPosition + WrapSize, static_cast<double>(WrapSize));

                const float deltaAfterSnap = getCurrentDelta();
                const float error = std::abs(deltaAfterSnap - deltaBeforeSnap);

                std::cout << "Ramp complete: deltaBeforeSnap=" << deltaBeforeSnap
                          << " deltaAfterSnap=" << deltaAfterSnap << " target=" << m_targetDelta << " error=" << error
                          << "\n";

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

  private:
    float m_sampleRate;
    double m_position{0.0};
    double m_advance{1.0};

    float m_targetDelta{0.0f};
    float m_maxAdvance{1.5f};
    size_t m_totalSteps{0};

    TransitionPhase m_currentPhase{TransitionPhase::Idle};
    size_t m_currentStep{0};
    bool m_reducingDelta{false};

    float m_referencePosition{0.0f};
    Scheduled m_scheduled{false, 0.0f, 0.0f};
};

} // namespace AbacDsp
