#pragma once

#include "AudioProcessing.h"
#include "Filters/Biquad.h"
#include "Filters/OnePoleFilter.h"
#include "Filters/FourStageFilter.h"
#include "InterpolationCollection.h"
#include "Modulation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>


namespace AbacDsp
{

template <size_t MaxSize48Khz, size_t BlockSize>
class AllPassDelay
{
  public:
    static constexpr size_t minDelaySize{
        51u}; // don't allow delay lines shorter than 51, ( a wall at the distance of 30cm [@ 48kHz] )

    explicit AllPassDelay(float sampleRate)
        : m_sampleRate(sampleRate)
        , m_maxBufferSize(static_cast<size_t>(std::round(static_cast<float>(MaxSize48Khz) * sampleRate / 48000.f)))
        , m_buffer(m_maxBufferSize + 6, 0.f)
    {
    }

    void clear()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void newFadeIfNeeded()
    {
        if (m_newFadeSize && !m_fadeSteps)
        {
            m_fadeFactorIn = 0.0f;
            m_fadeFactorOut = 1.0f;
            m_fadeSteps = std::min<size_t>(8192, m_currentDelayWidth / 2);
            m_fadeAdvance = 1.0f / static_cast<float>(m_fadeSteps);
            m_headRead[1] = (m_headWrite + m_maxBufferSize - m_newFadeSize) % m_maxBufferSize;
            m_currentDelayWidth = m_newFadeSize;
        }
    }

    [[nodiscard]] float getDecayTimeInSamples(const float db = -60.f) const
    {
        const auto f = Convert::dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setFeedback(const float gain)
    {
        m_feedback = std::clamp(gain, -0.99f, 0.99f);
    }

    void setSize(const size_t newSize)
    {
        setSizeImpl<false>(newSize);
    }

    void setSize(const size_t newSize, const SkipSmoothing_t&)
    {
        setSizeImpl<true>(newSize);
    }

    void feedWrite(const float in)
    {
        m_buffer[m_headWrite] = in;
        if (++m_headWrite >= m_maxBufferSize)
        {
            m_headWrite = 0;
        }
    }

    float step(const float in)
    {
        auto getResult = [this]()
        {
            if (m_fadeSteps)
            {
                const auto outValue = nextHeadRead(0) * m_fadeFactorOut;
                const auto inValue = nextHeadRead(1) * m_fadeFactorIn;
                m_fadeFactorOut -= m_fadeAdvance;
                m_fadeFactorIn += m_fadeAdvance;
                if (--m_fadeSteps == 0)
                {
                    m_headRead[0] = m_headRead[1];
                    m_newFadeSize = m_newFadeSizeScheduled;
                    m_newFadeSizeScheduled = 0;
                }
                return outValue + inValue;
            }
            return nextHeadRead(0);
        };
        const float delayed = getResult();
        const auto output = -m_feedback * in + delayed;
        const auto toWrite = in + m_feedback * delayed;
        feedWrite(toWrite);
        return output;
    }

    float nextHeadRead(const size_t index)
    {
        const float returnValue = m_buffer[m_headRead[index]];
        m_headRead[index] = (m_headRead[index] + 1) % m_maxBufferSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target)
    {
        std::transform(source, source + BlockSize, target, [this](float in) { return step(in); });
    }

    void processBlockInplace(float* inplace, size_t numSamples)
    {
        processBlock(inplace, inplace);
    }

    [[nodiscard]] size_t size() const
    {
        return m_currentDelayWidth;
    }

  private:
    template <bool fastSetting>
    void setSizeImpl(const size_t newSize)
    {
        const auto clampedSize = std::clamp<size_t>(newSize, minDelaySize, m_maxBufferSize);
        if (clampedSize == m_currentDelayWidth)
        {
            return;
        }
        if constexpr (fastSetting)
        {
            m_currentDelayWidth = clampedSize;
            m_newFadeSizeScheduled = 0;
            m_newFadeSize = 0;
            m_headWrite = m_currentDelayWidth;
            m_headRead[0] = 0;
            return;
        }
        if (m_newFadeSize)
        {
            m_newFadeSizeScheduled = clampedSize;
        }
        else
        {
            m_newFadeSize = clampedSize;
        }
    }

    float m_sampleRate;
    float m_feedback{0.0f};
    size_t m_headWrite{0};
    std::array<size_t, 2> m_headRead{0, 0};
    size_t m_fadeSteps{0};
    float m_fadeFactorIn{0.0f};
    float m_fadeFactorOut{0.0f};
    float m_fadeAdvance{0.0f};
    size_t m_newFadeSize{0};
    size_t m_newFadeSizeScheduled{0};
    size_t m_currentDelayWidth{MaxSize48Khz / 8};
    size_t m_maxBufferSize{MaxSize48Khz};
    std::vector<float> m_buffer{};
};

template <size_t MaxSize48Khz>
class ModulatingAllPassDelay
{
  public:
    static constexpr float maxFilterFrequency{
        20001.f}; // lowpass only active if frequency under 20001 (20kHz inclusive)
    static constexpr size_t minDelaySize{
        51u}; // don't allow delay lines shorter than 51, ( a wall at the distance of 30cm [@ 48kHz] )

