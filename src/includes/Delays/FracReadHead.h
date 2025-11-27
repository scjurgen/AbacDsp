#pragma once

#include <cmath>
#include <cassert>
#include <algorithm>

namespace AbacDsp
{

template <size_t WrapSize>
class FracReadHead
{
  public:
    enum class TransitionPhase
    {
        Idle,     // advance = 1.0
        RampIn,   // ramping advance up/down towards max advance
        Constant, // advance at max speed
        RampOut   // ramping advance back to 1.0
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
        size_t rampSteps;
    };
    Scheduled m_scheduled;

    void setNewDelta(const float newTargetDelta, const float maxAdvance, const size_t rampSteps = 2000) noexcept
    {
        if (m_currentPhase != TransitionPhase::Idle)
        {
            m_scheduled.hasNewValues = true;
            m_scheduled.newTargetDelta = newTargetDelta;
            m_scheduled.maxAdvance = maxAdvance;
            m_scheduled.rampSteps = rampSteps;
        }

        const float currentDelta = m_referencePosition - m_position;

        if (std::abs(currentDelta - newTargetDelta) < 0.001f)
        {
            // nothing to do, good enough
            return;
        }

        m_targetDelta = newTargetDelta;
        m_maxAdvance = std::max(0.1f, std::min(10.0f, maxAdvance));
        m_rampSteps = std::max(static_cast<size_t>(1), rampSteps);
        m_advanceRampIn = 0;
        m_reducingDelta = (newTargetDelta < currentDelta);

        if (m_reducingDelta)
        {
            m_advanceIncrement = (m_maxAdvance - 1.0f) / static_cast<float>(m_rampSteps);
        }
        else
        {
            const float minAdvance = 2.0f - m_maxAdvance;
            m_advanceIncrement = (1.0f - minAdvance) / static_cast<float>(m_rampSteps);
        }

        calculateConstantSteps(currentDelta);

        m_currentPhase = TransitionPhase::RampIn;
        m_currentStep = 0;
    }

    float step(const float referencePosition) noexcept
    {
        m_referencePosition = referencePosition;
        updateAdvance();
        m_position += m_advance;
        if (m_position >= WrapSize)
        {
            m_position -= WrapSize;
        }
        return m_position;
    }

    [[nodiscard]] float getCurrentDelta() const noexcept
    {
        return m_referencePosition - m_position;
    }

    TransitionPhase getCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    [[nodiscard]] bool isAdjusting() const noexcept
    {
        return m_currentPhase != TransitionPhase::Idle;
    }

    [[nodiscard]] float getCurrentAdvance() const noexcept
    {
        return m_advance;
    }

    [[nodiscard]] float getCurrentPosition() const noexcept
    {
        return m_position;
    }

  private:
    void calculateConstantSteps(const float currentDelta) noexcept
    {
        // Calculate total delta change from both ramp phases
        const float totalRampDeltaChange = (static_cast<float>(m_rampSteps) * std::abs(m_maxAdvance - 1.0f) - 1) * 0.5f;

        if (const float requiredDeltaChange = std::abs(currentDelta - m_targetDelta);
            requiredDeltaChange <= totalRampDeltaChange)
        {
            m_constantSteps = 0;
        }
        else
        {
            const float constantDeltaChangeNeeded = requiredDeltaChange - totalRampDeltaChange;
            const float deltaPerConstantStep = std::abs(m_maxAdvance - 1.0f);
            m_constantSteps = static_cast<size_t>(constantDeltaChangeNeeded / deltaPerConstantStep);
        }
    }

    void updateAdvance() noexcept
    {
        switch (m_currentPhase)
        {
            case TransitionPhase::Idle:
                m_advance = 1.0f;
                break;

            case TransitionPhase::RampIn:

                if (m_reducingDelta)
                {
                    m_advance = 1.0f + (m_currentStep + 1) * m_advanceIncrement;
                }
                else
                {
                    m_advance = 1.0f - (m_currentStep + 1) * m_advanceIncrement;
                }
                m_advanceRampIn += m_advance;
                m_currentStep++;
                if (m_currentStep >= m_rampSteps)
                {
                    m_currentPhase = (m_constantSteps > 0) ? TransitionPhase::Constant : TransitionPhase::RampOut;
                    m_currentStep = 0;
                }
                break;

            case TransitionPhase::Constant:
                m_advance = m_reducingDelta ? m_maxAdvance : (2.0f - m_maxAdvance);

                m_currentStep++;
                if (m_currentStep >= m_constantSteps)
                {
                    m_currentPhase = TransitionPhase::RampOut;
                    m_currentStep = 0;
                }
                break;

            case TransitionPhase::RampOut:
                if (m_reducingDelta)
                {
                    m_advance = m_maxAdvance - (m_currentStep + 1) * m_advanceIncrement;
                }
                else
                {
                    const float minAdvance = 2.0f - m_maxAdvance;
                    m_advance = minAdvance + (m_currentStep + 1) * m_advanceIncrement;
                }

                m_currentStep++;
                if (m_currentStep >= m_rampSteps)
                {
                    // Transition to Idle - apply final position correction
                    m_currentPhase = TransitionPhase::Idle;

                    m_advance = 1.0f;
                    m_currentStep = 0;
                    if (m_scheduled.hasNewValues)
                    {
                        m_scheduled.hasNewValues = false;
                        setNewDelta(m_scheduled.newTargetDelta, m_scheduled.maxAdvance, m_scheduled.rampSteps);
                        break;
                    }
                    float targetPosition = m_referencePosition - m_targetDelta;
                    if (!m_reducingDelta)
                    {
                        targetPosition -= 1.f;
                    }
                    while (targetPosition < 0.0f)
                    {
                        targetPosition += static_cast<float>(WrapSize);
                    }
                    while (targetPosition >= static_cast<float>(WrapSize))
                    {
                        targetPosition -= static_cast<float>(WrapSize);
                    }

                    m_position = targetPosition;
                }
                break;
        }
        m_advance = std::max(0.01f, std::min(10.0f, m_advance));
    }

  private:
    float m_sampleRate;
    float m_position{0.0f};
    float m_advance{1.0f};

    // Target parameters
    float m_targetDelta{0.0f};
    float m_maxAdvance{2.0f};
    size_t m_rampSteps{16};

    // Phase tracking
    TransitionPhase m_currentPhase{TransitionPhase::Idle};
    size_t m_currentStep{0};
    size_t m_constantSteps{0};
    float m_advanceRampIn{0};

    // Advance calculation
    float m_advanceIncrement{0.0f};
    bool m_reducingDelta{true};

    // Reference position (x1 equivalent)
    float m_referencePosition{0.0f};
};


}