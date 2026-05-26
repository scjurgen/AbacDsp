#pragma once

#include <array>
#include <cmath>
#include <numbers>

#include "Numbers/Approximation.h"
#include "Parameters/SmoothingParameter.h"

namespace AbacDsp
{

class FlutterLfo
{
  public:
    explicit FlutterLfo(const float sampleRate, const float frequencyMultiplier, const float amplitude = 1.f,
                        const float phaseOffset = 0.f)
        : m_frequencyMultiplier(frequencyMultiplier / sampleRate)
        , m_amplitude(amplitude)
        , m_phase(phaseOffset)
    {
    }

    void reset() noexcept
    {
        m_phase = 0.0f;
    }

    [[nodiscard]] static float fastCos(const float x) noexcept
    {
        // 1 - 6 * x^2 / (pi^2) + 4 * x^3 / (|pi^3|)
        constexpr float c1 = 0.60792710185403f; // 6/π²
        constexpr float c2 = 0.12900613773279f; // 4/π³
        const auto xSquare = x * x;
        const auto xCube = xSquare * x;
        return 1.f - c1 * xSquare + c2 * std::abs(xCube);
    }

    [[nodiscard]] float step(const float baseFrequency) noexcept
    {
        const auto phaseInc = std::numbers::pi_v<float> * 2.0f * baseFrequency * m_frequencyMultiplier;

        m_phase += phaseInc;

        constexpr float pi = std::numbers::pi_v<float>;
        while (m_phase > pi)
        {
            m_phase -= 2.0f * pi;
        }
        while (m_phase < -pi)
        {
            m_phase += 2.0f * pi;
        }
        return m_amplitude * Approximation::remezFullCosP6<Approximation::DomainMinusPiToPi>(m_phase);
    }

  private:
    float m_frequencyMultiplier;
    float m_amplitude;
    float m_phase;
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

    [[nodiscard]] float step() noexcept
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
        return 1.0f + m_depth * flutterValue;
    }

  private:
    static constexpr float defaultSmoothingTime{0.1f};

    const float m_sampleRate;
    LinearSmoothing m_rateSmoothed;
    LinearSmoothing m_depthSmoothed;
    float m_depth{0.f};
    float m_flutterFreq{0.f};
    std::array<FlutterLfo, 3> m_lfos;
};

}
