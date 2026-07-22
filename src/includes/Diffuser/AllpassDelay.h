#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Filters/OnePoleFilter.h"
#include "Filters/PoleMixingFilter.h"
#include "Helpers/SkipSmoothing.h"
#include "Modulation/Modulation.h"
#include "Numbers/Convert.h"
#include "Numbers/Interpolation.h"


namespace AbacDsp
{

enum class AllpassFeedbackStyle
{
    Direct,    // feeds the raw delayed sample back into the write path (legacy behavior)
    Schroeder, // classic Schroeder allpass: feeds the filter output back into the write path
};

template <size_t MaxSize48Khz, size_t BlockSize>
class AllPassDelay
{
  public:
    static constexpr size_t minDelaySize{
        51u}; // don't allow delay lines shorter than 51, ( a wall at the distance of 30cm [@ 48kHz] )

    explicit AllPassDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_maxBufferSize(static_cast<size_t>(std::round(static_cast<float>(MaxSize48Khz) * sampleRate / 48000.f)))
        , m_buffer(m_maxBufferSize + 6, 0.f)
    {
    }

    void clear() noexcept
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void newFadeIfNeeded() noexcept
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

    [[nodiscard]] float getDecayTimeInSamples(const float db = -60.f) const noexcept
    {
        const auto f = Convert::dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setFeedback(const float gain) noexcept
    {
        m_feedback = std::clamp(gain, -0.99f, 0.99f);
    }

    void setSize(const size_t newSize) noexcept
    {
        setSizeImpl<false>(newSize);
    }

    void setSize(const size_t newSize, const SkipSmoothing_t&) noexcept
    {
        setSizeImpl<true>(newSize);
    }

    void feedWrite(const float in) noexcept
    {
        m_buffer[m_headWrite] = in;
        if (++m_headWrite >= m_maxBufferSize)
        {
            m_headWrite = 0;
        }
    }

    [[nodiscard]] float step(const float in)
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

    [[nodiscard]] float nextHeadRead(const size_t index) noexcept
    {
        const float returnValue = m_buffer[m_headRead[index]];
        m_headRead[index] = (m_headRead[index] + 1) % m_maxBufferSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target) noexcept
    {
        std::transform(source, source + BlockSize, target, [this](const float in) { return step(in); });
    }

    void processBlockInplace(float* inplace, [[maybe_unused]] size_t numSamples) noexcept
    {
        processBlock(inplace, inplace);
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return m_currentDelayWidth;
    }

  private:
    template <bool fastSetting>
    void setSizeImpl(const size_t newSize) noexcept
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

    const float m_sampleRate;
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

template <size_t MaxSize48Khz, AllpassFeedbackStyle Style = AllpassFeedbackStyle::Direct>
class ModulatingAllPassDelay
{
  public:
    static constexpr float maxFilterFrequency{
        20001.f}; // lowpass only active if frequency under 20001 (20kHz inclusive)
    static constexpr size_t minDelaySize{
        51u}; // don't allow delay lines shorter than 51, ( a wall at the distance of 30cm [@ 48kHz] )
    static constexpr float modulationSafetyMargin{
        8.f}; // headroom kept between the modulated read head and the write head, in samples

    explicit ModulatingAllPassDelay(const float sampleRate)
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

    void clear() noexcept
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void newFadeIfNeeded() noexcept
    {
        if (m_newFadeSize && !m_fadeSteps)
        {
            m_fadeFactorIn = 0.0f;
            m_fadeFactorOut = 1.0f;
            m_fadeSteps = std::min<size_t>(8192, m_currentDelayWidth / 2);
            m_fadeAdvance = 1.0f / static_cast<float>(m_fadeSteps);
            m_headRead[1] = (m_headWrite + m_maxBufferSize - m_newFadeSize) % m_maxBufferSize;
            m_currentDelayWidth = m_newFadeSize;
            trimModulationDepth();
        }
    }

    [[nodiscard]] float getDecayTimeInSamples(const float db = -60.f) const
    {
        const auto f = Convert::dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setLowpass(const float value) noexcept
    {
        m_useLowPass = value < maxFilterFrequency;
        m_lowpass.setCutoff(value);
    }

    void setAllpass(const float value) noexcept
    {
        m_useAllPass = value < maxFilterFrequency;
        m_allpass.setCutoff(value);
    }

    void setFeedback(const float gain) noexcept
    {
        m_feedback = std::clamp(gain, -0.999f, 0.999f);
    }

    void setSize(const size_t newSize) noexcept
    {
        setSizeImpl<false>(newSize);
    }

    void setSize(const size_t newSize, const SkipSmoothing_t&) noexcept
    {
        setSizeImpl<true>(newSize);
    }

    void setModulationDepth(const float depth) noexcept
    {
        m_modulationDepth = depth * 500.f;
        trimModulationDepth();
    }

    void setModulationSpeed(const float speedHz) noexcept
    {
        m_modulation.setModulationSpeed(speedHz);
    }

    void feedWrite(const float in) noexcept
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

        // replicate values at end guard region so interpolation reads past MAXSIZE stay valid
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

    [[nodiscard]] float step(const float in)
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
            return nextHeadRead(0);
        };
        const float delayed = getResult();
        const auto output = -m_feedback * in + delayed;
        if constexpr (Style == AllpassFeedbackStyle::Schroeder)
        {
            feedWrite(in + m_feedback * output);
        }
        else
        {
            feedWrite(in + m_feedback * delayed);
        }
        return output;
    }

    float nextHeadRead(const size_t index)
    {
        auto getReturnValue = [this](const size_t idx)
        {
            if (m_modulation.isModulating())
            {
                const auto [depth, fraction] = m_modulation.lastValuePair();
                auto dHead = m_headRead[idx] + static_cast<size_t>(depth);
                if (dHead >= m_maxBufferSize)
                {
                    dHead -= m_maxBufferSize;
                }
                return Interpolation::hermite43x(&m_buffer[dHead], fraction);
            }
            else
            {
                return m_buffer[m_headRead[idx]];
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
            trimModulationDepth();
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

    // Keeps the modulated read head from ever reaching the write head: the LFO's own peak
    // amplitude is capped to the active delay width (minus a small interpolation margin), not
    // just the total buffer capacity, so a short delay with high depth degrades to a smaller
    // wobble instead of the read head overtaking (or colliding with) the write head.
    void trimModulationDepth() noexcept
    {
        const auto maxSafeDepth = std::max(0.f, static_cast<float>(m_currentDelayWidth) - modulationSafetyMargin);
        m_modulation.setModulationDepth(
            std::min({maxSafeDepth, static_cast<float>(m_maxBufferSize - 3), m_modulationDepth}));
    }

    const float m_sampleRate;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowpass;
    Ap6Smooth m_allpass;

    bool m_useLowPass{true};
    bool m_useAllPass{false};

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
    size_t m_tick{0};
    Modulation m_modulation;
    float m_modulationDepth{0.0f};
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
        m_maxSize = maxSize;
        m_buffer.resize(maxSize + 6);
        m_currentDelayWidth = maxSize / 8;
    }

    void clear() noexcept
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    [[nodiscard]] float getDecayTimeInSamples(const float db = -60.f) const noexcept
    {
        const auto f = Convert::dbToGain(db);
        return std::log10(f) * static_cast<float>(m_currentDelayWidth) / std::log10(m_feedback);
    }

    void setLowpass(const float value) noexcept
    {
        m_lowPass.setCutoff(value);
    }

    void setAllpass(const float value) noexcept
    {
        m_dispersionFilter.setCutoff(value);
    }

    void setFeedback(const float gain) noexcept
    {
        m_feedback = std::clamp(gain, -1.f, 1.f);
    }

    void setFeedbackNoClamp(const float gain) noexcept
    {
        m_feedback = gain;
    }

    void reset() noexcept
    {
        m_dispersionFilter.reset();
        m_lowPass.reset();
        std::fill_n(m_buffer.data(), m_buffer.size(), 0);
    }

    void setSize(const size_t newSize) noexcept
    {
        const auto clampedSize = std::clamp<size_t>(newSize, 2, m_maxSize);
        m_currentDelayWidth = clampedSize;
        m_headRead = (m_headWrite + m_maxSize - m_currentDelayWidth) % m_maxSize;
    }

    void setModulationDepth(const float depth) noexcept
    {
        m_modulationDepth = depth * 100.f;
        trimModulationDepth();
    }

    void setModulationSpeed(const float speedHz) noexcept
    {
        m_modulation.setModulationSpeed(speedHz);
    }

    void feedWrite(const float in) noexcept
    {
        m_buffer[m_headWrite] = m_lowPass.step(in);
        m_buffer[m_headWrite] = m_dispersionFilter.step(m_buffer[m_headWrite]);

        // replicate values at end guard region so interpolation reads past MAXSIZE stay valid
        if (m_headWrite < 6)
        {
            const size_t padIndex = m_headWrite + m_maxSize;
            m_buffer[padIndex] = m_buffer[m_headWrite];
        }

        if (++m_headWrite >= m_maxSize)
        {
            m_headWrite = 0;
        }
    }

    [[nodiscard]] float step(const float in)
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
        const auto output = -m_feedback * in + delayed;
        const auto toWrite = in + m_feedback * delayed;
        feedWrite(toWrite);
        return output;
    }

    [[nodiscard]] float nextHeadRead()
    {
        float returnValue{0.0f};
        if (m_modulation.isModulating())
        {
            const auto [depth, fraction] = m_modulation.lastValuePair();
            auto dHead = m_headRead + static_cast<size_t>(depth);
            if (dHead >= m_maxSize)
            {
                dHead -= m_maxSize;
            }
            returnValue = Interpolation::linearPt2(&m_buffer[dHead], fraction);
        }
        else
        {
            returnValue = m_buffer[m_headRead];
        }
        m_headRead = (m_headRead + 1) % m_maxSize;
        return returnValue;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](const float in) { return step(in); });
    }

