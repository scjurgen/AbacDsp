#pragma once

#include <cmath>
#include <iterator>
#include <numbers>
#include <random>
#include <stdexcept>

#include "Filters/OnePoleFilter.h"

/*
 * Don't use these generators for production code
 * intended use is in unit-tests or documentation
 */

namespace AbacDsp
{
/// @ingroup naivegenerators
/// @brief Waveform shape, selected at compile time.
enum class Wave
{
    Sine,
    Saw,
    Triangle,
    Square,
    Noise
};

/**
 * @ingroup naivegenerators
 * @brief Waveform generators computed directly from the phase, aliasing included.
 *
 * Naive means the discontinuous shapes are evaluated straight from the phasor
 * with no band limiting, so a saw or square aliases audibly above a few hundred
 * hertz. That is the point: these are the baseline the band-limited generators
 * and the wavetable oscillator are compared against, and the reference for what
 * the ideal waveform is before any anti-aliasing is applied.
 *
 * Sine and Noise do not alias and are usable as ordinary sources.
 * @see https://en.wikipedia.org/wiki/Aliasing
 */
template <Wave Style>
class Generator
{
  public:
    explicit Generator(const float sampleRate, const float frequency = 440.f)
        : m_sampleRate(sampleRate)
        , m_frequency(frequency)
        , m_advance(frequency / sampleRate)
        , m_lowpass(sampleRate)
    {
        if constexpr (Style == Wave::Noise)
        {
            m_rng.seed(std::random_device{}());
        }
    }

    [[nodiscard]] float step()
    {
        if constexpr (Style == Wave::Sine)
        {
            const auto v = std::sin(m_phase * 2.0f * std::numbers::pi_v<float>);
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Saw)
        {
            const auto v = 2.0f * (m_phase - 0.5f);
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Triangle)
        {
            const auto v = 2.0f * std::abs(2.0f * (m_phase - std::floor(m_phase + 0.5f))) - 1.0f;
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Square)
        {
            const auto v = m_phase < 0.5f ? 1.0f : -1.0f;
            advancePhase();
            return v;
        }
        else if constexpr (Style == Wave::Noise)
        {
            std::uniform_real_distribution dist(-1.0f, 1.0f);
            m_lowpass.setCutoff(m_frequency);
            const auto filtered = m_lowpass.step(dist(m_rng));
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
            const auto value = step();
            for (size_t i = 0; i < numChannels && begin != end; ++i)
            {
                *begin++ = value;
            }
        }
    }

  private:
    void advancePhase() noexcept
    {
        m_phase += m_advance;
        if (m_phase > 1.0f)
        {
            m_phase -= 1.0f;
        }
    }

    const float m_sampleRate;
    float m_frequency;
    float m_phase{0.0f};
    float m_advance;
    std::mt19937 m_rng;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowpass;
};
}
