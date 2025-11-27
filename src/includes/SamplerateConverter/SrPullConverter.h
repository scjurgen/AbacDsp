#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>

#include "SincFilter.h"

namespace AbacDsp
{
constexpr int SrPullConvertMaxRatio = 16;

struct SrConverterData
{
    const float* dataIn;
    float* dataOut;
    long inputFrames, outputFrames;
    long inputFramesConsumed, outputFramesGenerated;
    bool endOfInput;
    float ratio;
};

class SrPullConverter
{
    static constexpr size_t MAXCHANNELS = 2;
    using ProcessCallback = std::function<long(float**, size_t numChannels)>;
    using DataCallback = std::function<long(void*, float**)>;

  public:
    explicit SrPullConverter(const std::shared_ptr<const SincFilter>& sincFilter)
        : m_bufferSize(sincFilter->getBufferSize(SrPullConvertMaxRatio, MAXCHANNELS))
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

    size_t fetchBlock(const float currentRatio, float* data, const size_t numSamples, const size_t numChannels,
                      ProcessCallback& cb)
    {
        assert(currentRatio <= SrPullConvertMaxRatio || currentRatio >= 1.f / SrPullConvertMaxRatio);
        srData.ratio = currentRatio;
        srData.dataOut = data;
        srData.outputFrames = numSamples;
        srData.dataIn = m_savedData;
        srData.inputFrames = m_savedFrames;

        size_t output_frames_gen = 0;
        while (output_frames_gen < numSamples)
        {
            if (srData.inputFrames == 0)
            {
                float* ptr = nullptr;
                srData.inputFrames = cb(&ptr, numChannels);
                srData.dataIn = ptr;
                srData.endOfInput = (srData.inputFrames == 0);
            }

            process(numChannels);

            srData.dataIn += srData.inputFramesConsumed * numChannels;
            srData.inputFrames -= srData.inputFramesConsumed;

            srData.dataOut += srData.outputFramesGenerated * numChannels;
            srData.outputFrames -= srData.outputFramesGenerated;

            output_frames_gen += srData.outputFramesGenerated;

            if (srData.endOfInput && srData.outputFramesGenerated == 0)
            {
                break;
            }
        }
        m_savedData = srData.dataIn;
        m_savedFrames = srData.inputFrames;
        return output_frames_gen;
    }

    size_t fetchBlock(float currentRatio, float* data, size_t numSamples, size_t numChannels)
    {
        if (currentRatio > SrPullConvertMaxRatio || currentRatio < 1.f / SrPullConvertMaxRatio)
        {
            assert(false);
        }
        srData.ratio = currentRatio;
        srData.dataOut = data;
        srData.outputFrames = numSamples;
        srData.dataIn = m_savedData;
        srData.inputFrames = m_savedFrames;

        size_t output_frames_gen = 0;
        while (output_frames_gen < numSamples)
        {
            if (srData.inputFrames == 0)
            {
                float* ptr = nullptr;
                srData.inputFrames = m_cb(m_userCbData, &ptr);
                srData.dataIn = ptr;
                srData.endOfInput = srData.inputFrames == 0;
            }
            if (srData.endOfInput)
            {
                break;
            }
            process(numChannels);

            srData.dataIn += srData.inputFramesConsumed * numChannels;
            srData.inputFrames -= srData.inputFramesConsumed;

            srData.dataOut += srData.outputFramesGenerated * numChannels;
            srData.outputFrames -= srData.outputFramesGenerated;

            output_frames_gen += srData.outputFramesGenerated;

            if (srData.endOfInput && srData.outputFramesGenerated == 0)
            {
                break;
            }
        }
        m_savedData = srData.dataIn;
        m_savedFrames = srData.inputFrames;
        return output_frames_gen;
    }

    void setCallback(std::function<long(void*, float**)> cb, void* userData)
    {
        m_cb = std::move(cb);
        m_userCbData = userData;
    }

  private:
    bool process(size_t numChannels)
    {
        srData.inputFramesConsumed = 0;
        srData.outputFramesGenerated = 0;

        if (getLastRatio() < 1.f / SrPullConvertMaxRatio)
        {
            setLastRatio(srData.ratio);
        }
        return variProcess(srData.ratio, numChannels);
    }

