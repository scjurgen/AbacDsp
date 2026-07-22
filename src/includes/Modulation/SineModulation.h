#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace AbacDsp
{

/*
 * Raised-cosine position modulator for modulated delay read heads - same role and public
 * interface as Modulation (triangle position sweep), but the position's rate of change is
 * itself a sine, not a square wave. A triangle sweep has a *constant* velocity that flips sign
 * at each turnaround, which turns into an abrupt, two-state pitch shift once the swing is large
 * enough to be audible (see Modulation_test.cpp, RateOfChangeIsTwoConstantValuesNotASmoothCurve).
 * This sweeps position as depth/2 * (1 - cos(phase)), from 0 up to depth and back over one full
 * cycle, whose derivative varies continuously - a smooth vibrato instead of two alternating
 * pitches.
 * Speed and depth changes are deferred to the trough (phase == 0, position == 0) so the read
 * position never jumps, matching Modulation's turnaround-deferred update.
 */
class SineModulation
{
  public:
    explicit SineModulation(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void setModulationDepth(const float depth) noexcept
    {
        m_newDepth = depth;
        m_hasNewData = true;
    }

    void setModulationSpeed(const float speedHz) noexcept
    {
        m_newSpeedHz = speedHz;
        m_hasNewData = true;
    }

    [[nodiscard]] bool isModulating() const noexcept
    {
        return m_activeModulation;
    }

    [[nodiscard]] std::pair<int, float> lastValuePair() const noexcept
    {
        const auto intPos = static_cast<int>(m_position);
        const auto fraction = m_position - static_cast<float>(intPos);
        return {std::max(intPos, 0), fraction};
    }

    [[nodiscard]] float lastValue() const noexcept
    {
        return m_position;
    }

    void tick() noexcept
    {
        // m_phaseAdvance starts at 0 (nothing applied yet), which would otherwise make the
        // "wrapped past the trough" check below always false and the pending depth/speed never
        // take effect at all - so treat "never yet initialised" as its own safe point too.
        const auto atSafePoint = m_phaseAdvance == 0.0f || m_phase < m_phaseAdvance;
        if (m_hasNewData && atSafePoint)
        {
            applyPendingChange();
        }

        m_phase += m_phaseAdvance;
        if (m_phase >= twoPi)
        {
            m_phase -= twoPi;
        }
        m_position = m_depth * 0.5f * (1.0f - std::cos(m_phase));
    }

  private:
    static constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    void applyPendingChange() noexcept
    {
        m_depth = m_newDepth;
        m_speedHz = m_newSpeedHz;
        m_activeModulation = m_depth > 0.0f;
        m_phaseAdvance = twoPi * m_speedHz / m_sampleRate;
        m_hasNewData = false;
    }

    const float m_sampleRate;
    float m_phase{0.0f};
    float m_phaseAdvance{0.0f};
    float m_position{0.0f};
    float m_depth{0.0f};
    float m_speedHz{1.0f};
    float m_newDepth{100.0f};
    float m_newSpeedHz{1.0f};
    bool m_activeModulation{false};
    bool m_hasNewData{true};
};

}
