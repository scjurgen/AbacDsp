#pragma once

#include <cmath>
#include <iterator>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>
#include "Filters/OnePoleFilter.h"

namespace AbacDsp
{
enum class Wave
{
    Sine,
    Saw,
    Triangle,
    Square,
    Noise
};

template <Wave Style>
class Generator
{
  public:
    explicit Generator(const float sampleRate, const float frequency = 440.f)
        : m_sampleRate(sampleRate)
        , m_frequency(frequency)
        , m_phase(0.0f)
        , m_lastNoise(0.0f)
        , m_lowpass(sampleRate)
    {
        m_advance = m_frequency / m_sampleRate;
        if constexpr (Style == Wave::Noise)
        {
            m_rng.seed(std::random_device{}());
        }
    }

    float step()
    {
        if constexpr (Style == Wave::Sine)
        {
            float v = std::sin(m_phase * 2.0f * std::numbers::pi_v<float>);
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Saw)
        {
            float v = 2.0f * (m_phase - 0.5f);
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Triangle)
        {
            float v = 2.0f * std::abs(2.0f * (m_phase - std::floor(m_phase + 0.5f))) - 1.0f;
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Square)
        {
            float v = m_phase < 0.5f ? 1.0f : -1.0f;
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Noise)
        {
            std::uniform_real_distribution dist(-1.0f, 1.0f);
            m_lowpass.setCutoff(m_frequency);
            float filtered = m_lowpass.step(dist(m_rng));
            advancePhase();
            return filtered;
        }
    }

    template <typename FloatIt>
    void render(FloatIt begin, FloatIt end, const size_t numChannels = 1)
    {
        renderWithFrequency(begin, end, m_frequency, numChannels);
    }

    template <typename FloatIt>
    void renderWithFrequency(FloatIt begin, FloatIt end, const float frequency, const size_t numChannels = 1)
    {
        if (numChannels == 0)
            throw std::invalid_argument("numChannels can not be 0");
        if (const auto count = static_cast<size_t>(std::distance(begin, end)); count % numChannels != 0)
            throw std::invalid_argument("Iterator range size must be a multiple of numChannels");
        m_frequency = frequency;
        m_advance = m_frequency / m_sampleRate;
        while (begin != end)
        {
            float value = step();
            for (size_t i = 0; i < numChannels && begin != end; ++i)
            {
                *begin++ = value;
            }
        }
    }

  private:
    void advancePhase()
    {
        m_phase += m_advance;
        if (m_phase > 1.0f)
        {
            m_phase -= 1.0f;
        }
    }

    float m_sampleRate, m_frequency, m_phase, m_advance, m_lastNoise;
    std::mt19937 m_rng;
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass, false> m_lowpass;
};
}
