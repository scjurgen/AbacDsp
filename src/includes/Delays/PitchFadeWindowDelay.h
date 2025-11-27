#pragma once

#include "Numbers/Interpolation.h"

#include <random>
#include <vector>

namespace AbacDsp
{
template <size_t MAXSIZE>
class PitchFadeWindowDelay
{
  public:
    static constexpr size_t MaxInterpolationWidth{2};

    PitchFadeWindowDelay()
        : m_buffer(MAXSIZE + MaxInterpolationWidth, 0.f)
        , m_fadeBuffer(MAXSIZE / 2, 0.f)
    {
        setFadeTime(10000);
    }

    void setSize(const size_t newSize)
    {
        m_readHeads.size = std::min(newSize, m_maxSize - 1);
    }

    void setFadeTime(const size_t t)
    {
        m_readHeads.fadeTime = std::min(t, m_fadeBuffer.size() - 1);
        m_readHeads.fadeStep = 1.0f / m_readHeads.fadeTime;
        const float invFadeTime = 1.0f / m_readHeads.fadeTime;
        for (size_t i = 0; i < m_readHeads.fadeTime; ++i)
        {
            m_fadeBuffer[i] = static_cast<float>(i) * invFadeTime;
        }
    }

    float step(const float in)
    {
        float returnValue = 0.f;

        auto& rdHd = m_readHeads;
        if (!rdHd.fade)
        {
            returnValue += getFractional(rdHd.fadeInPos);
            if (--rdHd.plainSteps == 0)
            {
                triggerFade();
            }
        }
        else
        {
            const auto fadeInFactor = m_fadeBuffer[rdHd.fadeTime - rdHd.fadeCount];
            const auto fadeOutFactor = m_fadeBuffer[rdHd.fadeCount - 1];
            returnValue = getFractional(rdHd.fadeInPos) * fadeInFactor + getFractional(rdHd.fadeOutPos) * fadeOutFactor;
            rdHd.fadeInGain += rdHd.fadeStep;
            rdHd.fadeOutGain -= rdHd.fadeStep;
            advanceFade(rdHd.fadeOutPos);
            if (--rdHd.fadeCount <= 0)
            {
                rdHd.fade = false;
                if (rdHd.fadeTime * 2 > rdHd.size)
                {
                    rdHd.plainSteps = 1;
                }
                else
                {
                    rdHd.plainSteps = rdHd.size - rdHd.fadeTime * 2;
                }
            }
        }
        advanceFade(rdHd.fadeInPos);

        if (m_head < MaxInterpolationWidth)
        {
            m_buffer[m_head + m_maxSize] = in;
        }

        m_buffer[m_head++] = in;
        m_head = m_head % m_maxSize;
        return returnValue;
    }

    void setReverse(const bool reverse)
    {
        m_reverse = reverse;
    }

    void setPitch(const float semitones)
    {
        const float ratio = std::pow(2.f, semitones / 12.f);
        setPitchRatio(ratio);
    }

    void setPitchRatio(const float ratio)
    {
        if (std::fpclassify(ratio) == FP_ZERO)
        {
            return;
        }
        m_readHeads.advance = ratio;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            target[i] = step(source[i]);
        }
    }

  private:
    void triggerFade()
    {
        m_readHeads.fade = true;
        m_readHeads.fadeOutPos = m_readHeads.fadeInPos;
        m_readHeads.fadeCount = m_readHeads.fadeTime;
        m_readHeads.fadeInGain = 0;
        m_readHeads.fadeOutGain = 1.0;
        m_readHeads.fadeInPos = calcMaterialInPosition();
    }

    void advanceFade(float& fadePos)
    {
        fadePos += m_reverse ? -m_readHeads.advance : m_readHeads.advance;
        while (fadePos < 0)
        {
            fadePos += m_maxSize;
        }
        while (fadePos >= m_maxSize)
        {
            fadePos -= m_maxSize;
        }
    }

    [[nodiscard]] float getFractional(const float position) const
    {
        const auto idx = static_cast<size_t>(floorf(position));
        const float fractional = position - static_cast<float>(idx);
        // whenever using another interpolation set the MaxInterpolationWidth
        return Interpolation::linearPt2(&m_buffer[idx % m_maxSize], fractional);
    }

    [[nodiscard]] float calcMaterialInPosition()
    {
        /*
         * play out slower: m_head-2 (fade over set time)
         * play out faster: m_head - speed * size e.g.
         *
         * forward
         * 0       r           h           w
         * |-------|>----------|>----------|
         * h = head, v=speed, w = windowsize
         * pr = h-w+t*v
         * pf = h+t
         * -w+t*v = t ==>  t = w/(v-1)   v!=1
         *
         * max t = w/(v-1)
         * w = t*(v-1)
         *
         * backward
         * 0                   h            w
         * |-----------------<||>----------|
         * h = head, v=speed, w = windowsize
         *
         * pr = h-t*v
         * ph = h-w+t
         * h-t*v = h-w+t  => -t*v = -w+t    w = t+t*v => t = w/(1+v)    v != -1
         *
         * max t = w/(1+v)
         *
         */
        float pos = m_head + (m_reverse || (m_readHeads.advance <= 1.0f) ? m_maxSize - 2.f
                                                                         : -m_readHeads.advance * m_readHeads.size - 1);
        pos -= m_randDistribution(m_randomGenerator);

        while (pos >= m_maxSize)
        {
            pos -= m_maxSize;
        }
        while (pos < 0)
        {
            pos += m_maxSize;
        }
        return round(pos);
    }

    struct ReadHead
    {
        float fadeInPos{MAXSIZE * 0.25f};
        float fadeOutPos{MAXSIZE * 0.75f};
        float advance{0.001f};
        size_t size{MAXSIZE};
        float fadeInGain{1};
        float fadeOutGain{0};
        size_t fadeTime{MAXSIZE / 4};
        size_t fadeCount{0};
        size_t plainSteps{1};
        float fadeStep{1.f / fadeTime};
        bool fade{false};
    };

    ReadHead m_readHeads{};

    std::vector<float> m_buffer;
    std::vector<float> m_fadeBuffer;
    static constexpr size_t m_maxSize{MAXSIZE};
    size_t m_head{0};
    bool m_reverse{false};
    static constexpr size_t m_randomVariation{200};
    std::minstd_rand m_randomGenerator;
    std::uniform_int_distribution<size_t> m_randDistribution{0, m_randomVariation};
};
}