    void processBlockInplace(float* inplace, const size_t numSamples)
    {
        std::transform(inplace, inplace + numSamples, inplace, [this](const float in) { return step(in); });
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return m_currentDelayWidth;
    }

  private:
    const float m_sampleRate;

    void trimModulationDepth() noexcept
    {
        m_modulation.setModulationDepth(std::min(static_cast<float>(m_maxSize - 3), m_modulationDepth));
    }

    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowPass;
    Ap18Smooth m_dispersionFilter;
    float m_feedback{0.0f};
    size_t m_headRead{0};
    size_t m_headWrite{0};
    size_t m_currentDelayWidth{10};
    size_t m_tick{0};
    Modulation m_modulation;
    float m_modulationDepth{0.0f};
    size_t m_maxSize{0};
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

    void clear() noexcept
    {
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    }

    void setLowpass(const float value) noexcept
    {
        m_lowPass.setCutoff(value);
    }

    void setAllpass(const float value) noexcept
    {
        m_dispersionFilter.setCutoff(value);
    }

    void setFeedback(const float gain) noexcept
    {
        m_feedback = std::clamp(gain, -1.f, 1.f);
    }

    void setFeedbackNoClamp(const float gain) noexcept
    {
        m_feedback = gain;
    }

    void reset() noexcept
    {
        m_dispersionFilter.reset();
        m_lowPass.reset();
        std::fill_n(m_buffer.data(), m_delaySteps, 0);
    }

