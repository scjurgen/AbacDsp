#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "Audio/FixedSizeProcessor.h"
#include "Numbers/Interpolation.h"

namespace AbacDsp
{
template <size_t MaxSizeInSamples>
class ModulatingDelayPitchedAdjust
{
  public:
    explicit ModulatingDelayPitchedAdjust(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_buffer(MaxSizeInSamples + 6)
    {
    }

    void setWidthInMsecs(const float milliseconds) noexcept
    {
        m_setByTime = true;
        const auto newSize = std::clamp<size_t>(getSamplesPerMillisecond(milliseconds, m_sampleRate, MaxSizeInSamples),
                                                48u, MaxSizeInSamples - 1);

        if (newSize == m_currentDistance)
        {
            return;
        }
        m_lastDistanceRequested = newSize;
    }

    void setSize(size_t newSize) noexcept
    {
        if (newSize >= MaxSizeInSamples)
        {
            newSize = MaxSizeInSamples - 1;
        }
        if (newSize == m_currentDistance)
        {
            return;
        }
        m_setByTime = false;
        m_lastDistanceRequested = newSize;
    }

    void setFeedback(const float gain) noexcept
    {
        m_feedback = std::clamp(gain, -0.99999f, 0.99999f);
    }

    void feedBackByTime(const float msecs, const float db = 0.001f, const bool negative = false) noexcept
    {
        m_decayMsecs = msecs;
        const auto feedback = std::pow(db, m_currentDistance / m_sampleRate / (msecs / 1000.0f));
        setFeedback(negative ? -feedback : feedback);
    }

    void setModDepth(const float depth) noexcept
    {
        m_modWidth = depth * 100.f;
    }

    void setModSpeed(const float speedHz) noexcept
    {
        m_modAdvanceTick = 2.f * speedHz / m_sampleRate;
    }

    void sweepTick() noexcept
    {
        m_currentPhase += m_modAdvanceTick;
        if (m_currentPhase >= 1.0f)
        {
            m_currentPhase -= 2.0f;
        }
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        sweepTick();
        const auto depth = m_modWidth * (std::abs(m_currentPhase) + 1) + 1; // triangular wave
        float dHead = m_headRead + depth;
        if (dHead >= MaxSizeInSamples)
        {
            dHead -= MaxSizeInSamples;
        }
        float intTailPosition{};
        const auto fraction = std::modf(dHead, &intTailPosition);
        const auto bufferValue = Interpolation::hermite43z(&m_buffer[static_cast<size_t>(intTailPosition)], fraction);
        const auto feedDelay = in + bufferValue * m_feedback;
        const auto ret = bufferValue;
        m_buffer[m_headWrite] = feedDelay;

        if (m_headWrite < 6)
        {
            const auto padIndex = m_headWrite + MaxSizeInSamples;
            m_buffer[padIndex] = m_buffer[m_headWrite];
        }

        m_headWrite++;
        if (m_headWrite >= MaxSizeInSamples)
        {
            m_headWrite = 0;
        }
        if (m_advanceSteps)
        {
            const auto headReadFloor = static_cast<ptrdiff_t>(m_headRead);
            const ptrdiff_t dt = (m_headWrite > m_headRead)
                                     ? static_cast<ptrdiff_t>(m_headWrite) - headReadFloor
                                     : static_cast<ptrdiff_t>(m_headWrite + MaxSizeInSamples) - headReadFloor;

            m_currentDistance = static_cast<size_t>(dt);
            if (static_cast<size_t>(dt) / 4 == m_newDistance / 4)
            {
                m_advanceSteps = false;
                m_advance = 1.0f;
                if (m_setByTime)
                {
                    feedBackByTime(m_decayMsecs);
                }
            }
        }
        else
        {
            if (m_lastDistanceRequested)
            {
                adjustBufferByPitching(m_lastDistanceRequested + 3);
                m_lastDistanceRequested = 0;
            }
        }
        m_headRead += m_advance;
        if (m_headRead >= MaxSizeInSamples)
        {
            m_headRead -= MaxSizeInSamples;
        }
        return ret;
    }

    [[nodiscard]] bool isAdvancing() const noexcept
    {
        return m_advanceSteps;
    }

    void blockFill(std::span<const float> in, std::span<float> out) noexcept
    {
        std::transform(in.begin(), in.end(), out.begin(), [this](const float s) noexcept { return step(s); });
    }

  private:
    void adjustBufferByPitching(const size_t newSize) noexcept
    {
        m_newDistance = newSize;
        if (newSize > m_currentDistance)
        {
            if (newSize == 0)
            {
                return;
            }
            m_advanceSteps = true;
            m_advance = 0.5f;
        }
        else
        {
            if (m_currentDistance == 0)
            {
                return;
            }
            m_advanceSteps = true;
            m_advance = 2.f;
        }
    }

    const float m_sampleRate{48000.0f};
    float m_feedback{0.0f};
    float m_headRead{0.0f};
    size_t m_headWrite{0};
    float m_decayMsecs{100.0f};
    size_t m_currentDistance{MaxSizeInSamples / 10};
    size_t m_newDistance{0};
    size_t m_lastDistanceRequested{0};
    bool m_advanceSteps{false};
    float m_advance{1.0f};
    float m_modWidth{0.1f};
    float m_modAdvanceTick{0.01f};
    float m_currentPhase{0.0f};
    bool m_setByTime{false};
    std::vector<float> m_buffer;
};
}
