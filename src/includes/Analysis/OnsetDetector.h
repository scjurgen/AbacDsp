#pragma once

#include <cmath>
#include <cstddef>

namespace AbacDsp
{

/**
 * @ingroup analysis
 * @brief Hysteresis (Schmitt-trigger) onset detector: one envelope, two thresholds.
 *
 * An onset fires when the envelope rises past the upper threshold; it only re-arms once the
 * envelope has since dropped past the lower one. A held/strummed chord's rippling, beating
 * sustain rarely dips far enough to re-arm, so it fires once rather than re-triggering on every
 * ripple. The tradeoff: a genuinely new attack played while the previous one is still loud
 * (within the hysteresis band) will not register as a separate hit.
 */
class OnsetDetector
{
  public:
    explicit OnsetDetector(const float sampleRate) noexcept
        : m_attackCoeff(timeConstant(kAttackTimeMs, sampleRate))
        , m_releaseCoeff(timeConstant(kReleaseTimeMs, sampleRate))
        , m_minGapSamples(static_cast<size_t>(kMinGapMs * 0.001f * sampleRate))
    {
    }

    void reset() noexcept
    {
        m_envelope = 0.f;
        m_armed = true;
        m_gapCounter = 0;
    }

    // Feed one input sample; returns true on the sample an onset is detected on.
    [[nodiscard]] bool step(const float sample) noexcept
    {
        const float x = std::abs(sample);
        const float coeff = (x > m_envelope) ? m_attackCoeff : m_releaseCoeff;
        m_envelope = coeff * m_envelope + (1.f - coeff) * x;

        if (m_gapCounter > 0)
        {
            --m_gapCounter;
        }
        if (m_armed && m_envelope >= kUpperThreshold && m_gapCounter == 0)
        {
            m_armed = false;
            m_gapCounter = m_minGapSamples;
            return true;
        }
        if (!m_armed && m_envelope < kLowerThreshold)
        {
            m_armed = true;
        }
        return false;
    }

  private:
    static constexpr float kUpperThreshold{0.08f};  // linear, roughly -22 dBFS: fires an onset
    static constexpr float kLowerThreshold{0.048f}; // must drop below this to re-arm
    static constexpr float kAttackTimeMs{1.f};
    static constexpr float kReleaseTimeMs{60.f};
    static constexpr float kMinGapMs{20.f}; // debounces one physical attack edge, not a musical rest

    [[nodiscard]] static float timeConstant(const float timeMs, const float sampleRate) noexcept
    {
        return std::exp(-1.f / (timeMs * 0.001f * sampleRate));
    }

    float m_attackCoeff;
    float m_releaseCoeff;
    size_t m_minGapSamples;
    float m_envelope{0.f};
    bool m_armed{true};
    size_t m_gapCounter{0};
};

}
