#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

#include "Analysis/EnvelopeFollower.h"

namespace AbacDsp
{

/**
 * @ingroup analysis
 * @brief Single-channel level meter: falling peak, sliding RMS, peak hold and clip latch.
 *
 * Meter ballistics fall linearly in dB (a fixed number of dB per second, as in IEC PPMs),
 * not exponentially in amplitude, so the fall rate reads the same at every level. The
 * peak rises instantly. The hold marker keeps the highest peak for the hold time, then
 * follows the falling peak; a hold time of infinity keeps it until reset(). All levels are
 * in dBFS and floored at kFloorDb. Allocates only in the constructor.
 */
class LevelMeter
{
  public:
    static constexpr float kFloorDb{-120.f};
    static constexpr float kClipLevel{1.f};

    explicit LevelMeter(const float sampleRate, const float maxRmsWindowSeconds = 1.f)
        : m_sampleRate(sampleRate)
        , m_rms(std::max<size_t>(1, static_cast<size_t>(maxRmsWindowSeconds * sampleRate)))
    {
        setRmsWindowSeconds(0.3f);
    }

    void setPeakFallDbPerSecond(const float dbPerSecond) noexcept
    {
        m_fallDbPerSecond = std::max(0.f, dbPerSecond);
    }

    // Clamped to the maximum given at construction.
    void setRmsWindowSeconds(const float seconds) noexcept
    {
        m_rms.setWindowSize(static_cast<size_t>(std::max(0.f, seconds) * m_sampleRate));
    }

    // 0 disables the hold; infinity holds until reset().
    void setPeakHoldSeconds(const float seconds) noexcept
    {
        m_holdSeconds = std::max(0.f, seconds);
        m_holdRemaining = std::min(m_holdRemaining, m_holdSeconds);
    }

    void reset() noexcept
    {
        m_holdDb = m_peakDb;
        m_holdRemaining = 0.f;
        m_clipped = false;
    }

    void process(const std::span<const float> block) noexcept
    {
        m_rms.feed(block);
        float blockPeak{0.f};
        for (const auto sample : block)
        {
            blockPeak = std::max(blockPeak, std::abs(sample));
        }
        m_clipped = m_clipped || blockPeak >= kClipLevel;

        const float elapsedSeconds = static_cast<float>(block.size()) / m_sampleRate;
        m_peakDb = std::max({toDb(blockPeak), m_peakDb - m_fallDbPerSecond * elapsedSeconds, kFloorDb});
        updateHold(elapsedSeconds);
    }

    [[nodiscard]] float peakDb() const noexcept
    {
        return m_peakDb;
    }

    [[nodiscard]] float rmsDb() const noexcept
    {
        return toDb(m_rms.getRms());
    }

    [[nodiscard]] float holdDb() const noexcept
    {
        return m_holdDb;
    }

    [[nodiscard]] bool isClipped() const noexcept
    {
        return m_clipped;
    }

  private:
    [[nodiscard]] static float toDb(const float gain) noexcept
    {
        return gain > 0.f ? std::max(kFloorDb, 20.f * std::log10(gain)) : kFloorDb;
    }

    void updateHold(const float elapsedSeconds) noexcept
    {
        if (m_peakDb >= m_holdDb)
        {
            m_holdDb = m_peakDb;
            m_holdRemaining = m_holdSeconds;
            return;
        }
        m_holdRemaining -= elapsedSeconds;
        if (m_holdRemaining <= 0.f)
        {
            m_holdDb = m_peakDb;
        }
    }

    float m_sampleRate;
    RmsFollower m_rms;
    float m_fallDbPerSecond{11.8f};
    float m_holdSeconds{0.f};
    float m_holdRemaining{0.f};
    float m_peakDb{kFloorDb};
    float m_holdDb{kFloorDb};
    bool m_clipped{false};
};

}
