#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace AbacDsp
{

/*
 * Four cascaded one-pole stages with resonance feedback, atan input saturation
 * and smoothed cutoff transitions. Derived classes mix the stage outputs
 * (weights f0..f4) into low-, high-, band-, all-pass and notch responses,
 * see the aliases at the end of the file.
 */
class FourStageFilter
{
  public:
    explicit FourStageFilter(const float sampleRate)
        : FourStageFilter(sampleRate, 1000.f)
    {
    }

    FourStageFilter(const float sampleRate, const float defaultCutoff)
        : m_sampleRate(sampleRate)
    {
        setCutoff(defaultCutoff);
        m_stepsAdvance = 0;
        m_pole = m_newPole;
    }

    virtual ~FourStageFilter() = default;
    FourStageFilter(const FourStageFilter&) = default;
    FourStageFilter& operator=(const FourStageFilter&) = default;
    FourStageFilter(FourStageFilter&&) noexcept = default;
    FourStageFilter& operator=(FourStageFilter&&) noexcept = default;

    void reset() noexcept
    {
        std::ranges::fill(m_v, 0.f);
    }

    void setSmoothingSteps(const size_t steps) noexcept
    {
        m_stepsAdvanceSetting = steps;
    }

    void setResonance(const float value) noexcept
    {
        m_reso = value * 4.f;
        m_gain = std::clamp(1 + m_adaptGain, 1.f, 10.f);
    }

    void setAdaptGain(const float value) noexcept
    {
        m_adaptGain = value;
    }

    float setCutoff(const float cutoff)
    {
        if (cutoff == m_lastCutoffIn)
        {
            return 0.f;
        }
        m_lastCutoffIn = cutoff;
        const auto cF = std::clamp(warpCutoffForSampleRate(cutoff), 10.f, 22000.f);
        m_cutoff = cF;
        m_newPole = std::exp(-2.f * std::numbers::pi_v<float> * cF / m_sampleRate);
        if (m_stepsAdvanceSetting == 0)
        {
            m_pole = m_newPole;
        }
        else
        {
            m_advance = (m_newPole - m_pole) / static_cast<float>(m_stepsAdvanceSetting);
        }
        m_stepsAdvance = m_stepsAdvanceSetting;
        return m_cutoff;
    }

    [[nodiscard]] float currentFactor() const noexcept
    {
        return m_pole;
    }

    virtual float step(float in) = 0;

    void processBlockInplace(float* source, const size_t numSamples)
    {
        processBlock(source, source, numSamples);
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        size_t index = 0;
        size_t toIndex = numSamples;

        // split into if-less blocks
        if (m_stepsAdvance)
        {
            if (m_stepsAdvance < numSamples)
            {
                toIndex = m_stepsAdvance;
                m_stepsAdvance = 0;
            }
            else
            {
                m_stepsAdvance -= numSamples;
            }
            while (index < toIndex)
            {
                m_pole += m_advance;
                target[index] = step(source[index]);
                ++index;
            }
            if (!m_stepsAdvance)
            {
                m_pole = m_newPole;
            }
        }
        while (index < numSamples)
        {
            target[index] = step(source[index]);
            ++index;
        }
    }

    [[nodiscard]] float correctGain() const noexcept
    {
        return m_gain;
    }

  private:
    // per-rate cubic corrections so a requested cutoff lands on the measured response
    [[nodiscard]] float warpCutoffForSampleRate(const float cutoff) const noexcept
    {
        const float x = cutoff;
        if (m_sampleRate == 44100.f)
        {
            return 0.1070741493f + 1.000163615f * x + -6.77430211e-05f * x * x + 3.441634626e-09f * x * x * x;
        }
        if (m_sampleRate == 48000.f)
        {
            return 0.1409743683f + 0.9999793344f * x + -6.203395634e-05f * x * x + 2.855230937e-09f * x * x * x;
        }
        if (m_sampleRate == 96000.f)
        {
            return 0.2635405566f + 1.000099839f * x + -3.105817148e-05f * x * x + 7.1736266e-10f * x * x * x;
        }
        if (m_sampleRate == 192000.f)
        {
            return -0.01065210969f + 1.001839706f * x + -1.61077305e-05f * x * x + 2.229968241e-10f * x * x * x;
        }
        if (m_sampleRate == 384000.f)
        {
            return -0.1060540674f + 1.002323759f * x + -8.204298197e-06f * x * x + 6.640562956e-11f * x * x * x;
        }
        return x;
    }

    float m_sampleRate;
    float m_lastCutoffIn{1.f};
    float m_advance{0.f};
    size_t m_stepsAdvance{0};
    size_t m_stepsAdvanceSetting{0};
    float m_newPole{0.5f};
    float m_cutoff{1000.f};
    float m_gain{1.f};
    float m_adaptGain{0.f};

  protected:
    float m_reso{0.f};
    float m_pole{0.5f};
    std::array<float, 4> m_v{};
};

// optimized for fixed coefficients, small integer weights fold into adds
template <int f0, int f1, int f2, int f3, int f4>
class FixedFourStageFilter final : public FourStageFilter
{
  public:
    explicit FixedFourStageFilter(const float sampleRate)
        : FourStageFilter(sampleRate, 1000.f)
    {
    }

    float step(const float in) override
    {
        const auto feed = std::atan(in - m_v[3] * m_reso);
        m_v[0] = feed + m_pole * (m_v[0] - feed);
        m_v[1] = m_v[0] + m_pole * (m_v[1] - m_v[0]);
        m_v[2] = m_v[1] + m_pole * (m_v[2] - m_v[1]);
        m_v[3] = m_v[2] + m_pole * (m_v[3] - m_v[2]);
        return f0 * feed + f1 * m_v[0] + f2 * m_v[1] + f3 * m_v[2] + f4 * m_v[3];
    }
};

// with resonance the atan stage acts as a gentle saturator, hence bypass is not pointless
using ByPassSmooth = FixedFourStageFilter<1, 0, 0, 0, 0>;
using Lp6Smooth = FixedFourStageFilter<0, 1, 0, 0, 0>;
using Lp12Smooth = FixedFourStageFilter<0, 0, 1, 0, 0>;
using Lp18Smooth = FixedFourStageFilter<0, 0, 0, 1, 0>;
using Lp24Smooth = FixedFourStageFilter<0, 0, 0, 0, 1>;

using Ap6Smooth = FixedFourStageFilter<1, -2, 0, 0, 0>;
using Ap12Smooth = FixedFourStageFilter<1, -4, 4, 0, 0>;
using Ap18Smooth = FixedFourStageFilter<1, -6, 12, -8, 0>;
using Ap24Smooth = FixedFourStageFilter<1, -8, 24, -32, 16>;

using Bp12Smooth = FixedFourStageFilter<0, -2, 2, 0, 0>;
using Bp24Smooth = FixedFourStageFilter<0, 0, 4, -8, 4>;

using Hp6Smooth = FixedFourStageFilter<1, -1, 0, 0, 0>;
using Hp12Smooth = FixedFourStageFilter<1, -2, 1, 0, 0>;
using Hp18Smooth = FixedFourStageFilter<1, -3, 3, -1, 0>;
using Hp24Smooth = FixedFourStageFilter<1, -4, 6, -4, 1>;

using Phaser12Smooth = FixedFourStageFilter<1, -2, 2, 0, 0>;
using Phaser24Smooth = FixedFourStageFilter<1, -4, 12, -16, 8>;

using DoubleNotch = FixedFourStageFilter<1, -4, 11, -14, 7>;
using Notch12Smooth = FixedFourStageFilter<1, -2, 2, 0, 0>;
using Hp12Lp6Smooth = FixedFourStageFilter<0, -3, 6, -3, 0>;
using Hp18Lp6Smooth = FixedFourStageFilter<0, -3, 9, -9, 3>;
using Notch12Lp6Smooth = FixedFourStageFilter<0, -1, 2, -2, 0>;
using Allpass18Lp6Smooth = FixedFourStageFilter<0, -1, 3, -6, 4>;

}
