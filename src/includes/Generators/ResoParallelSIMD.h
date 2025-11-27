#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <simd/simd.h>
#include <vector>

/*
 purely for excitation and some decay
 *
 */
namespace AbacDsp
{
/*
 * Using size greater equal 40 NumElements, there are huge differences (on ARM and Intel)
* ARM factor per samples:
  Par Simd  4                   5.82x
  Par Simd  8                   2.89x
  Par Simd  12                  1.93x
  Par Simd  16                  3.40x
  Par Simd  20                  3.43x
  Par Simd  24                  3.43x
  Par Simd  32                  3.54x
  ---
  Par Simd  40                  1.00x
  Par Simd  48                  1.03x
  Par Simd  100                 1.00x
  Par Simd  5000                1.06x
  Par Simd  10000               1.08x

Intel actually good at 32 :
  Par Simd  4                   2.21x
  Par Simd  8                   1.12x
  Par Simd  12                  1.00x
  Par Simd  16                  2.55x
  Par Simd  20                  2.55x
  Par Simd  24                  2.53x
---
  Par Simd  32                  1.10x
  Par Simd  40                  1.10x
  Par Simd  48                  1.12x
  Par Simd  100                 1.11x
  Par Simd  5000                1.13x
  Par Simd  10000               1.10x

 */

template <size_t NumElements, size_t BlockSize>
    requires(NumElements % 4 == 0 && NumElements > 0)
class ResoBpParallelSIMD
{
    struct BandPassCoefficients
    {
        float b0{};
        float a1{};
        float a2{};
    };

  public:
    static constexpr size_t NumSimdGroups = NumElements / 4;

    explicit ResoBpParallelSIMD(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    ResoBpParallelSIMD()
        : ResoBpParallelSIMD(48000.f)
    {
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    void setByDecay(const size_t mainIndex, const size_t index, const float frequency, const float t)
    {
        constexpr auto k = 0.1447648273f;
        float Q = std::numbers::pi_v<float> * frequency * t * k;
        computeCoefficients(mainIndex, index, frequency, Q);
    }

    void setDecay(const size_t mainIndex, const size_t index, const float t)
    {
        constexpr auto k = 0.1447648273f;
        const float Q = std::numbers::pi_v<float> * m_frequency[mainIndex] * t * k;
        const auto kqCl = m_K[mainIndex] / std::max(Q, 0.01f);
        const auto norm = 1.f / (1 + kqCl + m_kSquare[mainIndex]);

        m_cf[mainIndex][index].b0 = kqCl * norm;
        m_cf[mainIndex][index].a1 = 2 * (m_kSquare[mainIndex] - 1) * norm;
        m_cf[mainIndex][index].a2 = (1 - kqCl + m_kSquare[mainIndex]) * norm;

        updateSoACoefficients(mainIndex, index);
    }

    void computeCoefficients(const size_t mainIndex, const size_t index, const float frequency,
                             const float Q = 1.f / std::numbers::sqrt2_v<float>) noexcept
    {
        m_frequency[mainIndex] = frequency;
        m_Fc[mainIndex] = frequency / m_sampleRate;
        m_K[mainIndex] = std::tan(std::numbers::pi_v<float> * m_Fc[mainIndex]);
        m_kSquare[mainIndex] = m_K[mainIndex] * m_K[mainIndex];

        const auto kqCl = m_K[mainIndex] / std::max(Q, 0.01f);
        const auto norm = 1.f / (1 + kqCl + m_kSquare[mainIndex]);

        m_cf[mainIndex][index].b0 = kqCl * norm;
        m_cf[mainIndex][index].a1 = 2 * (m_kSquare[mainIndex] - 1) * norm;
        m_cf[mainIndex][index].a2 = (1 - kqCl + m_kSquare[mainIndex]) * norm;

        updateSoACoefficients(mainIndex, index);
    }

    void process(float* outBuffer) noexcept
    {
        for (size_t s = 0; s < BlockSize; ++s)
        {
            outBuffer[s] = 0.f;
        }

        for (size_t g = 0; g < NumSimdGroups; ++g)
        {
            const simd_float4 b0 = m_currentSet ? m_b0_set1[g] : m_b0_set0[g];
            const simd_float4 a1 = m_currentSet ? m_a1_set1[g] : m_a1_set0[g];
            const simd_float4 a2 = m_currentSet ? m_a2_set1[g] : m_a2_set0[g];

            for (size_t s = 0; s < BlockSize; ++s)
            {
                const simd_float4 out = b0 + m_z0[g];
                m_z0[g] = m_z1[g] - a1 * out;
                m_z1[g] = -b0 - a2 * out;
                outBuffer[s] += simd_reduce_add(out);
            }
        }
    }

    void process(std::array<float, BlockSize>& out) noexcept
    {
        process(out.data());
    }

    void reset(const size_t mainIndex, const float v1 = 0.f, const float v2 = 0.f) noexcept
    {
        m_z[mainIndex][0] = v1;
        m_z[mainIndex][1] = v2;

        const size_t groupIndex = mainIndex / 4;
        const size_t laneIndex = mainIndex % 4;
        m_z0[groupIndex][laneIndex] = v1;
        m_z1[groupIndex][laneIndex] = v2;
    }

    [[nodiscard]] float magnitude(size_t mainIndex, const size_t subIndex, const float hz,
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

    void damp(const bool damp)
    {
        m_currentSet = damp ? 1 : 0;
    }

    bool isActive(const size_t mainIndex) noexcept
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
    void updateSoACoefficients(const size_t mainIndex, const size_t index)
    {
        const size_t groupIndex = mainIndex / 4;
        const size_t laneIndex = mainIndex % 4;

        if (index == 0)
        {
            m_b0_set0[groupIndex][laneIndex] = m_cf[mainIndex][0].b0;
            m_a1_set0[groupIndex][laneIndex] = m_cf[mainIndex][0].a1;
            m_a2_set0[groupIndex][laneIndex] = m_cf[mainIndex][0].a2;
        }
        else
        {
            m_b0_set1[groupIndex][laneIndex] = m_cf[mainIndex][1].b0;
            m_a1_set1[groupIndex][laneIndex] = m_cf[mainIndex][1].a1;
            m_a2_set1[groupIndex][laneIndex] = m_cf[mainIndex][1].a2;
        }
    }

    float m_sampleRate{48000.f};
    std::array<size_t, NumElements> m_inActiveCount{};
    std::array<std::array<BandPassCoefficients, 2>, NumElements> m_cf{};
    std::array<std::array<float, 2>, NumElements> m_z{};
    std::array<float, NumElements> m_frequency{};
    std::array<float, NumElements> m_Fc{};
    std::array<float, NumElements> m_K{};
    std::array<float, NumElements> m_kSquare{};

    size_t m_currentSet{};

    alignas(16) std::array<simd_float4, NumSimdGroups> m_b0_set0{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_b0_set1{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_a1_set0{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_a1_set1{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_a2_set0{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_a2_set1{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_z0{};
    alignas(16) std::array<simd_float4, NumSimdGroups> m_z1{};
};

}