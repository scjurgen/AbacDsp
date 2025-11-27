#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "SincFilter.h"

namespace AbacDsp
{
constexpr float SrConvertMaxRatio{128};

struct SrPushConverterData
{
    const float* dataIn;
    float* dataOut;
    long inputFrames, outputFrames;
    long inputFramesConsumed, outputFramesGenerated;
    float ratio;
};

template <size_t MAXCHANNELS>
class SrPushConverter
{
  public:
    explicit SrPushConverter(const std::shared_ptr<SincFilter>& sincFilter)
        : m_bufferSize(sincFilter->getBufferSize(SrConvertMaxRatio, MAXCHANNELS))
        , m_buffer(m_bufferSize)
        , m_sincFilter(sincFilter)
    {
        reset();
    }

    void reset()
    {
        m_savedData = nullptr;
        m_savedFrames = 0;
        m_bufferCurrent = m_bufferEnd = 0;
        m_bufferRealEnd = -1;
        m_lastPosition = 0.0;
        m_lastRatio = 0.0;
        std::ranges::fill(m_buffer, 0);
    }

    size_t fetchBlock(const float currentRatio, const float* in, const size_t numSamples, float* data,
                      const size_t maxNumSamples) noexcept
    {
        srData.ratio = std::clamp(currentRatio, 1.f / SrConvertMaxRatio, SrConvertMaxRatio);
        srData.dataOut = data;
        srData.outputFrames = maxNumSamples;
        srData.dataIn = in;
        srData.inputFrames = numSamples;

        process();
        assert(srData.inputFramesConsumed == numSamples);
        return srData.outputFramesGenerated;
    }

  private:
    bool process() noexcept
    {
        srData.inputFramesConsumed = 0;
        srData.outputFramesGenerated = 0;

        if (getLastRatio() < 1.f / SrConvertMaxRatio)
        {
            setLastRatio(srData.ratio);
        }
        return variProcess(srData.ratio);
    }

    bool prepareData(const float* data_in, const int halfFilterChannelWidth, const size_t numChannels) noexcept
    {
        int len = 0;

        if (m_bufferRealEnd >= 0)
        {
            return true;
        }

        if (m_bufferCurrent == 0)
        {
            len = m_bufferSize - 2 * halfFilterChannelWidth;
            m_bufferCurrent = m_bufferEnd = halfFilterChannelWidth;
        }
        else if (m_bufferEnd + halfFilterChannelWidth + numChannels + m_inCount < m_bufferSize)
        {
            len = std::max(m_bufferSize - m_bufferCurrent - halfFilterChannelWidth, 0);
        }
        else
        {
            len = m_bufferEnd - m_bufferCurrent;
            std::copy(m_buffer.begin() + (m_bufferCurrent - halfFilterChannelWidth),
                      m_buffer.begin() + (m_bufferCurrent - halfFilterChannelWidth) + (halfFilterChannelWidth + len),
                      m_buffer.begin());
            m_bufferCurrent = halfFilterChannelWidth;
            m_bufferEnd = m_bufferCurrent + len;

            len = std::max(m_bufferSize - m_bufferCurrent - halfFilterChannelWidth, 0);
        }

        len = std::min(static_cast<int>(m_inCount - m_usedCount), len);
        len -= len % numChannels;

        if (len < 0 || m_bufferEnd + len > m_bufferSize)
        {
            return false;
        }

        std::copy_n(data_in + m_usedCount, len, m_buffer.begin() + m_bufferEnd);
        m_bufferEnd += len;
        m_usedCount += len;

        return true;
    }

    static constexpr auto DSteps = 4096;
    static constexpr auto DStepsFloat = static_cast<float>(DSteps);

    template <size_t CHANNELS>
    void calcSincOutput(const float floatIncrement, const float inputIndex, const float scale, float* output) noexcept
    {
        const auto increment = lrint(floatIncrement * DStepsFloat);
        const auto startFilterIdx = lrint(inputIndex * floatIncrement * DStepsFloat);
        const auto maxFilterIdx = m_sincFilter->halfCoeffWidth() * DSteps;

        // Process left half
        auto numCoeff = (maxFilterIdx - startFilterIdx) / increment;
        auto filterIdx = startFilterIdx + numCoeff * increment;
        float left[CHANNELS]{};
        if (scale == 1.f)
        {
            const auto fraction = static_cast<float>(filterIdx & (DSteps - 1)) / DStepsFloat;
            const auto dataIdx = m_bufferCurrent - CHANNELS * numCoeff;
            m_sincFilter->processFixUp<CHANNELS>(numCoeff + 1, m_buffer.data(), dataIdx, fraction, filterIdx / DSteps,
                                                 left);
        }
        else
        {
            m_sincFilter->processFilterHalf<CHANNELS, DSteps, 1, -1>(
                filterIdx, m_buffer.data(), m_bufferCurrent - CHANNELS * numCoeff, increment, left);
        }
 filterIdx = increment - startFilterIdx;
        numCoeff = (maxFilterIdx - filterIdx) / increment;
        filterIdx = filterIdx + numCoeff * increment;
        float right[CHANNELS]{};
        if (scale == 1.f)
        {
            const auto fraction = static_cast<float>(filterIdx & (DSteps - 1)) / DStepsFloat;
            const auto dataIdx = m_bufferCurrent + CHANNELS * (1 + numCoeff) + (CHANNELS - 1);

            m_sincFilter->processFixDown<CHANNELS>(numCoeff + 1, m_buffer.data(), dataIdx, fraction, filterIdx / DSteps,
                                                   right);
        }
        else
        {
            m_sincFilter->processFilterHalf<CHANNELS, DSteps, -1, 0>(
                filterIdx, m_buffer.data(), m_bufferCurrent + CHANNELS * (1 + numCoeff), increment, right);
        }

        for (int ch = 0; ch < CHANNELS; ch++)
        {
            output[ch] = scale * (left[ch] + right[ch]);
        }
    }

