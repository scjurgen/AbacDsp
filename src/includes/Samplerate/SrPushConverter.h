#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <ranges>
#include <vector>

#include "SincFilter.h"

namespace AbacDsp
{
constexpr int SrConvertMaxRatio = 16;

template <size_t NumChannels>
    requires(NumChannels > 0)
class SrPushConverter
{
    struct SrConverterData
    {
        const float* dataIn{};
        float* dataOut{};
        long inputFrames{};
        long outputFrames{};
        long inputFramesConsumed{};
        long outputFramesGenerated{};
        bool endOfInput{};
        float ratio{};
    };

    struct BlockState
    {
        const float* savedData{};
        long savedFrames{};
        float lastPosition{};
        long inCount{};
        long usedCount{};
        long outCount{};
        long outGenerated{};

        void reset()
        {
            savedData = nullptr;
            savedFrames = 0;
            lastPosition = 0.0f;
            inCount = 0;
            usedCount = 0;
            outCount = 0;
            outGenerated = 0;
        }
    };

    struct CircularBufferState
    {
        int current{};
        int end{};
        int realEnd{-1};
        int size{};

        void reset()
        {
            current = 0;
            end = 0;
            realEnd = -1;
        }

        [[nodiscard]] int samplesAvailable() const noexcept
        {
            return (end - current + size) % size;
        }

        void advanceCurrent(int samples) noexcept
        {
            current = (current + samples) % size;
        }

        bool atOrPastRealEnd(float inputIndex, float terminate) const noexcept
        {
            return (realEnd >= 0) && (current + inputIndex + terminate >= realEnd);
        }

        // Prepares room for new samples and keeps buffer positions valid.
        // Returns max number of samples that can be added without overflowing.
        int ensureSpaceFor(int halfFilterChannelWidth, int numChannels, long inCount)
        {
            if (current == 0)
            {
                current = end = halfFilterChannelWidth;
                return size - 2 * halfFilterChannelWidth;
            }
            else if (end + halfFilterChannelWidth + numChannels + inCount < size)
            {
                return std::max(size - current - halfFilterChannelWidth, 0);
            }
            else
            {
                // Wrap buffer: keep necessary tail data at start
                const int len = end - current;
                // The actual copy will be done by the caller
                current = halfFilterChannelWidth;
                end = current + len;
                return std::max(size - current - halfFilterChannelWidth, 0);
            }
        }
    };


  public:
    explicit SrPushConverter(const std::shared_ptr<SincFilter>& sincFilter)
        : m_circBufState{0, 0, -1, sincFilter->getBufferSize(SrConvertMaxRatio, NumChannels)}
        , m_buffer(m_circBufState.size)
        , m_sincFilter(sincFilter)
    {
        reset();
    }

    void reset()
    {
        m_blockState.reset();
        m_circBufState.reset();
        std::ranges::fill(m_buffer, 0.0f);
    }

    size_t fetchBlock(float currentRatio, const float* in, size_t numSamples, float* target, size_t maxSizeTarget)
    {
        if ((currentRatio > SrConvertMaxRatio) || (currentRatio < 1.0f / SrConvertMaxRatio))
        {
            assert(false);
        }

        size_t output_frames_gen = 0;
        srData.ratio = currentRatio;
        srData.dataOut = target;
        srData.outputFrames = maxSizeTarget;

        if (m_blockState.savedFrames)
        {
            srData.dataIn = m_blockState.savedData;
            srData.inputFrames = m_blockState.savedFrames;
            process();
            srData.dataOut += srData.outputFramesGenerated * NumChannels;
            srData.outputFrames -= srData.outputFramesGenerated;
            output_frames_gen += srData.outputFramesGenerated;
        }

        srData.dataIn = in;
        srData.inputFrames = numSamples;
        process();
        srData.dataIn += srData.inputFramesConsumed * NumChannels;
        srData.inputFrames -= srData.inputFramesConsumed;
        srData.dataOut += srData.outputFramesGenerated * NumChannels;
        srData.outputFrames -= srData.outputFramesGenerated;
        output_frames_gen += srData.outputFramesGenerated;

        m_blockState.savedData = srData.dataIn;
        m_blockState.savedFrames = srData.inputFrames;
        return output_frames_gen;
    }

  private:
    bool process()
    {
        srData.inputFramesConsumed = 0;
        srData.outputFramesGenerated = 0;
        return variProcess(srData.ratio);
    }

    bool prepareData(const float* data_in, bool end_of_input, int halfFilterChannelWidth)
    {
        if (m_circBufState.realEnd >= 0)
        {
            return true;
        }

        int len = m_circBufState.ensureSpaceFor(halfFilterChannelWidth, NumChannels, m_blockState.inCount);

        len = std::min(static_cast<int>(m_blockState.inCount - m_blockState.usedCount), len);
        len -= len % NumChannels;

        if ((len < 0) || (m_circBufState.end + len > m_circBufState.size))
        {
            return false;
        }

        std::copy_n(data_in + m_blockState.usedCount, len, m_buffer.begin() + m_circBufState.end);
        m_circBufState.end += len;
        m_blockState.usedCount += len;

        if ((m_blockState.usedCount == m_blockState.inCount) &&
            (m_circBufState.samplesAvailable() < 2 * halfFilterChannelWidth) && end_of_input)
        {
            if (m_circBufState.size - m_circBufState.end < halfFilterChannelWidth + 12)
            {
                const int remainLen = m_circBufState.end - m_circBufState.current;
                std::copy(m_buffer.begin() + (m_circBufState.current - halfFilterChannelWidth),
                          m_buffer.begin() + (m_circBufState.current - halfFilterChannelWidth) +
                              (halfFilterChannelWidth + remainLen),
                          m_buffer.begin());
                m_circBufState.current = halfFilterChannelWidth;
                m_circBufState.end = m_circBufState.current + remainLen;
            }

            m_circBufState.realEnd = m_circBufState.end;

            int padLen = halfFilterChannelWidth + 12;
            if ((padLen < 0) || (m_circBufState.end + padLen > m_circBufState.size))
            {
                padLen = m_circBufState.size - m_circBufState.end;
            }
            std::fill_n(m_buffer.begin() + m_circBufState.end, padLen, 0.0f);
            m_circBufState.end += padLen;
        }
        return true;
    }