    explicit ModulatingAllPassDelay(float sampleRate)
        : m_sampleRate(sampleRate)
        , m_lowpass(sampleRate)
        , m_allpass(sampleRate)
        , m_modulation(m_sampleRate / 16)
        , m_maxBufferSize(static_cast<size_t>(std::round(static_cast<float>(MaxSize48Khz) * sampleRate / 48000.f)))
        , m_buffer(m_maxBufferSize + 6, 0.f)
    {
    }

    ~ModulatingAllPassDelay() = default;
    ModulatingAllPassDelay(const ModulatingAllPassDelay&) = delete;
    ModulatingAllPassDelay& operator=(const ModulatingAllPassDelay&) = delete;
    ModulatingAllPassDelay(const ModulatingAllPassDelay&&) = delete;
    ModulatingAllPassDelay& operator=(const ModulatingAllPassDelay&&) = delete;

    void clear()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void newFadeIfNeeded()
    {
        if (m_newFadeSize && !m_fadeSteps)
        {
            m_fadeFactorIn = 0.0f;
            m_fadeFactorOut = 1.0f;
            m_fadeSteps = std::min<size_t>(8192, m_currentDelayWidth / 2);
            m_fadeAdvance = 1.0f / static_cast<float>(m_fadeSteps);
            m_headRead[1] = (m_headWrite + m_maxBufferSize - m_newFadeSize) % m_maxBufferSize;
            m_currentDelayWidth = m_newFadeSize;
        }
    }

