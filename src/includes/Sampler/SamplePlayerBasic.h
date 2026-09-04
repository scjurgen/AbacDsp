#pragma once

#include <memory>
#include <vector>

#include "Filters/Biquad.h"

namespace AbacDsp
{

/**
 * @ingroup sampler
 * @brief Single-voice stereo sample playback at a fractional read rate.
 *
 * One voice, one shared_ptr to the data, optional looping. The lowpass tracks
 * the read rate: playing a sample faster shifts its content up, and without
 * filtering the top of that content folds back as alias.
 */
class SamplePlayerBasic
{
  public:
    explicit SamplePlayerBasic(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_lowPassFilter(sampleRate)
    {
    }

    void runStereo(const std::shared_ptr<std::vector<float>>& data)
    {
        m_data = data;
    }

    void restart()
    {
        m_isDone = false;
        m_intPlayPos = 0;
        m_frac = 0.0f;
    }

    void setLoop(const bool loop)
    {
        m_loop = loop;
    }

    void setPlaybackRate(const float rate)
    {
        m_advanceFactor = rate;
        m_needsFiltering = rate < 1.0f;
        if (m_needsFiltering)
        {
            const float cutoffFreq = (m_sampleRate * 0.5f) * rate * 0.8f;
            m_lowPassFilter.computeType1(4, cutoffFreq, 0.5f, true);
        }
    }

    void processBlock(float* targetAddLeft, float* targetAddRight, const size_t numSamples)
    {
        if (m_isDone || !m_data)
        {
            return;
        }

        const size_t dataSize = m_data->size() / 2;
        const float* samples = m_data->data();

        for (size_t i = 0; i < numSamples; ++i)
        {
            if (m_intPlayPos + 2 >= dataSize)
            {
                if (m_loop)
                {
                    m_intPlayPos = 0;
                    m_frac = 0.0f;
                    continue;
                }
                m_isDone = true;
                return;
            }

            const size_t pPos = m_intPlayPos * 2;
            const auto ym1_L = samples[pPos];
            const auto y0_L = samples[pPos + 2];
            const auto y1_L = samples[pPos + 4];
            const auto y2_L = samples[pPos + 6];

            const auto ym1_R = samples[pPos + 1];
            const auto y0_R = samples[pPos + 3];
            const auto y1_R = samples[pPos + 5];
            const auto y2_R = samples[pPos + 7];

            const auto outL = hermiteInterpolation(ym1_L, y0_L, y1_L, y2_L, m_frac);
            const auto outR = hermiteInterpolation(ym1_R, y0_R, y1_R, y2_R, m_frac);

            targetAddLeft[i] += outL;
            targetAddRight[i] += outR;

            m_frac += m_advanceFactor;
            const auto increment = static_cast<size_t>(m_frac);
            m_frac -= static_cast<float>(increment);
            m_intPlayPos += increment;
        }

        if (m_needsFiltering)
        {
            m_lowPassFilter.processBlockStereo(targetAddLeft, targetAddRight, targetAddLeft, targetAddRight,
                                               numSamples);
        }
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_isDone;
    }

  private:
    [[nodiscard]] static float hermiteInterpolation(const float ym1, const float y0, const float y1, const float y2,
                                                    const float frac) noexcept
    {
        const auto c0 = y0;
        const auto c1 = 0.5f * (y1 - ym1);
        const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    std::shared_ptr<std::vector<float>> m_data{};
    size_t m_intPlayPos{0};
    float m_frac{0.0f};
    float m_advanceFactor{1.0f};
    bool m_loop{false};
    bool m_isDone{true};
    const float m_sampleRate;
    ChebyshevBiquad m_lowPassFilter;
    bool m_needsFiltering{false};
};

}