    static constexpr auto DSteps = 4096;
    static constexpr auto DStepsFloat = static_cast<float>(DSteps);

    void calcSincOutput(const float floatIncrement, const float inputIndex, const float scale, float* output)
    {
        const auto increment = lrint(floatIncrement * DStepsFloat);
        const auto startFilterIdx = lrint(inputIndex * floatIncrement * DStepsFloat);
        const auto maxFilterIdx = m_sincFilter->halfCoeffWidth() * DSteps;

        auto numCoeff = (maxFilterIdx - startFilterIdx) / increment;
        auto filterIdx = startFilterIdx + numCoeff * increment;
        float left[NumChannels]{};
        if (scale == 1.0f)
        {
            const auto fraction = static_cast<float>(filterIdx & (DSteps - 1)) / DStepsFloat;
            const auto dataIdx = m_circBufState.current - NumChannels * numCoeff;
            m_sincFilter->processFixUp<NumChannels>(numCoeff + 1, &m_buffer[dataIdx], fraction, filterIdx / DSteps,
                                                    left);
        }
        else
        {
            m_sincFilter->processFilterHalf<NumChannels, DSteps, 1, -1>(
                filterIdx, &m_buffer[m_circBufState.current - NumChannels * numCoeff], increment, left);
        }

        filterIdx = increment - startFilterIdx;
        numCoeff = (maxFilterIdx - filterIdx) / increment;
        filterIdx = filterIdx + numCoeff * increment;
        float right[NumChannels]{};
        if (scale == 1.0f)
        {
            const auto fraction = static_cast<float>(filterIdx & (DSteps - 1)) / DStepsFloat;
            const auto dataIdx = m_circBufState.current + NumChannels * (1 + numCoeff) + (NumChannels - 1);
            m_sincFilter->processFixDown<NumChannels>(numCoeff + 1, &m_buffer[dataIdx], fraction, filterIdx / DSteps,
                                                      right);
        }
        else
        {
            m_sincFilter->processFilterHalf<NumChannels, DSteps, -1, 0>(
                filterIdx, &m_buffer[m_circBufState.current + NumChannels * (1 + numCoeff)], increment, right);
        }

        for (int ch = 0; ch < NumChannels; ch++)
        {
            output[ch] = scale * (left[ch] + right[ch]);
        }
    }

    bool variProcess(const float targetRatio)
    {
        m_blockState.inCount = srData.inputFrames * NumChannels;
        m_blockState.outCount = srData.outputFrames * NumChannels;
        m_blockState.usedCount = 0;
        m_blockState.outGenerated = 0;

        auto currentRatio = targetRatio;
        auto currentRatioReciprocal = 1.0f / targetRatio;
        const auto cnt = (m_sincFilter->halfCoeffWidth() + 2.0f) / m_sincFilter->increment();
        const auto mn = std::min(currentRatio, targetRatio);
        const auto count = (mn < 1.0f) ? (cnt / mn) : cnt;
        auto halfFilterChannelWidth = NumChannels * (lrint(count) + 1);

        auto inputIndex = m_blockState.lastPosition;
        auto remainder = std::fmod(inputIndex, 1.0f);
        m_circBufState.advanceCurrent(NumChannels * lrint(inputIndex - remainder));
        inputIndex = remainder;

        static auto terminate = currentRatioReciprocal + 1e-10f;

        while (m_blockState.outGenerated < m_blockState.outCount)
        {
            if (m_circBufState.samplesAvailable() <= halfFilterChannelWidth)
            {
                if (!prepareData(srData.dataIn, srData.endOfInput, halfFilterChannelWidth))
                {
                    return false;
                }
                if (m_circBufState.samplesAvailable() <= halfFilterChannelWidth)
                {
                    break;
                }
            }
            if (m_circBufState.atOrPastRealEnd(inputIndex, terminate))
            {
                break;
            }

            const auto floatIncrement = m_sincFilter->increment() * ((currentRatio < 1.0f) ? currentRatio : 1.0f);
            calcSincOutput(floatIncrement, inputIndex, floatIncrement / m_sincFilter->increment(),
                           srData.dataOut + m_blockState.outGenerated);

            m_blockState.outGenerated += NumChannels;
            inputIndex += currentRatioReciprocal;
            remainder = std::fmod(inputIndex, 1.0f);
            m_circBufState.advanceCurrent(NumChannels * lrint(inputIndex - remainder));
            inputIndex = remainder;
        }

        m_blockState.lastPosition = inputIndex;
        srData.inputFramesConsumed = m_blockState.usedCount / NumChannels;
        srData.outputFramesGenerated = m_blockState.outGenerated / NumChannels;
        return true;
    }

    BlockState m_blockState;
    CircularBufferState m_circBufState;
    SrConverterData srData;
    std::vector<float> m_buffer;
    std::shared_ptr<SincFilter> m_sincFilter;
};

} // namespace AbacDsp
