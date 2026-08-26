#pragma once

#include <algorithm>

#include "Analysis/EnvelopeFollower.h"
#include "Numbers/Convert.h"

namespace AbacDsp
{

/**
 * @ingroup dynamics
 * @brief Feedforward peak compressor with a fixed quadratic soft knee.
 *
 * The envelope is AbacDsp::PeakEnvelopeFollower (attack/release ballistics
 * already implemented there); this class adds the gain computer - a
 * standard soft-knee curve, ratio-derived slope above the knee, unity gain
 * below it. Knee width and makeup gain are fixed rather than exposed, per a
 * deliberate first-version scope cut: both are easy to add to the
 * constructor/signature later if a script ever needs them.
 * @see https://www.eecs.qmul.ac.uk/~josh/documents/2012/GiannoulisMassbergReiss-dynamicrangecompression-JAES2012.pdf
 */
class Compressor
{
  public:
    static constexpr size_t kEnvelopeDbRange = 60;
    static constexpr float kKneeDb = 3.f;

    explicit Compressor(const float sampleRate) noexcept
        : m_envelope(sampleRate)
    {
        setAttackMs(10.f);
        setReleaseMs(100.f);
    }

    void setThresholdDb(const float value) noexcept
    {
        m_thresholdDb = value;
    }

    void setRatio(const float value) noexcept
    {
        m_ratio = std::max(value, 1.f);
    }

    void setAttackMs(const float value) noexcept
    {
        m_envelope.setAttackInMsecs(std::max(value, 0.01f));
    }

    void setReleaseMs(const float value) noexcept
    {
        m_envelope.setReleaseInMsecs(std::max(value, 0.01f));
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        const auto envelopeDb = Convert::gainToDb(std::max(m_envelope.step(in), 1e-8f));
        const auto overshoot = envelopeDb - m_thresholdDb;
        const auto slope = 1.f / m_ratio - 1.f;

        float gainReductionDb;
        if (overshoot <= -kKneeDb * 0.5f)
        {
            gainReductionDb = 0.f;
        }
        else if (overshoot >= kKneeDb * 0.5f)
        {
            gainReductionDb = slope * overshoot;
        }
        else
        {
            const auto x = overshoot + kKneeDb * 0.5f;
            gainReductionDb = slope * x * x / (2.f * kKneeDb);
        }
        return in * Convert::dbToGain(gainReductionDb);
    }

  private:
    PeakEnvelopeFollower<kEnvelopeDbRange> m_envelope;
    float m_thresholdDb{0.f};
    float m_ratio{1.f};
};

}
