#pragma once

#include <algorithm>
#include <array>
#include <vector>
#include <cmath>
#include <numeric>
#include <optional>

namespace AbacDsp
{

class YinPitchDetector
{
  public:
    // Constexpr tolerances for better maintainability
    static constexpr float kDefaultThreshold = 0.1f;
    static constexpr float kSilenceThreshold = 1e-6f;
    static constexpr float kMinCumulativeSumThreshold = 1e-10f;
    static constexpr float kMinimumQualityThreshold = 0.8f;
    static constexpr float kMinPeriodsForBuffer = 4.0f; // Number of periods needed for reliable detection

    explicit YinPitchDetector(const float sampleRate, const float minFreq = 80.0f, const float maxFreq = 1000.0f,
                              const float hopFrequency = 50.0f)
        : m_sampleRate(sampleRate)
        , m_minFreq(minFreq)
        , m_maxFreq(maxFreq)
        , m_hopFrequency(hopFrequency)
        , m_bufferSize(calculateBufferSize(sampleRate, minFreq))
        , m_hopSize(calculateHopSize(sampleRate, hopFrequency))
        , m_minPeriod(static_cast<size_t>(sampleRate / maxFreq))
        , m_maxPeriod(static_cast<size_t>(sampleRate / minFreq))
        , m_buffer(m_bufferSize, 0.0f)
        , m_differenceFunction(m_bufferSize / 2, 0.0f)
        , m_cmndf(m_bufferSize / 2, 0.0f)
        , m_writeIndex(0)
        , m_currentPitch(0.0f)
        , m_hopCounter(0)
    {
    }

    bool hasNewPitch() const
    {
        return m_newPitch;
    }

