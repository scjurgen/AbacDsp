#pragma once

#include <algorithm>
#include <cmath>
#include <random>
#include <span>
#include <vector>

#include "Numbers/Interpolation.h"

namespace AbacDsp
{
template <size_t MAXSIZE>
class PitchFadeWindowDelay
{
  public:
    static constexpr size_t MaxInterpolationWidth{4};

    PitchFadeWindowDelay()
        : m_buffer(MAXSIZE + MaxInterpolationWidth, 0.f)
    {
        updateGeometry();
    }

    void setSize(const size_t newSize) noexcept
    {
        m_requestedWindow = newSize;
        updateGeometry();
    }

    void setFadeTime(const size_t t) noexcept
    {
        m_requestedFadeTime = t;
        updateGeometry();
    }

    [[nodiscard]] float step(const float in)
    {
        float returnValue = 0.f;

        auto& rdHd = m_readHeads;
        if (!rdHd.fade)
        {
            returnValue = getFractional(rdHd.fadeInPos);
            // the handoff in triggerFade takes over this position, so it has to advance
            // first: otherwise the outgoing head repeats its last sample and clicks
            advanceFade(rdHd.fadeInPos);
            if (--rdHd.plainSteps == 0)
            {
                triggerFade();
            }
        }
        else
        {
            const auto fadeInGain = static_cast<float>(rdHd.fadeTime - rdHd.fadeCount) * rdHd.fadeStep;
            returnValue =
                getFractional(rdHd.fadeInPos) * fadeInGain + getFractional(rdHd.fadeOutPos) * (1.f - fadeInGain);
            advanceFade(rdHd.fadeInPos);
            advanceFade(rdHd.fadeOutPos);
            if (--rdHd.fadeCount == 0)
            {
                rdHd.fade = false;
                rdHd.plainSteps = m_window - m_fadeTime * 2;
            }
        }

        if (m_head < MaxInterpolationWidth)
        {
            m_buffer[m_head + m_maxSize] = in;
        }

        m_buffer[m_head++] = in;
        m_head %= m_maxSize;
        return returnValue;
    }

    void setReverse(const bool reverse) noexcept
    {
        m_reverse = reverse;
        updateGeometry();
    }

    void setPitch(const float semitones) noexcept
    {
        setPitchRatio(std::pow(2.f, semitones / 12.f));
    }

    void setPitchRatio(const float ratio) noexcept
    {
        if (std::fpclassify(ratio) == FP_ZERO)
        {
            return;
        }
        m_readHeads.advance = ratio;
        updateGeometry();
    }

    void processBlock(std::span<const float> source, std::span<float> target)
    {
        for (size_t i = 0; i < source.size(); ++i)
        {
            target[i] = step(source[i]);
        }
    }

  private:
    void triggerFade()
    {
        m_readHeads.fade = true;
        m_readHeads.fadeOutPos = m_readHeads.fadeInPos;
        m_readHeads.fadeTime = m_fadeTime;
        m_readHeads.fadeCount = m_fadeTime;
        m_readHeads.fadeStep = 1.f / static_cast<float>(m_fadeTime - 1);
        m_readHeads.fadeInPos = calcMaterialInPosition();
    }

    void advanceFade(float& fadePos) noexcept
    {
        fadePos += m_reverse ? -m_readHeads.advance : m_readHeads.advance;
        while (fadePos < 0.f)
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
        const auto idx = static_cast<size_t>(std::floor(position));
        const float fractional = position - static_cast<float>(idx);
        // hermite43x interpolates between y[1] and y[2], so the window starts one sample early
        const auto base = (idx + m_maxSize - 1) % m_maxSize;
        return Interpolation::hermite43x(&m_buffer[base], fractional);
    }

    /*
     * How far a read head drifts against the write head per sample. A grain lives
     * exactly m_window samples, so its total drift is factor * m_window; once that
     * exceeds the buffer the head laps the write head mid-grain and the material
     * jumps by a whole buffer length, which is the audible click.
     */
    [[nodiscard]] float driftFactor() const noexcept
    {
        const float advance = m_readHeads.advance;
        if (m_reverse)
        {
            return 1.f + advance;
        }
        return advance > 1.f ? advance : 1.f - advance;
    }

    void updateGeometry() noexcept
    {
        constexpr float MinDrift{1.f / 1024.f};
        const float headRoom = static_cast<float>(m_maxSize - m_randomVariation - MaxInterpolationWidth - 2);
        const float driftLimit = std::floor(headRoom / std::max(driftFactor(), MinDrift));
        const auto maxWindow = static_cast<size_t>(std::min(driftLimit, static_cast<float>(m_maxSize - 1)));

        m_window = std::clamp(std::min(m_requestedWindow, maxWindow), MinWindow, m_maxSize - 1);
        // a grain must be fadeIn + at least one plain sample + fadeOut
        m_fadeTime = std::clamp(m_requestedFadeTime, size_t{2}, (m_window - 1) / 2);
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
        const float advance = m_readHeads.advance;
        const float offset = m_reverse || (advance <= 1.0f) ? static_cast<float>(m_maxSize) - 2.f
                                                            : -advance * static_cast<float>(m_window) - 1.f;
        float pos = static_cast<float>(m_head) + offset;
        pos -= static_cast<float>(m_randDistribution(m_randomGenerator));

        while (pos >= static_cast<float>(m_maxSize))
        {
            pos -= static_cast<float>(m_maxSize);
        }
        while (pos < 0.f)
        {
            pos += static_cast<float>(m_maxSize);
        }
        return std::round(pos);
    }

    struct ReadHead
    {
        float fadeInPos{MAXSIZE * 0.25f};
        float fadeOutPos{MAXSIZE * 0.75f};
        float advance{0.001f};
        size_t fadeTime{2};
        size_t fadeCount{0};
        size_t plainSteps{1};
        float fadeStep{1.f};
        bool fade{false};
    };

    static constexpr size_t MinWindow{5};

    ReadHead m_readHeads{};

    std::vector<float> m_buffer;
    static constexpr size_t m_maxSize{MAXSIZE};
    size_t m_requestedWindow{MAXSIZE};
    size_t m_requestedFadeTime{MAXSIZE / 4};
    size_t m_window{MAXSIZE};
    size_t m_fadeTime{MAXSIZE / 4};
    size_t m_head{0};
    bool m_reverse{false};
    static constexpr size_t m_randomVariation{200};
    std::minstd_rand m_randomGenerator;
    std::uniform_int_distribution<size_t> m_randDistribution{0, m_randomVariation};
};

}
