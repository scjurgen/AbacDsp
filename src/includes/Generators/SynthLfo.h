#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <random>

#include "Generators/BrownModulation.h"

namespace AbacDsp
{

enum class LfoType
{
    Sine,
    Triangle,
    Saw,
    Square,
    Noise,
    SampleHoldNoise,
    SampleHoldFlipFlop,
    BrownNoise
};

/**
 * @ingroup generators
 * @brief Naive (non-bandlimited) LFO waveform generator.
 *
 * Bandlimiting is unnecessary here: an LFO runs at sub-audio rates and its
 * output is always smoothed or filtered downstream before it can alias.
 */
class LfoGenerators
{
  public:
    explicit LfoGenerators(const float sampleRate)
        : m_invSampleRate(1.0f / sampleRate)
        , m_brown(sampleRate)
    {
    }

    void setWaveForm(const LfoType type) noexcept
    {
        m_type = type;
    }

    void setFrequency(const float frequency) noexcept
    {
        if (frequency > 0.0f)
        {
            m_advance = frequency * m_invSampleRate;
            m_brown.setCenterFrequency(frequency);
            m_playing = true;
            m_lastValue = 0.0f;
        }
        else
        {
            m_playing = false;
        }
    }

    [[nodiscard]] float step() noexcept
    {
        if (!m_playing)
        {
            return m_lastValue;
        }

        m_phase += m_advance;
        if (m_phase >= 1.0f)
        {
            m_phase -= 1.0f;
        }

        switch (m_type)
        {
            case LfoType::Sine:
                m_lastValue = std::sin(m_phase * 2.0f * std::numbers::pi_v<float>);
                break;
            case LfoType::Triangle:
                m_lastValue = m_phase < 0.5f ? 4.0f * m_phase - 1.0f : 3.0f - 4.0f * m_phase;
                break;
            case LfoType::Saw:
                m_lastValue = 2.0f * m_phase - 1.0f;
                break;
            case LfoType::Square:
                m_lastValue = m_phase < 0.5f ? 1.0f : -1.0f;
                break;
            case LfoType::Noise:
                randomProgress(4);
                m_lastValue = m_randDist(m_randEngine);
                break;
            case LfoType::SampleHoldNoise:
                randomProgress(2);
                break;
            case LfoType::SampleHoldFlipFlop:
                randomProgress(2);
                break;
            case LfoType::BrownNoise:
                m_lastValue = m_brown.step();
                break;
        }
        return m_lastValue;
    }

    void play() noexcept
    {
        m_playing = true;
    }

    void pause() noexcept
    {
        m_playing = false;
    }

    void stop() noexcept
    {
        m_playing = false;
        m_phase = 0.0f;
        m_lastValue = 0.0f;
    }

  private:
    void randomProgress(const size_t factor) noexcept
    {
        assert(factor != 0);
        m_noisePhase += m_advance * static_cast<float>(factor);
        while (m_noisePhase >= 1.0f)
        {
            m_noisePhase -= 1.0f;
            m_lastValue = m_randDist(m_randEngine);
            m_sampleHoldFlipper = (m_sampleHoldFlipper + 1) % factor;
        }
    }

    LfoType m_type{LfoType::Sine};
    const float m_invSampleRate;
    float m_advance{0.009167f};
    float m_phase{0.0f};
    float m_lastValue{0.0f};
    float m_noisePhase{0.0f};
    size_t m_sampleHoldFlipper{0};
    bool m_playing{true};

    std::mt19937 m_randEngine{std::random_device{}()};
    std::uniform_real_distribution<float> m_randDist{-1.0f, 1.0f};
    BrownModulation m_brown;
};

/**
 * @ingroup generators
 * @brief LfoGenerators with a speed-dependent one-pole smoother over its output.
 *
 * Smoothing time tracks speed on a log scale (5x period at low rates, 15x at
 * high) so a slow LFO stays free of stair-stepping while a fast one is not
 * smeared into silence.
 */
class SynthLfo : public LfoGenerators
{
  public:
    explicit SynthLfo(const float sampleRate)
        : LfoGenerators(sampleRate)
        , m_sampleRate(sampleRate)
    {
    }

    void setSpeed(const float hz) noexcept
    {
        setFrequency(hz);
        constexpr auto varRatioLow = 5.0f;
        constexpr auto varRatioHigh = 15.0f;
        const auto mcf = std::clamp(hz, 0.01f, m_sampleRate / 4.0f);
        const auto ratio = varRatioLow + (varRatioHigh - varRatioLow) * std::log10(mcf * 10.0f) / std::log10(1000.0f);
        m_lpFdbk = std::exp(-2.0f * std::numbers::pi_v<float> * mcf * ratio / m_sampleRate);
    }

    [[nodiscard]] float getValue() noexcept
    {
        const auto s = step();
        m_v = s + m_lpFdbk * (m_v - s);
        return m_v;
    }

  private:
    const float m_sampleRate;
    float m_lpFdbk{0.9f};
    float m_v{0.0f};
};

}
