#pragma once

#include <cmath>
#include <numbers>
#include <random>

#include "Filters/OnePoleFilter.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Parameters/SmoothingParameter.h"

namespace AbacDsp
{

/**
 * @ingroup modulation
 * @brief Slow tape speed drift, from a mean-reverting random process rather than an LFO.
 *
 * Wow is the slow end of tape speed error, below roughly 6 Hz, where flutter is
 * the fast end. The source is an Ornstein-Uhlenbeck process, not a periodic
 * oscillator: real transport drift wanders and returns without ever repeating,
 * and a listener picks out a repeating cycle immediately.
 *
 * Variance and drift shape that wandering independently of its depth.
 * @see https://en.wikipedia.org/wiki/Wow_and_flutter
 */
class Wow
{
  public:
    /// Exponent for the perceptual depth taper; tuned by ear against alternatives such as 2.5.
    static constexpr float perceptualDepthExponent{3.0f};

    explicit Wow(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_invSampleRate(1.0f / sampleRate)
        , m_lowpass(sampleRate)
        , m_depthLowpass(sampleRate)
        , m_ouProcess(sampleRate)
        , m_rng(std::random_device{}())
    {
        m_rateSmoothed.newTransition(1.f, defaultSmoothingTime, m_sampleRate, true);
        m_varianceSmoothed.newTransition(0.0f, defaultSmoothingTime, m_sampleRate, true);

        m_lowpass.setCutoff(10.0f);
        m_depthLowpass.setCutoff(10.0f);
    }

    void seed(const std::mt19937::result_type seed) noexcept
    {
        m_rng.seed(seed);
        m_ouProcess.seed(seed + 1);
    }

    void setRate(const float v) noexcept
    {
        m_rateSmoothed.newTransition(v, defaultSmoothingTime, m_sampleRate);
    }

    void setDepth(const float v) noexcept
    {
        m_depth = v;
    }

    /// @brief Sets depth through a power taper so a linear control feels natural.
    /// Low settings barely modulate and depth only climbs steeply near the top of the range.
    void setPerceptualDepth(const float v) noexcept
    {
        setDepth(std::pow(v, perceptualDepthExponent));
    }

    void setVariance(const float v) noexcept
    {
        m_varianceSmoothed.newTransition(v, defaultSmoothingTime, m_sampleRate);
    }

    void setDrift(const float v) noexcept
    {
        m_drift = v;
    }

    void setDriftRate(const float rateHz) noexcept
    {
        m_driftRate = std::clamp(rateHz, 0.01f, 0.5f);
    }

    // use sparingly, this stuff is CPU heavy
    [[nodiscard]] float step() noexcept
    {
        if (!m_varianceSmoothed.hasStoppedSmoothing())
        {
            m_ouProcess.setSigma(m_varianceSmoothed.getValue());
        }

        auto rate = m_rateSmoothed.getValue();
        const auto depth = m_depthLowpass.step(m_depth);

        if (m_drift > 0.0f)
        {
            // Stochastic drift: low-frequency random walk with mean reversion
            // This creates slow, correlated changes rather than pure sinusoidal drift
            const auto driftNoise = m_uniformDist(m_rng) * 0.002f;
            const auto meanReversion = -m_driftState / m_driftTimeConstant * m_invSampleRate;

            m_driftState += driftNoise + meanReversion;
            m_driftState = std::clamp(m_driftState, -0.1f, 0.1f);
            const auto driftModulation = 1.0f + m_drift * m_driftState;
            rate *= driftModulation;
        }
        const auto angleDelta = std::numbers::pi_v<float> * 2.0f * rate * m_invSampleRate;
        m_phase += angleDelta;

        while (m_phase >= std::numbers::pi_v<float> * 2.0f)
        {
            m_phase -= std::numbers::pi_v<float> * 2.0f;
        }

        const auto ouValue = m_ouProcess.step();
        const auto filteredOU = m_lowpass.step(ouValue);

        const auto currentDelay = depth * (std::sin(m_phase) + filteredOU);

        const auto delayDerivative = (currentDelay - m_previousDelay) * m_sampleRate / 1000.0f;
        m_previousDelay = currentDelay;

        return delayDerivative;
    }

  private:
    static constexpr float defaultSmoothingTime{0.01f};

    const float m_sampleRate;
    const float m_invSampleRate;

    LinearSmoothing m_rateSmoothed;
    float m_depth{1.f};
    LinearSmoothing m_varianceSmoothed;
    float m_drift{0.f};

    float m_phase{0.f};
    float m_driftRate{0.1f};         // Drift frequency in Hz
    float m_driftState{0.f};         // Current drift state (random walk)
    float m_driftTimeConstant{20.f}; // Time constant for drift evolution (~20-50 seconds)

    float m_previousDelay{0.f};

    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> m_lowpass;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> m_depthLowpass;
    OrnsteinUhlenbeckProcess m_ouProcess;

    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_uniformDist{-0.5f, 0.5f};
};

}