    bool variProcess(const float targetRatio) noexcept
    {
        m_inCount = srData.inputFrames * MAXCHANNELS;
        m_outCount = srData.outputFrames * MAXCHANNELS;
        m_usedCount = m_outGenerated = 0;

        auto currentRatio = m_lastRatio;
        auto currentRatioReciprocal = 1.f / m_lastRatio;
        auto halfFilterChannelWidth{0u};
        const auto cnt = (m_sincFilter->halfCoeffWidth() + 2.0f) / m_sincFilter->increment();
        const auto mn = std::min(m_lastRatio, targetRatio);
        const auto count = mn < 1 ? cnt / mn : cnt;

        halfFilterChannelWidth = MAXCHANNELS * (lrint(count) + 1);

        auto inputIndex = m_lastPosition;
        auto remainder = std::fmod(inputIndex, 1.f);

        m_bufferCurrent = (m_bufferCurrent + MAXCHANNELS * lrint(inputIndex - remainder)) % m_bufferSize;
        inputIndex = remainder;

        static auto terminate = currentRatioReciprocal + 1e-10f;

        while (m_outGenerated < m_outCount)
        {
            size_t samplesAvailable = (m_bufferEnd - m_bufferCurrent + m_bufferSize) % m_bufferSize;
            if (samplesAvailable <= halfFilterChannelWidth)
            {
                if (!prepareData(srData.dataIn, halfFilterChannelWidth, MAXCHANNELS))
                {
                    return false;
                }
                samplesAvailable = (m_bufferEnd - m_bufferCurrent + m_bufferSize) % m_bufferSize;
                if (samplesAvailable <= halfFilterChannelWidth)
                {
                    break;
                }
            }
            if (m_bufferRealEnd >= 0)
            {
                if (m_bufferCurrent + inputIndex + terminate >= m_bufferRealEnd)
                {
                    break;
                }
            }

            const auto floatIncrement = m_sincFilter->increment() * (currentRatio < 1.0f ? currentRatio : 1.0f);
            calcSincOutput<MAXCHANNELS>(floatIncrement, inputIndex, floatIncrement / m_sincFilter->increment(),
                                        srData.dataOut + m_outGenerated);


            m_outGenerated += MAXCHANNELS;
            if (m_outCount > 0 && fabs(m_lastRatio - targetRatio) > 1e-10)
            {
                currentRatio = m_lastRatio + m_outGenerated * (targetRatio - m_lastRatio) / m_outCount;
                currentRatioReciprocal = 1.f / currentRatio;
            }

            inputIndex += currentRatioReciprocal;

            remainder = std::fmod(inputIndex, 1.0f);
            m_bufferCurrent = (m_bufferCurrent + MAXCHANNELS * lrint(inputIndex - remainder)) % m_bufferSize;
            inputIndex = remainder;
        }
        m_lastPosition = inputIndex;
        m_lastRatio = targetRatio;
        srData.inputFramesConsumed = m_usedCount / MAXCHANNELS;
        srData.outputFramesGenerated = m_outGenerated / MAXCHANNELS;
        return true;
    }

    [[nodiscard]] float getLastRatio() const noexcept
    {
        return m_lastRatio;
    }

    void setLastRatio(const float lastRatio) noexcept
    {
        m_lastRatio = lastRatio;
    }

    void* m_userCbData;
    long m_savedFrames;
    const float* m_savedData;

    SrPushConverterData srData;

    float m_lastRatio{};
    float m_lastPosition{};
    long m_inCount{}, m_usedCount{};
    long m_outCount{}, m_outGenerated{};
    int m_bufferCurrent{};
    int m_bufferEnd{};
    int m_bufferRealEnd{};
    int m_bufferSize;
    std::vector<float> m_buffer;
    std::shared_ptr<SincFilter> m_sincFilter;
};

}
