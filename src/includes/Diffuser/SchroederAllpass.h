#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Numbers/Convert.h"

namespace AbacDsp
{
/**
 * @ingroup diffuser
 * @brief Schroeder allpass section: a delay line wrapped in a feedforward and feedback gain pair.
 *
 * Flat magnitude response with a dense, decaying impulse response, which is
 * what makes it the standard building block for smearing an impulse into
 * diffusion without colouring the spectrum. Gain sets echo density against
 * ringing; beyond about 0.7 the section starts to sound tonal.
 *
 * Delay lengths are chosen mutually prime so that the sections in a chain do
 * not reinforce each other at a common period. Dattorro's set at 48 kHz is
 * 229 and 173 at gain 0.75, then 613 and 449 at 0.625.
 * @see https://ccrma.stanford.edu/~jos/pasp/Schroeder_Allpass_Sections.html
 */
template <size_t MaxSize, size_t BlockSize>
class SchroederAllPass
{
  public:
    explicit SchroederAllPass()
        : m_buffer(MaxSize, 0.0f)
    {
    }

    [[nodiscard]] float step(const float input) noexcept
    {
        const float w_delayed = m_buffer[m_writeIndex];
        const float output = w_delayed - m_gain * input;
        m_buffer[m_writeIndex] = input + m_gain * output;
        m_writeIndex = (m_writeIndex + 1) % m_delayLength;
        return output;
    }

    void processBlock(const float* in, float* out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out[i] = step(in[i]);
        }
    }

    void setGain(const float gain) noexcept
    {
        m_gain = std::clamp(gain, -1.f, 1.f);
    }

    void setSize(const size_t samples) noexcept
    {
        if (samples != m_delayLength)
        {
            m_delayLength = samples;
            m_buffer.resize(samples, 0.0f);
            m_writeIndex = 0;
        }
    }

    void clear() noexcept
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
        m_writeIndex = 0;
    }

  private:
    size_t m_delayLength{MaxSize / 2};
    float m_gain{0.65f};
    std::vector<float> m_buffer;
    size_t m_writeIndex{0};
};

template <size_t MaxSize48Khz, size_t BlockSize>
class SchroederAllPassSoftTransition
{
  public:
    static constexpr size_t minDelaySize{1u};
    explicit SchroederAllPassSoftTransition(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_maxBufferSize(static_cast<size_t>(std::round(static_cast<float>(MaxSize48Khz) * sampleRate / 48000.f)))
        , m_buffer(m_maxBufferSize, 0.f)
    {
    }

    void clear() noexcept
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void newFadeIfNeeded() noexcept
    {
        if (m_newFadeSize && !m_fadeSteps)
        {
            m_fadeFactorIn = 0.0f;
            m_fadeFactorOut = 1.0f;
            m_fadeSteps = std::clamp<size_t>(m_currentDelayWidth * 2, 256, 8192);
            m_fadeAdvance = 1.0f / static_cast<float>(m_fadeSteps);
            m_headRead[1] = (m_headWrite + m_maxBufferSize - m_newFadeSize) % m_maxBufferSize;
            m_currentDelayWidth = m_newFadeSize;
        }
    }

    [[nodiscard]] float getDecayTimeInSamples(const float db = -60.f) const
    {
        const auto f = Convert::dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setGain(const float gain) noexcept
    {
        m_feedback = std::clamp(gain, -0.99999f, 0.99999f);
    }

    void setSize(const size_t newSize, const bool fast = false) noexcept
    {
        if (fast)
        {
            setSizeImpl<true>(newSize);
        }
        else
        {
            setSizeImpl<false>(newSize);
        }
    }

    void feedWrite(const float in) noexcept
    {
        m_buffer[m_headWrite] = in;
        if (++m_headWrite >= m_maxBufferSize)
        {
            m_headWrite = 0;
        }
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        newFadeIfNeeded();
        auto getNext = [this]()
        {
            if (m_fadeSteps)
            {
                const auto outValue = nextHeadRead(0) * m_fadeFactorOut;
                const auto inValue = nextHeadRead(1) * m_fadeFactorIn;
                m_fadeFactorOut -= m_fadeAdvance;
                m_fadeFactorIn += m_fadeAdvance;
                if (--m_fadeSteps == 0)
                {
                    m_headRead[0] = m_headRead[1];
                    m_newFadeSize = m_newFadeSizeScheduled;
                    m_newFadeSizeScheduled = 0;
                }
                return outValue + inValue;
            }
            return nextHeadRead(0);
        };

        const float delayed = getNext();
        const auto output = delayed - m_feedback * in;
        const auto toWrite = in + m_feedback * output;
        feedWrite(toWrite);
        return output;
    }

    float nextHeadRead(const size_t index) noexcept
    {
        const float returnValue = m_buffer[m_headRead[index]];
        m_headRead[index] = (m_headRead[index] + 1) % m_maxBufferSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target) noexcept
    {
        newFadeIfNeeded();
        if (m_fadeSteps)
        {
            std::transform(source, source + BlockSize, target, [this](const float in) { return step(in); });
        }
        else
        {
            std::transform(source, source + BlockSize, target,
                           [this](const float in)
                           {
                               const float delayed = nextHeadRead(0);
                               const auto output = delayed - m_feedback * in;
                               const auto toWrite = in + m_feedback * output;
                               feedWrite(toWrite);
                               return output;
                           });
        }
    }

    void processBlockInplace(float* inplace) noexcept
    {
        processBlock(inplace, inplace);
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return m_currentDelayWidth;
    }

  private:
    template <bool fastSetting>
    void setSizeImpl(const size_t newSize) noexcept
    {
        const auto clampedSize = std::clamp<size_t>(newSize, minDelaySize, m_maxBufferSize);
        if (clampedSize == m_currentDelayWidth)
        {
            return;
        }
        if constexpr (fastSetting)
        {
            m_currentDelayWidth = clampedSize;
            m_newFadeSizeScheduled = 0;
            m_newFadeSize = 0;
            m_headWrite = m_currentDelayWidth;
            m_headRead[0] = 0;
            return;
        }
        if (m_newFadeSize)
        {
            m_newFadeSizeScheduled = clampedSize;
        }
        else
        {
            m_newFadeSize = clampedSize;
        }
    }

    const float m_sampleRate;
    float m_feedback{0.0f};
    size_t m_headWrite{0};
    std::array<size_t, 2> m_headRead{0, 0};
    size_t m_fadeSteps{0};
    float m_fadeFactorIn{0.0f};
    float m_fadeFactorOut{0.0f};
    float m_fadeAdvance{0.0f};
    size_t m_newFadeSize{0};
    size_t m_newFadeSizeScheduled{0};
    size_t m_currentDelayWidth{MaxSize48Khz / 8};
    size_t m_maxBufferSize{MaxSize48Khz};
    std::vector<float> m_buffer{};
};

}