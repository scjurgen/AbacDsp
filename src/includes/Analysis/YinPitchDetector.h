#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace AbacDsp
{

class YinPitchDetector
{
  public:
    static constexpr float kDefaultThreshold = 0.1f;
    static constexpr float kSilenceThreshold = 1e-6f;
    static constexpr float kMinCumulativeSumThreshold = 1e-10f;
    static constexpr float kMinimumQualityThreshold = 0.8f;
    static constexpr float kMinPeriodsForBuffer = 4.0f;

    explicit YinPitchDetector(const float sampleRate, const float minFreq = 80.0f, const float maxFreq = 1000.0f,
                              const float hopFrequency = 50.0f)
        : m_sampleRate(sampleRate)
        , m_bufferSize(calculateBufferSize(sampleRate, minFreq))
        , m_hopSize(calculateHopSize(sampleRate, hopFrequency))
        , m_minPeriod(static_cast<size_t>(sampleRate / maxFreq))
        , m_maxPeriod(static_cast<size_t>(sampleRate / minFreq))
        , m_buffer(m_bufferSize, 0.0f)
        , m_differenceFunction(m_bufferSize / 2, 0.0f)
        , m_cmndf(m_bufferSize / 2, 0.0f)
    {
    }

    [[nodiscard]] bool hasNewPitch() const noexcept
    {
        return m_newPitch;
    }

    [[nodiscard]] float step(const float in)
    {
        m_buffer[m_writeIndex] = in;
        m_writeIndex = (m_writeIndex + 1) % m_bufferSize;

        m_newPitch = false;
        if (++m_hopCounter >= m_hopSize)
        {
            m_hopCounter = 0;
            m_currentPitch = computePitch();
            m_newPitch = true;
        }

        return m_currentPitch;
    }

    void processBlock(std::span<const float> source, std::span<float> target)
    {
        std::transform(source.begin(), source.end(), target.begin(), [this](const float in) { return step(in); });
    }

    [[nodiscard]] float getCurrentPitch() const noexcept
    {
        return m_currentPitch;
    }

    [[nodiscard]] size_t getBufferSize() const noexcept
    {
        return m_bufferSize;
    }

    [[nodiscard]] size_t getHopSize() const noexcept
    {
        return m_hopSize;
    }

    [[nodiscard]] float getActualHopRate() const noexcept
    {
        return m_sampleRate / static_cast<float>(m_hopSize);
    }

  private:
    const float m_sampleRate;

    const size_t m_bufferSize;
    const size_t m_hopSize;
    const size_t m_minPeriod;
    const size_t m_maxPeriod;

    std::vector<float> m_buffer;
    std::vector<float> m_differenceFunction;
    std::vector<float> m_cmndf;
    size_t m_writeIndex{0};
    float m_currentPitch{0.0f};
    size_t m_hopCounter{0};
    bool m_newPitch{false};

    [[nodiscard]] static size_t calculateBufferSize(const float sampleRate, const float minFreq) noexcept
    {
        return static_cast<size_t>(kMinPeriodsForBuffer * (sampleRate / minFreq));
    }

    [[nodiscard]] static size_t calculateHopSize(const float sampleRate, const float hopFrequency) noexcept
    {
        return static_cast<size_t>(sampleRate / hopFrequency);
    }

    [[nodiscard]] float computePitch()
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

    [[nodiscard]] bool hasEnoughEnergy() const
    {
        const size_t analysisLength = m_bufferSize / 2;
        float sumSquares = 0.0f;
        for (size_t i = 0; i < analysisLength; ++i)
        {
            const size_t idx = (m_writeIndex - analysisLength + i + m_bufferSize) % m_bufferSize;
            const float sample = m_buffer[idx];
            sumSquares += sample * sample;
        }
        return std::sqrt(sumSquares / static_cast<float>(analysisLength)) > kSilenceThreshold;
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

        // tau=0 is defined as 1 by the YIN algorithm convention
        m_differenceFunction[0] = 1.0f;
    }

    void computeCMNDF()
    {
        m_cmndf[0] = 1.0f;
        float cumulativeSum = m_differenceFunction[0];

        for (size_t tau = 1; tau < m_cmndf.size(); ++tau)
        {
            cumulativeSum += m_differenceFunction[tau];

            m_cmndf[tau] = (cumulativeSum > kMinCumulativeSumThreshold)
                               ? m_differenceFunction[tau] * static_cast<float>(tau) / cumulativeSum
                               : 1.0f;
        }
    }

    [[nodiscard]] std::optional<size_t> findAbsoluteThreshold() const
    {
        const size_t startTau = std::max(m_minPeriod, size_t{1});
        const size_t endTau = std::min(m_cmndf.size(), m_maxPeriod);

        for (size_t tau = startTau; tau < endTau; ++tau)
        {
            if (m_cmndf[tau] < kDefaultThreshold && tau > 0 && tau < m_cmndf.size() - 1 &&
                m_cmndf[tau] <= m_cmndf[tau - 1] && m_cmndf[tau] <= m_cmndf[tau + 1])
            {
                return tau;
            }
        }

        const auto minIt = std::min_element(m_cmndf.begin() + static_cast<ptrdiff_t>(startTau),
                                            m_cmndf.begin() + static_cast<ptrdiff_t>(endTau));
        if (minIt != m_cmndf.begin() + static_cast<ptrdiff_t>(endTau) && *minIt < kMinimumQualityThreshold)
        {
            return static_cast<size_t>(std::distance(m_cmndf.begin(), minIt));
        }

        return std::nullopt;
    }

    [[nodiscard]] float parabolicInterpolation(const size_t tau) const noexcept
    {
        if (tau == 0 || tau >= m_cmndf.size() - 1)
        {
            return static_cast<float>(tau);
        }

        const float y1 = m_cmndf[tau - 1];
        const float y2 = m_cmndf[tau];
        const float y3 = m_cmndf[tau + 1];

        const float a = (y1 - 2.0f * y2 + y3) / 2.0f;
        const float b = (y3 - y1) / 2.0f;

        if (std::abs(a) < 1e-6f)
        {
            return static_cast<float>(tau);
        }

        return static_cast<float>(tau) + (-b / (2.0f * a));
    }
};

}  // namespace AbacDsp
