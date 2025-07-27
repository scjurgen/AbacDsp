#pragma once

#include <algorithm>
#include <cmath>
#include <string>

template <size_t BlockSize>
class LinearSmoothingParameter
{
  public:
    explicit LinearSmoothingParameter(const float initialValue = 0.0f)
        : m_value(initialValue)
    {
        forceValue(initialValue);
    }

    void setMin(const float min)
    {
        m_min = min;
    }

    void setMax(const float max)
    {
        m_max = max;
    }

    void setMoniker(const std::string& moniker)
    {
        m_moniker = moniker;
    }

    [[nodiscard]] std::string getMoniker() const
    {
        return m_moniker;
    }

    void forceValue(const float newValue)
    {
        m_value = std::clamp(newValue, m_min, m_max);
        std::fill(m_values.begin(), m_values.end(), m_value);
    }

    void setValue(const float newValue)
    {
        const auto target = std::clamp(newValue, m_min, m_max);
        const auto delta = target - m_value;
        if (delta == 0.0f)
        {
            std::fill(m_values.begin(), m_values.end(), m_value);
            return;
        }
        std::generate(m_values.begin(), m_values.end(),
                      [dt = delta / static_cast<float>(BlockSize), v = &this->m_value]() mutable
                      {
                          *v += dt;
                          return *v;
                      });
    }

    [[nodiscard]] float getValue(size_t index) const
    {
        return m_values[index];
    }

  private:
    float m_value{0.0f};
    float m_min{0.0f};
    float m_max{1.0f};
    std::string m_moniker{};
    std::array<float, BlockSize> m_values{};
};

class LinearParameter
{
  public:
    explicit LinearParameter(const float initialValue = 0.0f)
        : m_value(initialValue)
        , m_target(initialValue)
        , m_stepsRemaining(0)
        , m_stepApply(0.0f)
    {
    }

    void setMin(const float min)
    {
        m_min = min;
        updateStep();
    }

    void setMax(const float max)
    {
        m_max = max;
        updateStep();
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
        updateStep();
    }

    void setMoniker(const std::string& moniker)
    {
        m_moniker = moniker;
    }

    std::string getMoniker() const
    {
        return m_moniker;
    }

    void setTransitionTime(const float seconds)
    {
        m_transitionTime = seconds;
        updateStep();
    }

    void forceValue(const float newValue)
    {
        m_target = std::clamp(newValue, m_min, m_max);
        m_value = m_target;
        m_stepsRemaining = 0;
    }

    void setValue(const float newValue)
    {
        m_target = std::clamp(newValue, m_min, m_max);
        const float delta = m_target - m_value;
        m_stepsRemaining = static_cast<int>(std::abs(delta / m_step));
        m_stepApply = m_stepsRemaining > 0 ? delta / m_stepsRemaining : 0.0f;
        if (m_stepsRemaining == 0)
        {
            m_value = m_target;
        }
    }

    float getValue() const
    {
        return m_value;
    }

    bool isTransitioning() const
    {
        return m_stepsRemaining > 0;
    }

    void tick()
    {
        if (m_stepsRemaining >= 1)
        {
            m_value += m_stepApply;
            m_stepsRemaining--;
            if (m_stepsRemaining == 0)
            {
                m_value = m_target;
                m_stepsRemaining = 0;
            }
        }
    }

  private:
    float m_value = 0.0f;
    float m_target = 0.0f;
    float m_min = 0.0f;
    float m_max = 100.0f;
    float m_step = 0.001f;
    float m_sampleRate = 48000.0f;
    float m_transitionTime = 1.0f;
    std::string m_moniker;
    int m_stepsRemaining = 0;
    float m_stepApply = 0.0f;

    void updateStep()
    {
        m_step = std::abs(m_max - m_min) / (m_transitionTime * m_sampleRate);
    }
};
