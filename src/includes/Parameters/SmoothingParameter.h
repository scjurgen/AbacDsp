#pragma once

#include <algorithm>
#include <cmath>

namespace AbacDsp
{
class LinearSmoothing
{
  public:
    LinearSmoothing(const float defaultValue = 1.f) noexcept
        : m_newValue(defaultValue)
        , m_currentValue(defaultValue)
    {
    }

    void forceCurrentValue(const float value) noexcept
    {
        m_currentValue = value;
        m_newValue = value;
        m_transitionSteps = 0;
    }

    [[nodiscard]] bool hasStoppedSmoothing() const noexcept
    {
        return m_transitionSteps == 0;
    }

    [[nodiscard]] float getValue(const size_t transitions = 1) noexcept
    {
        if (m_transitionSteps != 0)
        {
            m_transitionSteps -= std::min(transitions, m_transitionSteps);
            m_currentValue += m_delta * transitions;
            if (m_transitionSteps == 0)
            {
                m_currentValue = m_newValue;
            }
        }
        return m_currentValue;
    }

    [[nodiscard]] float getLastValue() const noexcept
    {
        return m_currentValue;
    }

    void newTransition(const float newValue, const float smoothingTimeInSeconds, const float sampleRate,
                       const bool forceValue = false) noexcept
    {
        if (newValue == m_currentValue)
        {
            m_transitionSteps = 0;
            return;
        }
        if (forceValue)
        {
            m_transitionSteps = 0;
            m_currentValue = newValue;
            return;
        }
        m_newValue = newValue;
        const auto numSamplesForTransition = smoothingTimeInSeconds * sampleRate;
        m_transitionSteps = static_cast<size_t>(std::floor(numSamplesForTransition));
        if (m_transitionSteps > 0)
        {
            m_delta = (m_newValue - m_currentValue) / static_cast<float>(numSamplesForTransition);
        }
        else
        {
            m_currentValue = m_newValue;
        }
    }

  private:
    float m_newValue{1.0f};
    float m_currentValue{1.0f};
    float m_delta{0.0f};
    size_t m_transitionSteps{0U};
};

}