    bool prepareData(const float* data_in, bool end_of_input, int halfFilterChannelWidth, size_t numChannels)
    {
        int len = 0;

        if (m_bufferRealEnd >= 0)
        {
            return true;
        }

        if (m_bufferCurrent == 0)
        {
            // Initial state. Set up zeros at the start of the m_buffer and then load new data after that.
            len = m_bufferSize - 2 * halfFilterChannelWidth;
            m_bufferCurrent = m_bufferEnd = halfFilterChannelWidth;
        }
        else if (m_bufferEnd + halfFilterChannelWidth + numChannels + m_inCount < m_bufferSize)
        {
            //  Load data at current end position
            len = std::max(m_bufferSize - m_bufferCurrent - halfFilterChannelWidth, 0);
        }
        else
        {
            // Move data at end of m_buffer back to the start of m_buffer
            len = m_bufferEnd - m_bufferCurrent;
            std::copy(m_buffer.begin() + (m_bufferCurrent - halfFilterChannelWidth),
                      m_buffer.begin() + (m_bufferCurrent - halfFilterChannelWidth) + (halfFilterChannelWidth + len),
                      m_buffer.begin());
            m_bufferCurrent = halfFilterChannelWidth;
            m_bufferEnd = m_bufferCurrent + len;

            // Now load data at current end of m_buffer.
            len = std::max(m_bufferSize - m_bufferCurrent - halfFilterChannelWidth, 0);
        }

        len = std::min((int) (m_inCount - m_usedCount), len);
        len -= len % numChannels;

        if (len < 0 || m_bufferEnd + len > m_bufferSize)
        {
            return false;
        }

        std::copy_n(data_in + m_usedCount, len, m_buffer.begin() + m_bufferEnd);
        m_bufferEnd += len;
        m_usedCount += len;

        if (m_usedCount == m_inCount && m_bufferEnd - m_bufferCurrent < 2 * halfFilterChannelWidth && end_of_input)
        {
            // current m_buffer has been consumed and this is the last m_buffer.
            if (m_bufferSize - m_bufferEnd < halfFilterChannelWidth + 12)
            {
                // If necessary, move data down to the start of the m_buffer
                len = m_bufferEnd - m_bufferCurrent;
                std::copy(m_buffer.begin() + (m_bufferCurrent - halfFilterChannelWidth),
                          m_buffer.begin() + (m_bufferCurrent - halfFilterChannelWidth) +
                              (halfFilterChannelWidth + len),
                          m_buffer.begin());
                m_bufferCurrent = halfFilterChannelWidth;
                m_bufferEnd = m_bufferCurrent + len;
            }

            m_bufferRealEnd = m_bufferEnd;
            len = halfFilterChannelWidth + 12;

            if (len < 0 || m_bufferEnd + len > m_bufferSize)
            {
                len = m_bufferSize - m_bufferEnd;
            }

            std::fill_n(m_buffer.begin() + m_bufferEnd, len, 0);
            m_bufferEnd += len;
        }

        return true;
    }

    static constexpr auto DSteps = 4096;
    static constexpr auto DStepsFloat = static_cast<float>(DSteps);

    template <size_t CHANNELS>
    void calcSincOutput(const float floatIncrement, const float inputIndex, const float scale, float* output)
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

        // Process right half
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

    bool variProcess(const float targetRatio, const size_t numChannels)
    {
        m_inCount = srData.inputFrames * numChannels;
        m_outCount = srData.outputFrames * numChannels;
        m_usedCount = m_outGenerated = 0;

        auto currentRatio = m_lastRatio;
        auto currentRatioReciprocal = 1.f / m_lastRatio;
        auto halfFilterChannelWidth{0u};
        // Check the sample rate ratio wrt the m_buffer len.
        const auto cnt = (m_sincFilter->halfCoeffWidth() + 2.0f) / m_sincFilter->increment();
        const auto mn = std::min(m_lastRatio, targetRatio);
        const auto count = mn < 1 ? cnt / mn : cnt;

        // Maximum coefficients on either side of center point.
        halfFilterChannelWidth = numChannels * (lrint(count) + 1);

        auto inputIndex = m_lastPosition;
        auto remainder = std::fmod(inputIndex, 1.f);

        m_bufferCurrent = (m_bufferCurrent + numChannels * lrint(inputIndex - remainder)) % m_bufferSize;
        inputIndex = remainder;

        static auto terminate = currentRatioReciprocal + 1e-10f;

        // produce
        while (m_outGenerated < m_outCount)
        {
            size_t samplesAvailable = (m_bufferEnd - m_bufferCurrent + m_bufferSize) % m_bufferSize;
            if (samplesAvailable <= halfFilterChannelWidth)
            {
                auto ok = prepareData(srData.dataIn, srData.endOfInput, halfFilterChannelWidth, numChannels);
                if (!ok)
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
            if (numChannels == 2)
            {
                calcSincOutput<2>(floatIncrement, inputIndex, floatIncrement / m_sincFilter->increment(),
                                  srData.dataOut + m_outGenerated);
            }
            else
            {
                calcSincOutput<1>(floatIncrement, inputIndex, floatIncrement / m_sincFilter->increment(),
                                  srData.dataOut + m_outGenerated);
            }

            m_outGenerated += numChannels;
            if (m_outCount > 0 && fabs(m_lastRatio - targetRatio) > 1e-10)
            {
                // this needs a thorough check, looks like we need to preestimate the number of samples to be generated
                // and substitute with m_outCount generated
                // also it should only be adapted when we have generated a sample?
                //
                currentRatio = m_lastRatio + m_outGenerated * (targetRatio - m_lastRatio) / m_outCount;
                currentRatioReciprocal = 1.f / currentRatio;
            }

            inputIndex += currentRatioReciprocal;

            remainder = std::fmod(inputIndex, 1.0f);
            m_bufferCurrent = (m_bufferCurrent + numChannels * lrint(inputIndex - remainder)) % m_bufferSize;
            inputIndex = remainder;
        }
        m_lastPosition = inputIndex;
        m_lastRatio = targetRatio;
        // m_lastRatio = currentRatio;
        srData.inputFramesConsumed = m_usedCount / numChannels;
        srData.outputFramesGenerated = m_outGenerated / numChannels;
        return true;
    }

    [[nodiscard]] float getLastRatio() const
    {
        return m_lastRatio;
    }

    void setLastRatio(float lastRatio)
    {
        m_lastRatio = lastRatio;
    }

    DataCallback m_cb;
    void* m_userCbData;
    long m_savedFrames;
    const float* m_savedData;


    SrConverterData srData;

    float m_lastRatio{};
    float m_lastPosition{};
    long m_inCount{}, m_usedCount{};
    long m_outCount{}, m_outGenerated{};
    int m_bufferCurrent{};
    int m_bufferEnd{};
    int m_bufferRealEnd{};
    int m_bufferSize;
    std::vector<float> m_buffer;
    std::shared_ptr<const SincFilter> m_sincFilter;
};

}