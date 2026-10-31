#pragma once

#include <algorithm>

#include "Filters/Biquad.h"

namespace AbacDsp
{

/**
 * @ingroup filters
 * @brief Sample-by-sample 4th-order Linkwitz-Riley split into a low and a high band.
 *
 * Each band is two cascaded Butterworth biquads (Q = 1/sqrt(2)) at the same frequency, so both bands are -6 dB at the
 * split frequency and in phase there. low + high is not the input but a 2nd-order allpass of it: flat magnitude, phase
 * lagging from 0 to -360 degrees. Whoever recombines the bands after processing one of them gets that allpass when the
 * processing is neutral.
 *
 * The frequency is clamped to [minFrequency, maxFrequencyRatio * sampleRate]. Setting it changes the coefficients at
 * once and keeps the states, so a jump can click; a caller that sweeps it sets it in small steps. Same filter as
 * Graph::Nodes::CrossoverLR4, which works on blocks and ports.
 * @see Linkwitz, S. H. (1976), "Active crossover networks for noncoincident drivers", JAES 24(1), 2-8
 * @see https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter
 */
class LinkwitzRileySplit
{
  public:
    struct Bands
    {
        float low{0.0f};
        float high{0.0f};
    };

    static constexpr float minFrequency{10.0f};
    static constexpr float maxFrequencyRatio{0.45f};
    static constexpr float defaultFrequency{1000.0f};

    explicit LinkwitzRileySplit(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        updateCoefficients();
    }

    void setSampleRate(const float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
        updateCoefficients();
    }

    void setFrequency(const float hz) noexcept
    {
        m_frequencyHz = hz;
        updateCoefficients();
    }

    /// The frequency in use, after clamping.
    [[nodiscard]] float getFrequency() const noexcept
    {
        return clampedFrequency();
    }

    [[nodiscard]] Bands step(const float in) noexcept
    {
        const float low = m_lowStage2.singleStepLowPass(m_lowStage1.singleStepLowPass(in));
        const float high = m_highStage2.singleStepHighPass(m_highStage1.singleStepHighPass(in));
        return {low, high};
    }

    void reset() noexcept
    {
        m_lowStage1.reset();
        m_lowStage2.reset();
        m_highStage1.reset();
        m_highStage2.reset();
    }

  private:
    static constexpr float kQ{0.70710678f};

    [[nodiscard]] float clampedFrequency() const noexcept
    {
        return std::clamp(m_frequencyHz, minFrequency, maxFrequencyRatio * m_sampleRate);
    }

    void updateCoefficients() noexcept
    {
        const float frequency = clampedFrequency();
        m_lowStage1.computeCoefficients(m_sampleRate, frequency, kQ, 0.0f);
        m_lowStage2.computeCoefficients(m_sampleRate, frequency, kQ, 0.0f);
        m_highStage1.computeCoefficients(m_sampleRate, frequency, kQ, 0.0f);
        m_highStage2.computeCoefficients(m_sampleRate, frequency, kQ, 0.0f);
    }

    float m_sampleRate;
    float m_frequencyHz{defaultFrequency};
    Biquad<BiquadFilterType::LowPass> m_lowStage1;
    Biquad<BiquadFilterType::LowPass> m_lowStage2;
    Biquad<BiquadFilterType::HighPass> m_highStage1;
    Biquad<BiquadFilterType::HighPass> m_highStage2;
};

} // namespace AbacDsp
