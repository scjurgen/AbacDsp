#pragma once

#include <vector>
#include <array>
#include <algorithm>

#include "Filters/OnePoleFilter.h"

namespace AbacDsp
{

template <size_t MAXSIZE>
class DispersionDelay
{
  public:
    using AllPass = OnePoleFilter<OnePoleFilterCharacteristic::AllPass>;

    explicit DispersionDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_allPass(sampleRate)
        , m_scheduledDelayWidth(MAXSIZE / 8)
        , m_hasScheduledChange(false)
        , m_currentReadHead(0)
        , m_fadeWindowSize(1024.f)
        , m_currentFadeIn(0.f)
        , m_currentFadeOut(1.f)
        , m_fadeStep(0)
        , m_activeFade(false)
        , m_targetRead(MAXSIZE - MAXSIZE / 8 - 1)
        , m_buffer(MAXSIZE, 0.f)
    {
        m_allPass.setCutoff(sampleRate / 2 - 1);
        m_read[0] = MAXSIZE - m_currentDelayWidth - 1;
        while (m_read[0] >= m_buffer.size())
        {
            m_read[0] -= m_buffer.size();
        }
        m_read[1] = m_read[0];
    }

    void setFadeWindowSize(const size_t size)
    {
        m_fadeWindowSize = size;
    }

    void setAllPassCutoff(const float f)
    {
        m_allPass.setCutoff(f);
    }

    void setSize(const size_t newSize)
    {
        const size_t clampedSize = std::min(newSize, MAXSIZE - 2);
        if (m_activeFade)
        {
            m_scheduledDelayWidth = clampedSize;
            m_hasScheduledChange = true;
            return;
        }

        if (clampedSize == m_currentDelayWidth)
        {
            return;
        }

        m_targetDelayWidth = clampedSize;
        m_activeFade = true;
        m_fadeStep = 0;
        m_currentFadeIn = 0.f;
        m_currentFadeOut = 1.f;

        const size_t inactiveHead = 1 - m_currentReadHead;
        m_targetRead = m_head - m_targetDelayWidth + MAXSIZE - 1;
        while (m_targetRead >= m_buffer.size())
        {
            m_targetRead -= m_buffer.size();
        }
        m_read[inactiveHead] = m_targetRead;
    }

    float step(const float in)
    {
        float result;

        if (m_activeFade)
        {
            const float sample1 = m_buffer[m_read[m_currentReadHead]];
            const float sample2 = m_buffer[m_read[1 - m_currentReadHead]];

            result = sample1 * m_currentFadeOut + sample2 * m_currentFadeIn;

            m_read[0]++;
            if (m_read[0] >= m_buffer.size())
            {
                m_read[0] = 0;
            }
            m_read[1]++;
            if (m_read[1] >= m_buffer.size())
            {
                m_read[1] = 0;
            }

            m_fadeStep++;

            if (const float fadeProgress = static_cast<float>(m_fadeStep) / m_fadeWindowSize; fadeProgress >= 1.f)
            {
                m_activeFade = false;
                m_currentDelayWidth = m_targetDelayWidth;
                m_currentReadHead = 1 - m_currentReadHead;
                m_currentFadeIn = 0.f;
                m_currentFadeOut = 1.f;
                m_read[1 - m_currentReadHead] = m_read[m_currentReadHead];
                result = sample2;

                if (m_hasScheduledChange)
                {
                    m_hasScheduledChange = false;
                    setSize(m_scheduledDelayWidth);
                }
            }
            else
            {
                m_currentFadeIn = fadeProgress;
                m_currentFadeOut = 1.f - fadeProgress;
            }
        }
        else
        {
            result = m_buffer[m_read[m_currentReadHead]];
            m_read[m_currentReadHead]++;
            if (m_read[m_currentReadHead] >= m_buffer.size())
            {
                m_read[m_currentReadHead] = 0;
            }
        }

        m_buffer[m_head++] = m_allPass.step(in);
        if (m_head >= m_buffer.size())
        {
            m_head = 0;
        }

        return result;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](const float in) { return step(in); });
    }

  private:
    float m_sampleRate;
    AllPass m_allPass;
    size_t m_currentDelayWidth{MAXSIZE / 8};
    size_t m_targetDelayWidth{MAXSIZE / 8};
    size_t m_scheduledDelayWidth;
    bool m_hasScheduledChange;
    size_t m_head{0};
    std::array<size_t, 2> m_read{};
    size_t m_currentReadHead{0};
    float m_fadeWindowSize{1024};
    float m_currentFadeIn;
    float m_currentFadeOut;
    size_t m_fadeStep;
    bool m_activeFade;
    size_t m_targetRead;
    std::vector<float> m_buffer;
};

}