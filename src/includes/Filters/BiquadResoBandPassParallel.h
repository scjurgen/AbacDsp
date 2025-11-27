#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>

namespace AbacDsp
{
template <size_t NumElements, size_t BlockSize>
class BiquadResoBandPassParallel
{
    struct BandPassCoefficients
    {
        float b0{};
        float a1{};
        float a2{};
    };

  public:
    explicit BiquadResoBandPassParallel(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        // computeCoefficients(0, 1000.f);
        // computeCoefficients(1, 1000.f);
    }

    BiquadResoBandPassParallel()
    {
        // computeCoefficients(0, 1000.f);
        // computeCoefficients(1, 1000.f);
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    float m_sampleRate{48000.f};

    void setByDecay(const size_t mainIndex, const size_t index, const float frequency, const float t)
    {
        constexpr auto k = 0.1447648273f; // 1.f / std::log(1000.f); //  1/6.9078f
        float Q = std::numbers::pi_v<float> * frequency * t * k;
        computeCoefficients(mainIndex, index, frequency, Q);
    }

    void setDecay(const size_t mainIndex, const size_t index, const float t)
    {
        constexpr auto k = 0.1447648273f; // 1.f / std::log(1000.f); //  1/6.9078f
        const float Q = std::numbers::pi_v<float> * m_frequency[mainIndex] * t * k;
        const auto kqCl = K[mainIndex] / std::max(Q, 0.01f);
        const auto norm = 1.f / (1 + kqCl + kSquare[mainIndex]);
        m_cf[mainIndex][index].b0 = kqCl * norm;
        m_cf[mainIndex][index].a1 = 2 * (kSquare[mainIndex] - 1) * norm;
        m_cf[mainIndex][index].a2 = (1 - kqCl + kSquare[mainIndex]) * norm;
    }

    void computeCoefficients(const size_t mainIndex, const size_t index, const float frequency,
                             const float Q = 1.f / std::numbers::sqrt2_v<float>) noexcept
    {
        m_frequency[mainIndex] = frequency;
        Fc[mainIndex] = frequency / m_sampleRate;
        K[mainIndex] = std::tan(std::numbers::pi_v<float> * Fc[mainIndex]);
        kSquare[mainIndex] = K[mainIndex] * K[mainIndex];

        const auto kqCl = K[mainIndex] / std::max(Q, 0.01f);
        const auto norm = 1.f / (1 + kqCl + kSquare[mainIndex]);
        m_cf[mainIndex][index].b0 = kqCl * norm;
        m_cf[mainIndex][index].a1 = 2 * (kSquare[mainIndex] - 1) * norm;
        m_cf[mainIndex][index].a2 = (1 - kqCl + kSquare[mainIndex]) * norm;
    }

    float step(size_t mainIndex, const float in) noexcept
    {
        const auto b0s = in * m_cf[mainIndex][m_currentSet[mainIndex]].b0;
        const auto out = b0s + m_z[mainIndex][0];
        m_z[mainIndex][0] = m_z[mainIndex][1] - m_cf[mainIndex][m_currentSet[mainIndex]].a1 * out;
        m_z[mainIndex][1] = -b0s - m_cf[mainIndex][m_currentSet[mainIndex]].a2 * out;
        return out;
    }

    void process(const float* in, float* outBuffer) noexcept
    {
        for (size_t s = 0; s < BlockSize; ++s)
        {
            outBuffer[s] = 0;
            for (size_t i = 0; i < NumElements; ++i)
            {
                outBuffer[s] += step(i, in[s]);
            }
        }
    }

    void reset(size_t mainIndex, const float v1 = 0.f, const float v2 = 0.f) noexcept
    {
        m_z[mainIndex][0] = v1;
        m_z[mainIndex][1] = v2;
    }

    float magnitude(size_t mainIndex, const size_t subIndex, const float hz,
                    const float sampleRate = 48000.f) const noexcept
    {
        const auto b0 = static_cast<double>(m_cf[mainIndex][subIndex].b0);
        const auto a1 = static_cast<double>(m_cf[mainIndex][subIndex].a1);
        const auto a2 = static_cast<double>(m_cf[mainIndex][subIndex].a2);
        const auto phi = 4 * std::pow(std::sin(2 * std::numbers::pi_v<double> * hz / sampleRate / 2), 2);
        const auto db =
            10 * std::log10(std::pow((b0 + 0 + -b0), 2) + (b0 * -b0 * phi - (0 * (b0 + -b0) + 4 * b0 * -b0)) * phi) -
            10 * std::log10(std::pow((1.f + a1 + a2), 2) + (a2 * phi - (a1 * (1 + a2) + 4 * a2)) * phi);
        return static_cast<float>(db);
    }

    void damp(size_t mainIndex, const bool damp)
    {
        m_currentSet[mainIndex] = damp ? 1 : 0;
    }

    bool isActive(size_t mainIndex) noexcept
    {
        if (std::abs(m_z[mainIndex][0]) > 1E-5f || std::abs(m_z[mainIndex][1]) > 1E-5f)
        {
            m_inActiveCount[mainIndex] = 0;
        }
        else
        {
            m_inActiveCount[mainIndex]++;
        }
        return m_inActiveCount[mainIndex] < 32;
    }

  private:
    std::array<size_t, NumElements> m_currentSet{};
    std::array<int, NumElements> m_inActiveCount{};
    std::array<std::array<BandPassCoefficients, 2>, NumElements> m_cf{};
    std::array<std::array<float, 2>, NumElements> m_z{};
    std::array<float, NumElements> m_frequency{};
    std::array<float, NumElements> Fc{};
    std::array<float, NumElements> K{};
    std::array<float, NumElements> kSquare{};
};
}
