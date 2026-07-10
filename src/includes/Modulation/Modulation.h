#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace AbacDsp
{

/*
 * Triangle position modulator for modulated delay read heads.
 * tick() advances one step; lastValuePair() yields the integer offset and
 * the fractional part for interpolated buffer reads.
 * Speed and depth changes are deferred to a direction turnaround so the
 * read position never jumps.
 */
class Modulation
{
  public:
    explicit Modulation(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void setModulationDepth(const float depth) noexcept
    {
        if (!std::equal_to<float>{}(m_depth, depth))
        {
            m_depth = depth;
            m_hasNewData = true;
        }
    }

    void setModulationSpeed(const float speedHz) noexcept
    {
        m_speedHz = speedHz;
        m_hasNewData = true;
    }

    [[nodiscard]] bool isModulating() const noexcept
    {
        return m_activeModulation;
    }

    [[nodiscard]] std::pair<int, float> lastValuePair() const noexcept
    {
        return {std::max(m_intPos, 0), m_fraction};
    }

    [[nodiscard]] float lastValue() const noexcept
    {
        return static_cast<float>(m_intPos) + m_fraction;
    }

    void tick() noexcept
    {
        m_position++;

        if (m_position >= m_totalSteps)
        {
            m_advance = -m_advance;
            if (m_hasNewData && m_negativeDirection)
            {
                setNewAdvance();
            }
            m_negativeDirection = !m_negativeDirection;
            m_position = 0;
        }
        m_fraction += m_advance;
        if (m_negativeDirection)
        {
            while (m_fraction < 0.f)
            {
                m_fraction += 1.f;
                m_intPos--;
            }
        }
        else
        {
            while (m_fraction >= 1.f)
            {
                m_fraction -= 1.f;
                m_intPos++;
            }
        }
    }

  private:
    void setNewAdvance() noexcept
    {
        m_activeModulation = m_depth > 0;
        m_hasNewData = false;
        m_advance = m_depth * 2 * m_speedHz / m_sampleRate;
        m_totalSteps = static_cast<int>(std::round(m_sampleRate / m_speedHz / 2));
    }

    float m_sampleRate;

    bool m_negativeDirection{false};
    int m_position{0};
    int m_totalSteps{0};

    int m_intPos{1};
    float m_fraction{0.f};
    float m_advance{0.f};
    float m_speedHz{1.f};
    float m_depth{100.f};
    bool m_activeModulation{false};
    bool m_hasNewData{false};
};

}
