#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace AbacDsp
{

/**
 * @ingroup filters
 * @brief State variable filter (SVF) giving simultaneous low/high/band/notch outputs from one step.
 *
 * Same topology-preserving trapezoidal integration as SvfResoBP - g = tan(pi*f/fs),
 * k = 1/Q - generalised to hand back every standard combination of the two integrator
 * states instead of committing to the bandpass tap alone. Where SvfResoBP is tuned for a
 * decaying resonant ring, this is the general-purpose building block for a
 * per-sample-modulation-safe low/high/band/notch filter.
 * @see https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
 */
class SvfMultiMode
{
  public:
    /// @brief Every standard SVF combination from one step() call; pick the field the caller needs.
    struct Outputs
    {
        float low{};
        float high{};
        float band{};
        float notch{};
    };

    explicit SvfMultiMode(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    SvfMultiMode() = default;

    void setSampleRate(const float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }

    void computeCoefficients(const float frequency, const float Q) noexcept
    {
        const auto f = std::clamp(frequency, 1.f, m_sampleRate * 0.499f);
        const auto g = std::tan(std::numbers::pi_v<float> * f / m_sampleRate);
        const auto k = 1.f / std::max(Q, 0.01f);
        m_k = k;
        m_a1 = 1.f / (1.f + g * (g + k));
        m_a2 = g * m_a1;
        m_a3 = g * m_a2;
    }

    [[nodiscard]] Outputs step(const float in) noexcept
    {
        const float v3 = in - m_z1;
        const float v1 = m_a1 * m_z0 + m_a2 * v3;
        const float v2 = m_z1 + m_a2 * m_z0 + m_a3 * v3;
        m_z0 = 2.f * v1 - m_z0;
        m_z1 = 2.f * v2 - m_z1;
        return Outputs{v2, in - m_k * v1 - v2, v1, in - m_k * v1};
    }

    void reset(const float z0 = 0.f, const float z1 = 0.f) noexcept
    {
        m_z0 = z0;
        m_z1 = z1;
    }

  private:
    float m_sampleRate{48000.f};
    float m_k{1.f};
    float m_a1{0.f};
    float m_a2{0.f};
    float m_a3{0.f};
    float m_z0{0.f};
    float m_z1{0.f};
};

}
