#pragma once


#include "Numbers/Convert.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

/*
 * Dattorro 48kHz:
 *     229  0.75
 *     173  0.75
 *     613  0.625
 *     449  0.625
 */
namespace AbacDsp
{
template <size_t MaxSize, size_t BlockSize>
class SchroederAllPass
{
  public:
    explicit SchroederAllPass()
        : m_delayLength(MaxSize / 2)
        , m_gain(0.65f)
        , m_buffer(MaxSize, 0.0f)
        , m_writeIndex(0)
    {
    }

    float step(const float input)
    {
        const float w_delayed = m_buffer[m_writeIndex];
        const float output = w_delayed - m_gain * input;
        m_buffer[m_writeIndex] = input + m_gain * output;
        m_writeIndex = (m_writeIndex + 1) % m_delayLength;
        return output;
    }

    void processBlock(const float* in, float* out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out[i] = step(in[i]);
        }
    }

    void setGain(const float gain)
    {
        m_gain = std::clamp(gain, -1.f, 1.f);
    }

    void setSize(const size_t samples)
    {
        if (samples != m_delayLength)
        {
            m_delayLength = samples;
            m_buffer.resize(samples, 0.0f);
            m_writeIndex = 0;
        }
    }

    void clear()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
        m_writeIndex = 0;
    }

  private:
    size_t m_delayLength;
    float m_gain;
    std::vector<float> m_buffer;
    size_t m_writeIndex;
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

    void clear()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void newFadeIfNeeded()
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

    void setGain(const float gain)
    {
        m_feedback = std::clamp(gain, -0.99999f, 0.99999f);
    }

    void setSize(const size_t newSize, const bool fast = false)
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

    void feedWrite(const float in)
    {
        m_buffer[m_headWrite] = in;
        if (++m_headWrite >= m_maxBufferSize)
        {
            m_headWrite = 0;
        }
    }

    float step(const float in)
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

    float nextHeadRead(const size_t index)
    {
        const float returnValue = m_buffer[m_headRead[index]];
        m_headRead[index] = (m_headRead[index] + 1) % m_maxBufferSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target)
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

    void processBlockInplace(float* inplace)
    {
        processBlock(inplace, inplace);
    }

    [[nodiscard]] size_t size() const
    {
        return m_currentDelayWidth;
    }

  private:
    template <bool fastSetting>
    void setSizeImpl(const size_t newSize)
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

    float m_sampleRate;
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