    [[nodiscard]] float getDecayTimeInSamples(const float db = -60.f) const
    {
        const auto f = dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setLowpass(const float value)
    {
        m_useLowPass = value < maxFilterFrequency;
        m_lowpass.setCutoff(value);
    }

    void setAllpass(const float value)
    {
        m_useAllPass = value < maxFilterFrequency;
        m_allpass.setCutoff(value);
    }

    void setFeedback(const float gain)
    {
        m_feedback = std::clamp(gain, -0.99f, 0.99f);
    }

    void setSize(const size_t newSize)
    {
        setSizeImpl<false>(newSize);
    }

    void setSize(const size_t newSize, const SkipSmoothing_t&)
    {
        setSizeImpl<true>(newSize);
    }

    void setModulationDepth(const float depth)
    {
        m_modulationDepth = depth * 100.f;
        trimModulationDepth();
    }

    void setModulationSpeed(const float speedHz)
    {
        m_modulation.setModulationSpeed(speedHz);
    }

    void feedWrite(const float in)
    {
        if (m_useLowPass)
        {
            m_buffer[m_headWrite] = m_lowpass.step(in);
        }
        else
        {
            m_buffer[m_headWrite] = in;
        }

        if (m_useAllPass)
        {
            m_buffer[m_headWrite] = m_allpass.step(m_buffer[m_headWrite]);
        }

        // replicate values of begin at end, so we don't need to do handle InterPolate on splitted buffer
        if (m_headWrite < 6)
        {
            const size_t padIndex = m_headWrite + m_maxBufferSize;
            m_buffer[padIndex] = m_buffer[m_headWrite];
        }

        if (++m_headWrite >= m_maxBufferSize)
        {
            m_headWrite = 0;
        }
    }

    float step(const float in)
    {
        if (++m_tick == 16)
        {
            m_tick = 0;
            m_modulation.tick();
            newFadeIfNeeded();
        }
        auto getResult = [this]()
        {
            if (m_fadeSteps)
            {
                const auto outValue = nextHeadRead(0) * m_fadeFactorOut;
                const auto inValue = nextHeadRead(1) * m_fadeFactorIn;
                m_fadeFactorOut -= m_fadeAdvance;
                m_fadeFactorIn += m_fadeAdvance;
                if (--m_fadeSteps == 0)
                {
                    m_headRead[0] = m_headRead[1];
                    m_newFadeSize = m_newFadeSizeScheduled;
                    m_newFadeSizeScheduled = 0;
                }
                return outValue + inValue;
            }
            else
            {
                return nextHeadRead(0);
            }
        };
        const float delayed = getResult();
        const auto output = -m_feedback * in + delayed;
        const auto toWrite = in + m_feedback * delayed;
        feedWrite(toWrite);
        return output;

        // const float result = getResult();
        // const auto feedDelay = in - result * m_feedback; // N.B. negative feedback
        // const auto ret = feedDelay * m_feedback + result;
        // feedWrite(feedDelay);
        // return ret;
    }

    float nextHeadRead(const size_t index)
    {
        auto getReturnValue = [this](const size_t index)
        {
            if (m_modulation.isModulating())
            {
                const auto [depth, fraction] = m_modulation.lastValuePair();
                auto dHead = m_headRead[index] + static_cast<size_t>(depth);
                if (dHead >= m_maxBufferSize)
                {
                    dHead -= m_maxBufferSize;
                }
                return Interpolation<float>::bspline_43x(&m_buffer[dHead], fraction);
            }
            else
            {
                return m_buffer[m_headRead[index]];
            }
        };

        const float returnValue = getReturnValue(index);
        m_headRead[index] = (m_headRead[index] + 1) % m_maxBufferSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](float in) { return step(in); });
    }

    void processBlockInplace(float* inplace, size_t numSamples)
    {
        std::transform(inplace, inplace + numSamples, inplace, [this](float in) { return step(in); });
    }

    [[nodiscard]] size_t size() const
    {
        return m_currentDelayWidth;
    }

  private:
    template <bool fastSetting>
    void setSizeImpl(const size_t newSize)
    {
        const auto clampedSize = std::clamp<size_t>(newSize, minDelaySize, m_maxBufferSize);
        if (clampedSize == m_currentDelayWidth)
        {
            return;
        }
        if constexpr (fastSetting)
        {
            m_currentDelayWidth = clampedSize;
            m_newFadeSizeScheduled = 0;
            m_newFadeSize = 0;
            m_headWrite = m_currentDelayWidth;
            m_headRead[0] = 0;
            return;
        }
        if (m_newFadeSize)
        {
            m_newFadeSizeScheduled = clampedSize;
        }
        else
        {
            m_newFadeSize = clampedSize;
        }
    }


    void trimModulationDepth()
    {
        m_modulation.setModulationDepth(std::min(static_cast<float>(m_maxBufferSize - 3), m_modulationDepth));
    }
    float m_sampleRate;

    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowpass;
    Ap6Smooth m_allpass;

    bool m_useLowPass{true};
    bool m_useAllPass{false};

    float m_feedback{0.0f};

    size_t m_headWrite{0};
    std::array<size_t, 2> m_headRead{0, 0};

    // fade strategy
    size_t m_fadeSteps{0};
    float m_fadeFactorIn{0.0f};
    float m_fadeFactorOut{0.0f};
    float m_fadeAdvance{0.0f};
    size_t m_newFadeSize{0};
    size_t m_newFadeSizeScheduled{0};

