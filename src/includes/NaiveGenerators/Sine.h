#pragma once

#include <cmath>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <vector>

class SineWave
{
public:
    explicit SineWave(const float sampleRate, const float frequency = 440.f)
        : m_sampleRate(sampleRate),
          m_frequency(frequency),
          m_phase(0.0f)
    {
        m_advance = m_frequency / m_sampleRate;
    }

    [[maybe_unused]] float step()
    {
        const float value = std::sinf(m_phase * 2.0f * std::numbers::pi_v<float>);
        m_phase += m_advance;
        if (m_phase > 1.0f)
        {
            m_phase -= 1.0f;
        }
        return value;
    }

    template <typename FloatIt>
    void render(FloatIt begin, FloatIt end, size_t numChannels = 1)
    {
        renderWithFrequency(begin, end, m_frequency, numChannels);
    }

    template <typename FloatIt>
    void renderWithFrequency(FloatIt begin, FloatIt end, float frequency, size_t numChannels = 1)
    {
        if (numChannels == 0)
        {
            throw std::invalid_argument("numChannels can not be 0");
        }

        if (const auto count = static_cast<size_t>(std::distance(begin, end)); count % numChannels != 0)
        {
            throw std::invalid_argument("Iterator range size must be a multiple of numChannels");
        }
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
    float m_sampleRate;
    float m_frequency;
    float m_phase;
    float m_advance;
};

