#pragma once

#include "Audio/FixedSizeProcessor.h"
#include "FracReadHead.h"
#include "Numbers/Interpolation.h"
#include "Modulation/Wow.h"

#include <algorithm>
#include <vector>
#include <cmath>

namespace AbacDsp
{

/*
 * | --------↑------------↓---------|
 * | ------ rd ---------- wr -------|
 * | ----------- nrd ---- wr -------|
 * | --------------↓------↑---------|
 *
 * reduce distance:
 *     m_advanceDirection > 0.f:
 *         delta = targetRead - currentRead
 *         delta < 0 -> target reached
 *         delta < 100 -> slow down
 */

template <size_t MaxSizeInSamples>
class OuModDelay
{
  public:
    static constexpr size_t WowStep{8};
    explicit OuModDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_buffer(MaxSizeInSamples + 6)
        , m_wow(sampleRate / WowStep)
        , m_rdHd(sampleRate)
    {
        m_wow.setDepth(0.1f);
        m_wow.setRate(0.25f);
        m_wow.setDrift(0.1f);
        m_wow.setVariance(0.1f);
    }

    void setWidthInMsecs(const float milliseconds)
    {
        auto newSize = std::clamp<size_t>(getSamplesPerMillisecond(milliseconds, m_sampleRate, MaxSizeInSamples), 48u,
                                          MaxSizeInSamples - 1);
        m_rdHd.setNewDelta(newSize, 2.f, 4000);
    }

    void setSize(const size_t newSize)
    {
        const auto mSize = std::max(newSize, MaxSizeInSamples - 1);
        m_rdHd.setNewDelta(mSize, 2.f, 4000);
    }

    void setFeedback(const float gain)
    {
        m_feedback = std::clamp(gain, -0.99999f, 0.99999f);
    }

    void feedBackByTime(const float msecs, const float db = 0.001f, const bool negative = false)
    {
        m_decayMsecs = msecs;
        const auto feedback = std::pow(db, m_rdHd.getCurrentPosition() / m_sampleRate / (msecs / 1000.0f));
        setFeedback(negative ? -feedback : feedback);
    }

    void setModDepth(const float depth) noexcept
    {
        m_wow.setDepth(depth);
    }

    void setModSpeed(const float speedHz) noexcept
    {
        m_wow.setRate(speedHz);
    }

    void setModVariance(const float variance) noexcept
    {
        m_wow.setVariance(variance);
    }

    void setModDrift(const float drift) noexcept
    {
        m_wow.setDrift(drift);
    }

    int m_wowCount{WowStep - 1};
    float m_lastWow{0.f};

    float step(const float in) noexcept
    {
        if (--m_wowCount <= 0)
        {
            m_lastWow = (m_wow.step() - 1) * 100.f;
            m_wowCount = WowStep;
        }


        float dHead = m_rdHd.step(m_headWrite) + m_lastWow;
        if (dHead >= MaxSizeInSamples)
        {
            dHead -= MaxSizeInSamples;
        }
        float intTailPosition;
        const auto fraction = modff(dHead, &intTailPosition);
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
        return ret;
    }

    template <size_t blockSize>
    void blockFill(const float* in, float* out) noexcept
    {
        for (uint32_t pos = 0; pos < blockSize; pos++)
        {
            out[pos] = step(in[pos]);
        }
    }

  private:
    float m_sampleRate{48000.0f};
    float m_feedback{0};
    size_t m_headWrite{0};
    float m_decayMsecs{100};
    // adapt soft buffersize *speed up/down*

    FracReadHead<MaxSizeInSamples> m_rdHd;
    // modulation

    std::vector<float> m_buffer;
    Wow m_wow;
};
}