    // adapt soft buffersize *speed up/down* could be also an option (pitching adaption like Eventide does):
    // Addendum: Hendrik prefers non pitching
    size_t m_currentDelayWidth{MaxSize48Khz / 8};
    size_t m_tick{0};
    Modulation m_modulation;
    float m_modulationDepth{0};
    size_t m_maxBufferSize{MaxSize48Khz};
    std::vector<float> m_buffer{};
};


template <bool positiveOnly>
class ModulatingAllPassDelayNoSoftAdapt
{
  public:
    ModulatingAllPassDelayNoSoftAdapt(const float sampleRate, const size_t maxSize)
        : m_sampleRate(sampleRate)
        , m_lowPass(sampleRate)
        , m_dispersionFilter(sampleRate)
        , m_modulation(sampleRate / 16)
    {
        m_dispersionFilter.setSmoothingSteps(0);
        // m_dispersionFilter.computeCoefficients(m_sampleRate, 1000, 0.707, 0);
        m_maxSize = maxSize;
        m_buffer.resize(maxSize + 6);
        m_currentDelayWidth = maxSize / 8;
    }


    void clear()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    float getDecayTimeInSamples(const float db = -60.f)
    {
        const auto f = dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setLowpass(const float value)
    {
        m_lowPass.setCutoff(value);
    }

    void setAllpass(const float value)
    {
        m_dispersionFilter.setCutoff(value);
        //        m_dispersionFilter.computeCoefficients(m_sampleRate, value, 0.7071067f, 0);
    }

    void setFeedback(const float gain)
    {
        m_feedback = std::clamp(gain, -1.f, 1.f);
    }

    void setFeedbackNoClamp(const float gain)
    {
        m_feedback = gain;
    }

    void reset()
    {
        m_dispersionFilter.reset();
        m_lowPass.reset();
        std::fill_n(m_buffer.data(), m_buffer.size(), 0);
    }

    void setSize(const size_t newSize)
    {
        const auto clampedSize = std::clamp<size_t>(newSize, 2, m_maxSize);
        m_currentDelayWidth = clampedSize;
        m_headRead = (m_headWrite + m_maxSize - m_currentDelayWidth) % m_maxSize;
    }

    void setModulationDepth(const float depth)
    {
        m_modulationDepth = depth * 100.f;
        trimModulationDepth();
    }

    void setModulationSpeed(float speedHz)
    {
        m_modulation.setModulationSpeed(speedHz);
    }

    void feedWrite(float in)
    {
        // m_buffer[m_headWrite] = in;
        m_buffer[m_headWrite] = m_lowPass.step(in);

        // m_buffer[m_headWrite] = m_dispersionFilter.singleStepAllPass(m_buffer[m_headWrite]);
        m_buffer[m_headWrite] = m_dispersionFilter.step(m_buffer[m_headWrite]);

        // replicate values of begin at end, so we don't need to do handle InterPolate on splitted buffer
        if (m_headWrite < 6)
        {
            size_t padIndex = m_headWrite + m_maxSize;
            m_buffer[padIndex] = m_buffer[m_headWrite];
        }

        if (++m_headWrite >= m_maxSize)
        {
            m_headWrite = 0;
        }
    }


    float step(const float in)
    {
        if (++m_tick == 16)
        {
            m_tick = 0;
            m_modulation.tick();
        }

        const float delayed = nextHeadRead();

        if (positiveOnly)
        {
            const auto toWrite = in + delayed * m_feedback;
            const auto output = toWrite * m_feedback + delayed;
            feedWrite(toWrite);
            return output;
        }
        else
        {
            // Schroeder allpass
            const auto output = -m_feedback * in + delayed;
            const auto toWrite = in + m_feedback * delayed;
            feedWrite(toWrite);
            return output;
        }
    }

    float nextHeadRead()
    {
        float returnValue;
        if (m_modulation.isModulating())
        {
            const auto [depth, fraction] = m_modulation.lastValuePair();
            auto dHead = m_headRead + static_cast<size_t>(depth);
            if (dHead >= m_maxSize)
            {
                dHead -= m_maxSize;
            }
            returnValue = Interpolation<float>::linearPt2(&m_buffer[dHead], fraction);
        }
        else
        {
            returnValue = m_buffer[m_headRead];
        }
        m_headRead = (m_headRead + 1) % m_maxSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target, size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](float in) { return step(in); });
    }
    void processBlockInplace(float* inplace, size_t numSamples)
    {
        std::transform(inplace, inplace + numSamples, inplace, [this](float in) { return step(in); });
    }

