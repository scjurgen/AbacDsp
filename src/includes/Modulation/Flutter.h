#pragma once

#include "Numbers/Approximation.h"


#include <array>
#include <numbers>
#include <cmath>
#include "Parameters/SmoothingParameter.h"

namespace AbacDsp
{

class FlutterLfo
{
  public:
    // float qcos(const float r)
    // {
    //     return std::cos(r * 3.1415926535f * 2.f);
    // }

    explicit FlutterLfo(const float sampleRate, const float frequencyMultiplier, const float amplitude,
                        const float phaseOffset)
        : m_frequencyMultiplier(std::abs(frequencyMultiplier / sampleRate))
        , m_amplitude(amplitude)
        , m_phase(phaseOffset)
    {
        while (m_phase < -1)
        {
            m_phase += 2.0f;
        }
        while (m_phase > 1)
        {
            m_phase -= 2.0f;
        }
        m_initialPhase = m_phase;
    }

    void reset() noexcept
    {
        m_phase = m_initialPhase;
    }

    float step(const float baseFrequency) noexcept
    {
        const auto angleDelta = 2.0f * std::abs(baseFrequency) * m_frequencyMultiplier;
        m_phase += angleDelta;

        while (m_phase >= 1)
        {
            m_phase -= 2.0f;
        }
        return m_amplitude * Approximation::remezFullCosP6<Approximation::DomainMinusOneToOne>(m_phase);
    }

  private:
    float m_frequencyMultiplier;
    float m_amplitude;
    float m_phase{0};
    float m_initialPhase{0};
};

class Flutter
{
  public:
    explicit Flutter(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_lfos({FlutterLfo(sampleRate, 1.0f, 1.0f, 0.0f), FlutterLfo(sampleRate, 2.0f, 0.3f, 13.0f / 4.0f),
                  FlutterLfo(sampleRate, 3.0f, 0.2f, -1.f / 10.0f)})
    {
        m_rateSmoothed.newTransition(0.3f, defaultSmoothingTime, m_sampleRate, true);
        m_depthSmoothed.newTransition(0.0f, defaultSmoothingTime, m_sampleRate, true);
    }

    void setRate(const float v) noexcept
    {
        m_rateSmoothed.newTransition(v, defaultSmoothingTime, m_sampleRate);
    }

    void setDepth(const float v) noexcept
    {
        m_depthSmoothed.newTransition(v * 0.01f, defaultSmoothingTime, m_sampleRate);
    }

    void reset() noexcept
    {
        for (auto& lfo : m_lfos)
        {
            lfo.reset();
        }
    }

    float step() noexcept
    {
        if (!m_rateSmoothed.hasStoppedSmoothing())
        {
            m_flutterFreq = m_rateSmoothed.getValue();
        }

        if (!m_depthSmoothed.hasStoppedSmoothing())
        {
            m_depth = m_depthSmoothed.getValue();
        }

        float flutterValue = 0.0f;
        for (auto& lfo : m_lfos)
        {
            flutterValue += lfo.step(m_flutterFreq);
        }
        // Convert to speed ratio: 1.0 = normal speed
        // Scale depth to produce realistic flutter variations (typically ±0.01% to ±1%)
        return 1.0f + m_depth * flutterValue;
    }

  private:
    static float fcos(const float x) noexcept
    {
        const auto xsqr = x * x;
        constexpr float c1 = 0.6079271019f; // 6/π²
        constexpr float c2 = 0.1290061377f; // 4/π³
        return 1 - c1 * xsqr + c2 * std::abs(xsqr * x);
    }
    static constexpr float defaultSmoothingTime{0.1f};

    const float m_sampleRate;
    LinearSmoothing m_rateSmoothed;
    LinearSmoothing m_depthSmoothed;
    float m_depth{0.f};
    float m_flutterFreq{0.f};
    std::array<FlutterLfo, 3> m_lfos;
};

}
