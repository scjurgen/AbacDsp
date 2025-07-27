#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace AbacadDsp
{

class RmsFollower
{
  public:
    using Precision = uint64_t;
    static constexpr Precision sizeFactor{1 << 24};
    static constexpr double reciprocalSizeFactor{1.0 / static_cast<double>(sizeFactor)};

    explicit RmsFollower(const size_t MaxWindowSize)
        : m_movingWindow(MaxWindowSize)
        , m_currentWidth{MaxWindowSize}
        , m_reciprocalWidth(1.f / static_cast<float>(MaxWindowSize))
    {
    }

    void setWindowSize(const size_t width)
    {
        constexpr size_t MinimumWindowSize{1};
        const auto newWidth = std::max(MinimumWindowSize, std::min(m_movingWindow.size(), width));
        const auto newReadHead = (m_writeHead + m_movingWindow.size() - newWidth) % m_movingWindow.size();
        if (newWidth < m_currentWidth)
        {
            while (newReadHead != m_readHead)
            {
                m_currentSum -= m_movingWindow[m_readHead++];
                m_readHead = m_readHead % m_movingWindow.size();
            }
        }
        else
        {
            while (newReadHead != m_readHead)
            {
                m_readHead = m_readHead == 0 ? m_movingWindow.size() - 1 : m_readHead - 1;
                m_currentSum += m_movingWindow[m_readHead];
            }
        }

        m_reciprocalWidth = 1.f / static_cast<float>(newWidth);
        m_currentWidth = newWidth;
    }

    void feed(const float* buffer, const size_t numSamples)
    {
        size_t indexBuffer = 0;
        while (indexBuffer < numSamples)
        {
            auto remainderWrite = m_movingWindow.size() - m_writeHead;
            auto remainderRead = m_movingWindow.size() - m_readHead;
            auto remainderMin = std::min(std::min(remainderWrite, remainderRead), m_currentWidth);
            auto samplesToProcess = numSamples - indexBuffer;
            if (samplesToProcess <= remainderMin)
            {
                addValues(buffer + indexBuffer, samplesToProcess);
                m_writeHead += samplesToProcess;
                m_readHead += samplesToProcess;
                indexBuffer += samplesToProcess;
            }
            else
            {
                addValues(buffer + indexBuffer, remainderMin);
                m_writeHead += remainderMin;
                m_readHead += remainderMin;
                indexBuffer += remainderMin;
            }
            if (m_writeHead == m_movingWindow.size())
            {
                m_writeHead = 0;
            }
            if (m_readHead == m_movingWindow.size())
            {
                m_readHead = 0;
            }
        }
    }

    [[nodiscard]] float getRms() const
    {
        return std::sqrt(
            static_cast<float>((static_cast<float>(m_currentSum) * reciprocalSizeFactor) * m_reciprocalWidth));
    }

    [[nodiscard]] float getMs() const
    {
        return static_cast<float>((static_cast<float>(m_currentSum) * reciprocalSizeFactor) * m_reciprocalWidth);
    }

  private:
    void addValues(const float* buffer, const size_t numSamples)
    {
        m_currentSum -= std::accumulate(m_movingWindow.data() + m_readHead,
                                        m_movingWindow.data() + m_readHead + numSamples, static_cast<Precision>(0));
        std::transform(buffer, buffer + numSamples, m_movingWindow.data() + m_writeHead,
                       [](const auto value)
                       { return static_cast<Precision>((value * value) * static_cast<float>(sizeFactor)); });
        m_currentSum += std::accumulate(m_movingWindow.data() + m_writeHead,
                                        m_movingWindow.data() + m_writeHead + numSamples, static_cast<Precision>(0));
    }

    std::vector<Precision> m_movingWindow;
    Precision m_currentSum{0}; // with 24bits of precision and a buffer of 1 second there would be (24+16) = 40 bits
    size_t m_readHead{0};
    size_t m_writeHead{0};
    size_t m_currentWidth{0};
    float m_reciprocalWidth;
};


template <size_t DBRange>
class PeakEnvelopeFollower
{
  public:
    explicit PeakEnvelopeFollower(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_range(std::pow(10.f, -static_cast<float>(DBRange) / 20.f))
    {
    }

    void setAttackInMsecs(const float attackInMilliseconds)
    {
        m_attackFactor = static_cast<float>(std::pow(m_range, 1.0f / (attackInMilliseconds * m_sampleRate / 1000.f)));
    }

    void setReleaseInMsecs(const float releaseInMilliseconds)
    {
        m_releaseFactor = static_cast<float>(std::pow(m_range, 1.0f / (releaseInMilliseconds * m_sampleRate / 1000.f)));
    }

    float step(const float value)
    {
        const auto valueIn = std::abs(value);
        m_envelope = valueIn > m_envelope ? m_attackFactor * (m_envelope - valueIn) + valueIn
                                          : m_releaseFactor * (m_envelope - valueIn) + valueIn;
        return m_envelope;
    }

    [[maybe_unused]] void blockAnalyze(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](const float in) { return step(in); });
    }

    [[nodiscard]] float getEnvelope() const
    {
        return m_envelope;
    }

  private:
    float m_sampleRate;
    const float m_range;
    float m_attackFactor{0.999f};
    float m_releaseFactor{0.999f};
    float m_envelope{0.f};
};
}