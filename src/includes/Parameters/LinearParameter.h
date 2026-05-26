#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace AbacDsp
{
template <size_t BlockSize>
class LinearSmoothingParameter
{
  public:
    explicit LinearSmoothingParameter(const float initialValue = 0.0f)
        : m_value(initialValue)
    {
        forceValue(initialValue);
    }

    void setMin(const float min) noexcept
    {
        m_min = min;
    }

    void setMax(const float max) noexcept
    {
        m_max = max;
    }

    void setMoniker(const std::string_view moniker)
    {
        m_moniker = moniker;
    }

    [[nodiscard]] std::string getMoniker() const
    {
        return m_moniker;
    }

    void forceValue(const float newValue) noexcept
    {
        m_value = std::clamp(newValue, m_min, m_max);
        std::ranges::fill(m_values, m_value);
    }

    void setValue(const float newValue) noexcept
    {
        const auto target = std::clamp(newValue, m_min, m_max);
        const auto delta = target - m_value;
        if (delta == 0.0f)
        {
            std::ranges::fill(m_values, m_value);
            return;
        }
        std::generate(m_values.begin(), m_values.end(),
                      [dt = delta / static_cast<float>(BlockSize), v = &this->m_value]() mutable
                      {
                          *v += dt;
                          return *v;
                      });
    }

    [[nodiscard]] float getValue(const size_t index) const noexcept
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

    void setMoniker(const std::string_view moniker)
    {
        m_moniker = moniker;
    }

    [[nodiscard]] std::string getMoniker() const
    {
        return m_moniker;
    }

    void setTransitionTime(const float seconds) noexcept
    {
        m_transitionTime = seconds;
        updateStep();
    }

    void forceValue(const float newValue) noexcept
    {
        m_target = std::clamp(newValue, m_min, m_max);
        m_value = m_target;
        m_stepsRemaining = 0;
    }

    void setValue(const float newValue) noexcept
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

    [[nodiscard]] float getValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] bool isTransitioning() const noexcept
    {
        return m_stepsRemaining > 0;
    }

    void tick() noexcept
    {
        if (m_stepsRemaining >= 1)
        {
            m_value += m_stepApply;
            m_stepsRemaining--;
            if (m_stepsRemaining == 0)
            {
                m_value = m_target;
            }
        }
    }

  private:
    float m_value{0.0f};
    float m_target{0.0f};
    float m_min{0.0f};
    float m_max{100.0f};
    float m_step{0.001f};
    float m_sampleRate{48000.0f};
    float m_transitionTime{1.0f};
    std::string m_moniker{};
    int m_stepsRemaining{0};
    float m_stepApply{0.0f};

    void updateStep()
    {
        m_step = std::abs(m_max - m_min) / (m_transitionTime * m_sampleRate);
    }
};
}