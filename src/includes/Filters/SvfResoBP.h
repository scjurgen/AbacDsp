#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace AbacDsp
{

class SvfResoBP
{
    struct BandPassCoefficients
    {
        float g{};
        float k{};
        float a1{};
        float a2{};
        float a3{};
    };

  public:
    explicit SvfResoBP(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        computeCoefficients(0, 1000.f);
        computeCoefficients(1, 1000.f);
    }

    SvfResoBP() = default;

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    void setByDecay(const size_t index, const float frequency, const float t)
    {
        m_decayMax = static_cast<int>(m_sampleRate * t);
        constexpr auto k = 0.1447648273f;
        float Q = std::numbers::pi_v<float> * frequency * t * k;
        computeCoefficients(index, frequency, Q);
    }

    void setDecay(const size_t index, const float t)
    {
        m_decayMax = static_cast<int>(m_sampleRate * t * 0.001f);
        constexpr auto k = 0.1447648273f;
        const float Q = std::numbers::pi_v<float> * m_frequency * t * k;
        updateK(index, Q);
    }

    void pitchBend(const float cents) noexcept
    {
        m_pitchBend = cents;
        recomputeCoefficientsWithBend(m_currentSet);
    }

    void computeCoefficients(const size_t index, const float frequency,
                             const float Q = 1.f / std::numbers::sqrt2_v<float>) noexcept
    {
        m_frequency = frequency;
        m_pitchBend = 0.f;
        const float k = 1.f / std::max(Q, 0.01f);
        m_cf[index].k = k;
        recomputeCoefficientsWithBend(index);
    }

    void updateK(const size_t index, const float Q) noexcept
    {
        const float g = m_cf[index].g;
        const float k = 1.f / std::max(Q, 0.01f);
        const float denom = 1.f / (1.f + g * (g + k));
        m_cf[index].k = k;
        m_cf[index].a1 = denom;
        m_cf[index].a2 = g * denom;
        m_cf[index].a3 = g * m_cf[index].a2;
    }

    float step(const float in) noexcept
    {
        const auto& cf = m_cf[m_currentSet];
        const float v3 = in - m_z[1];
        const float v1 = cf.a1 * m_z[0] + cf.a2 * v3;
        const float v2 = m_z[1] + cf.a2 * m_z[0] + cf.a3 * v3;
        m_z[0] = 2.f * v1 - m_z[0];
        m_z[1] = 2.f * v2 - m_z[1];
        return cf.k * v1;
    }

    float step0() noexcept
    {
        const auto& cf = m_cf[m_currentSet];
        const float v3 = -m_z[1];
        const float v1 = cf.a1 * m_z[0] + cf.a2 * v3;
        const float v2 = m_z[1] + cf.a2 * m_z[0] + cf.a3 * v3;
        m_z[0] = 2.f * v1 - m_z[0];
        m_z[1] = 2.f * v2 - m_z[1];
        return cf.k * v1;
    }

    void process(const float* in, float* outBuffer, const size_t numSamples) noexcept
    {
        std::transform(in, in + numSamples, outBuffer, [this](const float v) { return step(v); });
    }

    void process0(float* outBuffer, const size_t numSamples) noexcept
    {
        std::generate_n(outBuffer, numSamples, [this] { return step0(); });
    }

    void reset(const float v1 = 0.f, const float v2 = 0.f) noexcept
    {
        m_z[0] = v1;
        m_z[1] = v2;
    }

    void pump(const float f) noexcept
    {
        m_z[0] *= f;
        m_z[1] *= f;
    }

    [[nodiscard]] float currentMagnitudeSquared() const noexcept
    {
        return m_z[0] * m_z[0] + m_z[1] * m_z[1];
    }

    [[nodiscard]] float currentMagnitude() const noexcept
    {
        return std::sqrt(currentMagnitudeSquared());
    }

    void damp(const bool damp) noexcept
    {
        m_currentSet = damp ? 1 : 0;
    }

    bool isActive() noexcept
    {
        if (m_decayCount > 0)
        {
            m_decayCount--;
            return true;
        }

        if (std::abs(m_z[0]) > 1E-6f || std::abs(m_z[1]) > 1E-6f)
        {
            m_inActiveCount = 0;
        }
        else
        {
            m_inActiveCount++;
        }

        return m_inActiveCount < 32;
    }

    void triggered() noexcept
    {
        m_decayCount = m_decayMax;
    }

  private:
    void recomputeCoefficientsWithBend(const size_t index) noexcept
    {
        constexpr float centsToOctave = 1.f / 1200.f;
        const float ratio = std::exp2f(m_pitchBend * centsToOctave);
        const float bendFrequency = m_frequency * ratio;
        const float g = std::tan(std::numbers::pi_v<float> * bendFrequency / m_sampleRate);
        const float k = m_cf[index].k;
        const float denom = 1.f / (1.f + g * (g + k));
        m_cf[index].g = g;
        m_cf[index].a1 = denom;
        m_cf[index].a2 = g * denom;
        m_cf[index].a3 = g * m_cf[0].a2;
    }

    float m_sampleRate{48000.f};
    size_t m_currentSet{0};
    int m_decayCount{0};
    int m_decayMax{0};
    int m_inActiveCount{0};
    std::array<BandPassCoefficients, 2> m_cf{};
    std::array<float, 2> m_z{};
    float m_frequency{};
    float m_pitchBend{0.f};
};

} // namespace AbacDsp
