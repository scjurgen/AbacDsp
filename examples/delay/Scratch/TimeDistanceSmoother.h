#pragma once

#include <cmath>
#include <cstddef>
/*
 * @brief Smooth read head position with stable distance control and auto-correction.
 *
 * Goals:
 * 1. Smooth read advancement (no jitter artifacts)
 * 2. Stable distance maintenance once target reached
 * 3. Instant adaptation to write advance changes
 * 4. Auto-correction when distance drifts beyond threshold
 *
 * N.B.: keep class double to avoid fast drift
 */
class TimeDistanceSmoother
{
  public:
    explicit TimeDistanceSmoother(const float sampleRate)
        : m_sampleRate(static_cast<double>(sampleRate))
    {
    }

    void forceWritePosition(const double writePosition) noexcept
    {
        m_idealWritePosition = writePosition;
    }

    void forceReadPositionDistance(const double distance) noexcept
    {
        m_targetDistance = distance;
        m_readPosition = m_idealWritePosition - distance;
        wrapReadPosition();
        m_isTransitioning = false;
        m_correctionCooldown = 0;
    }

    void forceReadAdvance(const double advance) noexcept
    {
        m_writeAdvance = advance;
    }

    void setWrapPosition(const size_t wrapPosition) noexcept
    {
        m_wrapPosition = static_cast<double>(wrapPosition);
    }

    void setDistanceCorrectionThreshold(const double threshold) noexcept
    {
        m_correctionThreshold = threshold;
    }

    void setCorrectionTime(const double correctionTimeSeconds) noexcept
    {
        m_correctionTime = std::max(0.0001, correctionTimeSeconds);
    }

    void newTargetDistance(const double distance, const double transitionTimeSeconds = 0.02f) noexcept
    {
        if (std::abs(distance - m_targetDistance) < 0.5f)
        {
            return;
        }
        m_targetDistance = distance;

        // current actual distance
        double currentDistance = m_idealWritePosition - m_readPosition;
        currentDistance = normalizeDistance(currentDistance);

        const double distanceDelta = m_targetDistance - currentDistance;
        const double transitionSamples = transitionTimeSeconds * m_sampleRate;

        // distance change per sample needed
        m_distanceChangePerSample = distanceDelta / transitionSamples;
        m_remainingTransitionSamples = static_cast<int>(transitionSamples);
        m_isTransitioning = (m_remainingTransitionSamples > 0);

        m_correctionCooldown = 0;
    }

    void setCurrentWritePosition(const double position, const double advance) noexcept
    {
        m_idealWritePosition = position;
        wrapPosition(m_idealWritePosition);
        m_writeAdvance = advance;
    }

    [[nodiscard]] double getPosition() const noexcept
    {
        return m_readPosition;
    }

    void advancePosition() noexcept
    {
        if (m_correctionCooldown > 0)
        {
            m_correctionCooldown--;
        }

        double readAdvance = m_writeAdvance;

        if (m_isTransitioning)
        {
            // Adjust read advance to achieve distance change
            readAdvance -= m_distanceChangePerSample;
            m_remainingTransitionSamples--;
            if (m_remainingTransitionSamples <= 0)
            {
                m_isTransitioning = false;
            }
        }
        else if (m_correctionThreshold > 0.0f && m_correctionCooldown <= 0)
        {
            checkAndCorrectDistance();
        }

        m_readPosition += readAdvance;
        wrapReadPosition();
        m_idealWritePosition += m_writeAdvance;
        wrapPosition(m_idealWritePosition);
    }

    [[nodiscard]] float getCurrentDelta() const noexcept
    {
        double currentDistance = m_idealWritePosition - m_readPosition;
        currentDistance = normalizeDistance(currentDistance);
        return static_cast<float>(currentDistance);
    }

  private:
    void checkAndCorrectDistance() noexcept
    {
        double currentDistance = m_idealWritePosition - m_readPosition;
        currentDistance = normalizeDistance(currentDistance);

        const double distanceError = std::abs(currentDistance - m_targetDistance);

        if (distanceError > m_correctionThreshold)
        {
            const double correctionSamples = m_correctionTime * m_sampleRate;
            const double distanceDelta = m_targetDistance - currentDistance;

            // Start mini-transition for correction
            m_distanceChangePerSample = distanceDelta / correctionSamples;
            m_remainingTransitionSamples = static_cast<int>(correctionSamples);
            m_isTransitioning = true;
            // Set cooldown to prevent oscillation (correction time + 50%)
            m_correctionCooldown = static_cast<int>(correctionSamples * 1.5f);
        }
    }

    double normalizeDistance(double distance) const noexcept
    {
        while (distance >= m_wrapPosition)
        {
            distance -= m_wrapPosition;
        }
        while (distance < 0)
        {
            distance += m_wrapPosition;
        }
        return distance;
    }

    void wrapReadPosition()
    {
        wrapPosition(m_readPosition);
    }

    void wrapPosition(double& position) const noexcept
    {
        while (position >= m_wrapPosition)
        {
            position -= m_wrapPosition;
        }
        while (position < 0)
        {
            position += m_wrapPosition;
        }
    }

    double m_sampleRate{};
    double m_wrapPosition{100.0f};

    // Write tracking (jitter-free ideal position)
    double m_idealWritePosition{0.0f};
    double m_writeAdvance{1.0f};

    double m_readPosition{0.0f};

    double m_targetDistance{0.0f};

    bool m_isTransitioning{false};
    double m_distanceChangePerSample{0.0f};
    int m_remainingTransitionSamples{0};

    double m_correctionThreshold{1.0f};
    double m_correctionTime{0.05f};
    int m_correctionCooldown{0};
};