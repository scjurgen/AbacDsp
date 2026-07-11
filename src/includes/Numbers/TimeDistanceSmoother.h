#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

namespace AbacDsp
{

template <std::floating_point FloatType = double>
class TimeDistanceSmoother
{
  public:
    explicit TimeDistanceSmoother(const FloatType sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void forceWritePosition(const FloatType writePosition) noexcept
    {
        m_idealWritePosition = writePosition;
    }

    void forceReadPositionDistance(const FloatType distance) noexcept
    {
        m_targetDistance = distance;
        m_readPosition = m_idealWritePosition - distance;
        wrapReadPosition();
        m_isTransitioning = false;
        m_correctionCooldown = 0;
    }

    void forceReadAdvance(const FloatType advance) noexcept
    {
        m_writeAdvance = advance;
    }

    void setWrapPosition(const size_t wrapPosition) noexcept
    {
        m_wrapPosition = static_cast<FloatType>(wrapPosition);
    }

    void setDistanceCorrectionThreshold(const FloatType threshold) noexcept
    {
        m_correctionThreshold = threshold;
    }

    void setCorrectionTime(const FloatType correctionTimeSeconds) noexcept
    {
        m_correctionTime = std::max(static_cast<FloatType>(0.0001), correctionTimeSeconds);
    }

    void newTargetDistance(const FloatType distance, const FloatType transitionTimeSeconds = FloatType(0.02)) noexcept
    {
        if (std::abs(distance - m_targetDistance) < FloatType(0.5))
        {
            return;
        }

        m_targetDistance = distance;

        FloatType currentDistance = m_idealWritePosition - m_readPosition;
        currentDistance = normalizeDistance(currentDistance);

        const FloatType distanceDelta = m_targetDistance - currentDistance;
        const FloatType transitionSamples = transitionTimeSeconds * m_sampleRate;

        m_distanceChangePerSample = distanceDelta / transitionSamples;
        m_remainingTransitionSamples = static_cast<int>(transitionSamples);
        m_isTransitioning = (m_remainingTransitionSamples > 0);

        m_correctionCooldown = 0;
    }

    void setCurrentWritePosition(const FloatType position, const FloatType advance) noexcept
    {
        m_idealWritePosition = position;
        wrapPosition(m_idealWritePosition);
        m_writeAdvance = advance;
    }

    [[nodiscard]] FloatType getPosition() const noexcept
    {
        return m_readPosition;
    }

    void advancePosition() noexcept
    {
        if (m_correctionCooldown > 0)
        {
            m_correctionCooldown--;
        }

        FloatType readAdvance = m_writeAdvance;

        if (m_isTransitioning)
        {
            readAdvance -= m_distanceChangePerSample;
            m_remainingTransitionSamples--;
            if (m_remainingTransitionSamples <= 0)
            {
                m_isTransitioning = false;
            }
        }
        else if (m_correctionThreshold > FloatType(0) && m_correctionCooldown <= 0)
        {
            checkAndCorrectDistance();
        }

        m_readPosition += readAdvance;
        wrapReadPosition();
        m_idealWritePosition += m_writeAdvance;
        wrapPosition(m_idealWritePosition);
    }

    [[nodiscard]] FloatType getCurrentDelta() const noexcept
    {
        FloatType currentDistance = m_idealWritePosition - m_readPosition;
        currentDistance = normalizeDistance(currentDistance);
        return currentDistance;
    }

  private:
    void checkAndCorrectDistance() noexcept
    {
        FloatType currentDistance = m_idealWritePosition - m_readPosition;
        currentDistance = normalizeDistance(currentDistance);

        const FloatType distanceError = std::abs(currentDistance - m_targetDistance);

        if (distanceError > m_correctionThreshold)
        {
            const FloatType correctionSamples = m_correctionTime * m_sampleRate;
            const FloatType distanceDelta = m_targetDistance - currentDistance;

            m_distanceChangePerSample = distanceDelta / correctionSamples;
            m_remainingTransitionSamples = static_cast<int>(correctionSamples);
            m_isTransitioning = true;
            m_correctionCooldown = static_cast<int>(correctionSamples * FloatType(1.5));
        }
    }

    FloatType normalizeDistance(FloatType distance) const noexcept
    {
        while (distance >= m_wrapPosition)
        {
            distance -= m_wrapPosition;
        }
        while (distance < FloatType(0))
        {
            distance += m_wrapPosition;
        }
        return distance;
    }

    void wrapReadPosition() noexcept
    {
        wrapPosition(m_readPosition);
    }

    void wrapPosition(FloatType& position) const noexcept
    {
        while (position >= m_wrapPosition)
        {
            position -= m_wrapPosition;
        }
        while (position < FloatType(0))
        {
            position += m_wrapPosition;
        }
    }

    // keep all FloatType, otherwise we get drift problems
    const FloatType m_sampleRate{};
    FloatType m_wrapPosition{FloatType(100)};

    FloatType m_idealWritePosition{FloatType(0)};
    FloatType m_writeAdvance{FloatType(1)};

    FloatType m_readPosition{FloatType(0)};
    FloatType m_targetDistance{FloatType(0)};

    bool m_isTransitioning{false};
    FloatType m_distanceChangePerSample{FloatType(0)};
    int m_remainingTransitionSamples{0};

    FloatType m_correctionThreshold{FloatType(1)};
    FloatType m_correctionTime{FloatType(0.05)};
    int m_correctionCooldown{0};
};

}
