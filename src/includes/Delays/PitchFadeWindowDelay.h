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
                rdHd.plainSteps = rdHd.plainAfterFade;
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
        const auto grain = planGrain();
        m_readHeads.fade = true;
        m_readHeads.fadeOutPos = m_readHeads.fadeInPos;
        m_readHeads.fadeTime = std::clamp(m_requestedFadeTime, size_t{2}, (grain.life - 1) / 2);
        m_readHeads.fadeCount = m_readHeads.fadeTime;
        m_readHeads.fadeStep = 1.f / static_cast<float>(m_readHeads.fadeTime - 1);
        m_readHeads.plainAfterFade = grain.life - m_readHeads.fadeTime * 2;
        m_readHeads.fadeInPos = calcMaterialInPosition(grain.span);
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

    // How far a read head moves against the write head per sample.
    [[nodiscard]] float driftFactor() const noexcept
    {
        const float advance = m_readHeads.advance;
        if (m_reverse)
        {
            return 1.f + advance;
        }
        return advance > 1.f ? advance - 1.f : 1.f - advance;
    }

    struct Grain
    {
        size_t life{MinLife};
        float span{0.f};
    };

    /*
     * A grain only has to be reset once its head has drifted a window away from where it
     * started, which is what takes window/drift samples: the smaller the pitch shift, the
     * longer a grain stays valid, and at unity it never expires at all. MaxGrainLife caps
     * that, both to keep refreshing the material and to bound the excursion, since span is
     * how far the delay wanders before the head is put back.
     */
    [[nodiscard]] Grain planGrain() const noexcept
    {
        constexpr float MinDrift{1.f / 1024.f};
        const float drift = driftFactor();
        const float lifeLimit = static_cast<float>(m_window) / std::max(drift, MinDrift);
        const auto life = std::max(static_cast<size_t>(std::min(lifeLimit, static_cast<float>(MaxGrainLife))), MinLife);
        return {life, drift * static_cast<float>(life)};
    }

    void updateGeometry() noexcept
    {
        constexpr size_t HeadRoom{m_randomVariation + MinAge + MaxInterpolationWidth};
        m_window = std::clamp(m_requestedWindow, MinWindow, m_maxSize - HeadRoom);
    }

    /*
     * Where to drop a new head, as an age: how far behind the write head it reads.
     *
     * forward, v > 1
     * 0            r                        h
     * |------------|>>-------------------->||
     * the head reads faster than the write head, so it starts a span back and closes on
     * it, arriving at MinAge exactly as the grain ends: t = span/(v-1)
     *
     * forward v < 1, or reverse
     * 0            r                        h
     * |<<----------|<--------------------->||
     * the head loses ground, so it starts at MinAge and falls back by a span over its
     * life: t = span/(1-v) forward, t = span/(1+v) reverse
     *
     * The jitter always pushes away from the write head, never toward it: a catching up
     * head that started jitter closer would end up jitter past it, reading unwritten
     * samples.
     */
    [[nodiscard]] float calcMaterialInPosition(const float span)
    {
        const bool catchingUp = !m_reverse && m_readHeads.advance > 1.f;
        const auto jitter = static_cast<float>(m_randDistribution(m_randomGenerator));
        const float age = static_cast<float>(MinAge) + jitter + (catchingUp ? span : 0.f);

        float pos = static_cast<float>(m_head) - age;
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
        size_t plainAfterFade{1};
        float fadeStep{1.f};
        bool fade{false};
    };

    // hermite43x reads one sample either side, so a head any closer would interpolate
    // across samples the write head has not reached yet
    static constexpr size_t MinAge{4};
    static constexpr size_t MinWindow{5};
    // a grain must be fadeIn + at least one plain sample + fadeOut
    static constexpr size_t MinLife{5};
    // bounds how far the delay wanders before a head is put back near the write head
    static constexpr size_t MaxGrainLife{5000};

    ReadHead m_readHeads{};

    std::vector<float> m_buffer;
    static constexpr size_t m_maxSize{MAXSIZE};
    size_t m_requestedWindow{MAXSIZE};
    size_t m_requestedFadeTime{MAXSIZE / 4};
    size_t m_window{MAXSIZE};
    size_t m_head{0};
    bool m_reverse{false};
    static constexpr size_t m_randomVariation{200};
    std::minstd_rand m_randomGenerator;
    std::uniform_int_distribution<size_t> m_randDistribution{0, m_randomVariation};
};

}
