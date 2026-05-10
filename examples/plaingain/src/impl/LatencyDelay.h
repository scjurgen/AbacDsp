#pragma once

#include <array>
#include <cmath>
#include <vector>

class LatencyDelay
{
  public:
    explicit LatencyDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_xfadeLength(static_cast<size_t>(sampleRate * 0.1f))
        , m_delayBuffer(static_cast<size_t>(sampleRate * 0.1f), std::array<float, 2>{})
    {
    }

    void setLatency(const float value)
    {
        const size_t newLatency = std::min(static_cast<size_t>(value * m_sampleRate), m_delayBuffer.size() - 1);

        if (newLatency != m_latencyInSamples)
        {
            m_prevLatencyInSamples = m_latencyInSamples;
            m_latencyInSamples = newLatency;
            m_xfadeCounter = m_xfadeLength;
        }
    }

    std::array<float, 2> process(const std::array<float, 2> input)
    {
        m_delayBuffer[m_writePos] = input;

        const size_t readPos = (m_writePos + m_delayBuffer.size() - m_latencyInSamples) % m_delayBuffer.size();
        std::array<float, 2> sample = m_delayBuffer[readPos];

        if (m_xfadeCounter > 0)
        {
            const float t = static_cast<float>(m_xfadeCounter) / static_cast<float>(m_xfadeLength);
            const size_t prevReadPos =
                (m_writePos + m_delayBuffer.size() - m_prevLatencyInSamples) % m_delayBuffer.size();
            const auto& prevSample = m_delayBuffer[prevReadPos];
            sample[0] = sample[0] * (1.f - t) + prevSample[0] * t;
            sample[1] = sample[1] * (1.f - t) + prevSample[1] * t;
            --m_xfadeCounter;
        }

        m_writePos = (m_writePos + 1) % m_delayBuffer.size();
        return sample;
    }

  private:
    float m_sampleRate{};
    size_t m_latencyInSamples{};
    size_t m_prevLatencyInSamples{};

    size_t m_xfadeLength{};
    size_t m_xfadeCounter{};

    std::vector<std::array<float, 2>> m_delayBuffer;
    size_t m_writePos{};
};