    [[nodiscard]] size_t size() const
    {
        return m_currentDelayWidth;
    }

  private:
    float m_sampleRate;
    void trimModulationDepth()
    {
        m_modulation.setModulationDepth(std::min(static_cast<float>(m_maxSize - 3), m_modulationDepth));
    }

    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowPass;
    Ap18Smooth m_dispersionFilter;
    // Biquad<BiquadFilterType::AllPass> m_dispersionFilter;
    float m_feedback{0.0f};
    float m_lastValue{0.f};
    size_t m_headRead{0};
    size_t m_headWrite{0};
    size_t m_currentDelayWidth{10};
    size_t m_tick{0};
    Modulation m_modulation;
    float m_modulationDepth;
    size_t m_maxSize;
    std::vector<float> m_buffer{};
};


class FixedAllpassDelay
{
  public:
    FixedAllpassDelay(const float sampleRate, const size_t maxSize)
        : m_buffer(maxSize, 0.f)
        , m_maxSize(maxSize)
        , m_lowPass(sampleRate)
        , m_highPass(sampleRate)
        , m_dispersionFilter(sampleRate)
        , m_delaySteps(maxSize / 8)
    {
        m_highPass.setCutoff(25.f);
    }

    void clear()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void setLowpass(const float value)
    {
        m_lowPass.setCutoff(value);
    }

    void setAllpass(const float value)
    {
        m_dispersionFilter.setCutoff(value);
    }

    void setFeedback(const float gain)
    {
        m_feedback = std::clamp(gain, -1.f, 1.f);
    }

    void setFeedbackNoClamp(const float gain)
    {
        m_feedback = gain;
    }

    void reset()
    {
        m_dispersionFilter.reset();
        m_lowPass.reset();
        std::fill_n(m_buffer.data(), m_delaySteps, 0);
    }

    void setSize(const size_t newSize)
    {
        const auto clampedSize = std::clamp<size_t>(newSize, 2, m_maxSize);
        m_delaySteps = clampedSize;
        m_head = 0;
    }

    float step(const float in)
    {
        m_head = m_head >= m_delaySteps ? 0 : m_head;
        return stepNoIf(in);
    }

    float stepNoIf(const float in)
    {
        m_lastValue = m_buffer[m_head];
        const auto feedDelay = m_highPass.step(in + m_lastValue * m_feedback);
        const auto ret = feedDelay * m_feedback + m_lastValue;
        m_buffer[m_head++] = m_lowPass.step(m_dispersionFilter.step(feedDelay));
        return ret;
    }

    void processBlock(const float* source, float* target, size_t numSamples)
    {
        if (m_head + numSamples >= m_delaySteps)
        {
            std::transform(source, source + numSamples, target, [this](float in) { return step(in); });
        }
        else
        {
            std::transform(source, source + numSamples, target, [this](float in) { return stepNoIf(in); });
        }
    }

    void processBlockInplace(float* inplace, size_t numSamples)
    {
        processBlock(inplace, inplace, numSamples);
    }

    [[nodiscard]] size_t size() const
    {
        return m_delaySteps;
    }

  private:
    std::vector<float> m_buffer;
    size_t m_maxSize;

    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowPass;
    OnePoleFilter<OnePoleFilterCharacteristic::HighPass, true> m_highPass;
    Ap12FixedNoSmooth m_dispersionFilter;
    float m_feedback{0.0f};
    float m_lastValue{0.f};
    size_t m_head{0};
    size_t m_delaySteps;
};

}