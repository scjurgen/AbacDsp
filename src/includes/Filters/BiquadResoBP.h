#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>

namespace AbacDsp
{

/*
 * we have two sets of coefficients, one for normal operation,
 * one joker, for e.g. fast decay (q=0.1)
 */
class BiquadResoBP
{
    struct BandPassCoefficients
    {
        float b0{};
        float a1{};
        float a2{};
    };

  public:
    explicit BiquadResoBP(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        computeCoefficients(0, 1000.f);
        computeCoefficients(1, 1000.f);
    }

    BiquadResoBP()
    {
        computeCoefficients(0, 1000.f);
        computeCoefficients(1, 1000.f);
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    float m_sampleRate{48000.f};

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
        constexpr auto k = 0.1447648273f; // 1.f / std::log(1000.f); //  1/6.9078f
        const float Q = std::numbers::pi_v<float> * m_frequency * t * k;
        const auto kqCl = K / std::max(Q, 0.01f);
        const auto norm = 1.f / (1 + kqCl + kSquare);
        m_cf[index].b0 = kqCl * norm;
        m_cf[index].a1 = 2 * (kSquare - 1) * norm;
        m_cf[index].a2 = (1 - kqCl + kSquare) * norm;
    }

    void computeCoefficients(const size_t index, const float frequency,
                             const float Q = 1.f / std::numbers::sqrt2_v<float>) noexcept
    {
        m_frequency = frequency;
        Fc = frequency / m_sampleRate;
        K = std::tan(std::numbers::pi_v<float> * Fc);
        kSquare = K * K;

        const auto kqCl = K / std::max(Q, 0.01f);
        const auto norm = 1.f / (1 + kqCl + kSquare);
        m_cf[index].b0 = kqCl * norm;
        m_cf[index].a1 = 2 * (kSquare - 1) * norm;
        m_cf[index].a2 = (1 - kqCl + kSquare) * norm;
    }

    float step(const float in) noexcept
    {
        const auto b0s = in * m_cf[m_currentSet].b0;
        const auto out = b0s + m_z[0];
        m_z[0] = m_z[1] - m_cf[m_currentSet].a1 * out;
        m_z[1] = -b0s - m_cf[m_currentSet].a2 * out;
        return out;
    }

    float step0() noexcept
    {
        const auto out = m_z[0];
        m_z[0] = m_z[1] - m_cf[m_currentSet].a1 * out;
        m_z[1] = -m_cf[m_currentSet].a2 * out;
        return out;
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

    [[nodiscard]] float magnitude(const size_t index, const float hz, const float sampleRate = 48000.f) const noexcept
    {
        const auto b0 = static_cast<double>(m_cf[index].b0);
        const auto a1 = static_cast<double>(m_cf[index].a1);
        const auto a2 = static_cast<double>(m_cf[index].a2);
        const auto phi = 4 * std::pow(std::sin(2 * std::numbers::pi_v<double> * hz / sampleRate / 2), 2);
        const auto db =
            10 * std::log10(std::pow((b0 + 0 + -b0), 2) + (b0 * -b0 * phi - (0 * (b0 + -b0) + 4 * b0 * -b0)) * phi) -
            10 * std::log10(std::pow((1.f + a1 + a2), 2) + (a2 * phi - (a1 * (1 + a2) + 4 * a2)) * phi);
        return static_cast<float>(db);
    }

    void damp(const bool damp)
    {
        m_currentSet = damp ? 1 : 0;
    }

    bool isActive() noexcept
    {
        if (m_decayCount > 0)
        {
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

    void triggered()
    {
        m_decayCount = m_decayMax;
    }

  private:
    size_t m_currentSet{0};
    int m_decayCount{0};
    int m_decayMax{0};
    int m_inActiveCount{0};
    std::array<BandPassCoefficients, 2> m_cf{};
    std::array<float, 2> m_z{};
    float m_frequency{};
    float Fc{};
    float K{};
    float kSquare{};
};

}