    void setSize(const size_t newSize) noexcept
    {
        const auto clampedSize = std::clamp<size_t>(newSize, 2, m_maxSize);
        m_delaySteps = clampedSize;
        m_head = 0;
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        m_head = m_head >= m_delaySteps ? 0 : m_head;
        return stepNoIf(in);
    }

    [[nodiscard]] float stepNoIf(const float in) noexcept
    {
        m_lastValue = m_buffer[m_head];
        const auto feedDelay = m_highPass.step(in + m_lastValue * m_feedback);
        const auto ret = feedDelay * m_feedback + m_lastValue;
        m_buffer[m_head++] = m_lowPass.step(m_dispersionFilter.singleStep(feedDelay));
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

    void processBlockInplace(float* inplace, const size_t numSamples) noexcept
    {
        processBlock(inplace, inplace, numSamples);
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return m_delaySteps;
    }

  private:
    std::vector<float> m_buffer;
    size_t m_maxSize{0};

    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowPass;
    OnePoleFilter<OnePoleFilterCharacteristic::HighPass, true> m_highPass;
    FourStageOnePoleFilterNoResonance<1, -4, 4, 0, 0> m_dispersionFilter;
    float m_feedback{0.0f};
    float m_lastValue{0.f};
    size_t m_head{0};
    size_t m_delaySteps;
};

}