    float step(const float in)
    {
        // Add new sample to circular buffer
        m_buffer[m_writeIndex] = in;
        m_writeIndex = (m_writeIndex + 1) % m_bufferSize;

        // Process based on calculated hop size
        m_newPitch = false;
        if (++m_hopCounter >= m_hopSize)
        {
            m_hopCounter = 0;
            m_currentPitch = computePitch();
            m_newPitch = true;
        }

        return m_currentPitch;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](const float in) { return step(in); });
    }

    float getCurrentPitch() const
    {
        return m_currentPitch;
    }

    // Getters for debugging/testing
    size_t getBufferSize() const
    {
        return m_bufferSize;
    }
    size_t getHopSize() const
    {
        return m_hopSize;
    }
    float getActualHopRate() const
    {
        return m_sampleRate / static_cast<float>(m_hopSize);
    }

  private:
    // Configuration
    float m_sampleRate;
    float m_minFreq;
    float m_maxFreq;
    float m_hopFrequency;

    // Calculated sizes
    size_t m_bufferSize;
    size_t m_hopSize;
    size_t m_minPeriod;
    size_t m_maxPeriod;

    // Processing state
    std::vector<float> m_buffer;
    std::vector<float> m_differenceFunction;
    std::vector<float> m_cmndf;
    size_t m_writeIndex;
    float m_currentPitch;
    size_t m_hopCounter;
    bool m_newPitch{false};

    static size_t calculateBufferSize(const float sampleRate, const float minFreq)
    {
        // Calculate buffer size based on minimum frequency
        // We need at least kMinPeriodsForBuffer periods of the lowest frequency
        const float minPeriodSamples = sampleRate / minFreq;
        const size_t requiredSize = static_cast<size_t>(kMinPeriodsForBuffer * minPeriodSamples);

        return requiredSize;
    }

    static size_t calculateHopSize(const float sampleRate, const float hopFrequency)
    {
        return static_cast<size_t>(sampleRate / hopFrequency);
    }

    float computePitch()
    {
        if (!hasEnoughEnergy())
        {
            return 0.0f;
        }
        computeDifferenceFunction();
        computeCMNDF();
        const auto tauOpt = findAbsoluteThreshold();
        if (!tauOpt.has_value())
        {
            return 0.0f;
        }
        const float refinedTau = parabolicInterpolation(tauOpt.value());
        return m_sampleRate / refinedTau;
    }

    bool hasEnoughEnergy()
    {
        float sumSquares = 0.0f;
        const size_t analysisLength = m_bufferSize / 2;

        for (size_t i = 0; i < analysisLength; ++i)
        {
            const size_t idx = (m_writeIndex - analysisLength + i + m_bufferSize) % m_bufferSize;
            const float sample = m_buffer[idx];
            sumSquares += sample * sample;
        }

        const float rmsEnergy = std::sqrt(sumSquares / analysisLength);
        return rmsEnergy > kSilenceThreshold;
    }

    void computeDifferenceFunction()
    {
        const size_t maxTau = std::min(m_differenceFunction.size(), m_maxPeriod);

        for (size_t tau = 0; tau < maxTau; ++tau)
        {
            float sum = 0.0f;

            for (size_t j = 0; j < m_bufferSize / 2; ++j)
            {
                const size_t idx1 = (m_writeIndex + j) % m_bufferSize;
                const size_t idx2 = (m_writeIndex + j + tau) % m_bufferSize;

                const float diff = m_buffer[idx1] - m_buffer[idx2];
                sum += diff * diff;
            }

            m_differenceFunction[tau] = sum;
        }

        // Set tau=0 to 1.0 by convention
        m_differenceFunction[0] = 1.0f;
    }

    void computeCMNDF()
    {
        m_cmndf[0] = 1.0f;
        float cumulativeSum = m_differenceFunction[0];

        for (size_t tau = 1; tau < m_cmndf.size(); ++tau)
        {
            cumulativeSum += m_differenceFunction[tau];

            if (cumulativeSum > kMinCumulativeSumThreshold)
            {
                m_cmndf[tau] = m_differenceFunction[tau] * static_cast<float>(tau) / cumulativeSum;
            }
            else
            {
                m_cmndf[tau] = 1.0f; // Default to high value for near-zero cumulative sum
            }
        }
    }

    std::optional<size_t> findAbsoluteThreshold()
    {
        const size_t startTau = std::max(m_minPeriod, size_t(1));
        const size_t endTau = std::min(m_cmndf.size(), m_maxPeriod);

        // First pass: look for values below threshold that are local minima
        for (size_t tau = startTau; tau < endTau; ++tau)
        {
            if (m_cmndf[tau] < kDefaultThreshold)
            {
                // Check if it's a local minimum
                if (tau > 0 && tau < m_cmndf.size() - 1)
                {
                    if (m_cmndf[tau] <= m_cmndf[tau - 1] && m_cmndf[tau] <= m_cmndf[tau + 1])
                    {
                        return tau;
                    }
                }
            }
        }

        // Second pass: find global minimum, but only if it's significantly low
        const auto minIt = std::min_element(m_cmndf.begin() + startTau, m_cmndf.begin() + endTau);
        if (minIt != m_cmndf.begin() + endTau)
        {
            const float minValue = *minIt;
            // Only accept the global minimum if it's reasonably low
            if (minValue < kMinimumQualityThreshold)
            {
                return std::distance(m_cmndf.begin(), minIt);
            }
        }

        return std::nullopt;
    }

    float parabolicInterpolation(const size_t tau) const
    {
        if (tau == 0 || tau >= m_cmndf.size() - 1)
        {
            return static_cast<float>(tau);
        }

        const float y1 = m_cmndf[tau - 1];
        const float y2 = m_cmndf[tau];
        const float y3 = m_cmndf[tau + 1];

        // Parabolic interpolation
        const float a = (y1 - 2.0f * y2 + y3) / 2.0f;
        const float b = (y3 - y1) / 2.0f;

        if (std::abs(a) < 1e-6f)
        {
            return static_cast<float>(tau);
        }

        const float xVertex = -b / (2.0f * a);
        return static_cast<float>(tau) + xVertex;
    }
};

}
