#pragma once

#include <cmath>
#include <cstddef>

#include "Numbers/EasyingFunctions.h"

namespace AbacDsp
{

enum class TransitionPhase
{
    Idle,
    Ramping
};

// quartic=true: smoother ease in/out; quartic=false: more aggressive quadratic acceleration
template <size_t WrapSize, bool quartic = false>
class FracReadHead
{
  public:
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
        m_maxAdvance = m_reducingDelta ? (1 / maxAdvance) : maxAdvance;

        const float advanceDeviation = m_maxAdvance - 1.0f;
        m_totalSteps = static_cast<size_t>(std::ceil(std::abs(1.5f * deltaDifference / advanceDeviation)));

        if (m_totalSteps == 0)
        {
            return;
        }

        m_currentPhase = TransitionPhase::Ramping;
        m_currentStep = 0;
    }

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
    const float m_sampleRate;
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

}  // namespace AbacDsp
