#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "../Filters/Sinc/SincFilter.h"

namespace AbacDsp
{
constexpr float SrConvertMaxRatio{128};

/// @ingroup srconverter
/// @brief Push-conversion call state: what was offered, what was taken, what came out.
/// Frames consumed and generated are both reported because a rate change makes them differ.
struct SrPushConverterData
{
    const float* dataIn;
    float* dataOut;
    long inputFrames, outputFrames;
    long inputFramesConsumed, outputFramesGenerated;
    float ratio;
};

/**
 * @ingroup srconverter
 * @brief Push-model sample rate converter: caller supplies input, takes whatever comes out.
 *
 * The caller drives, so this suits a real-time path where input arrives on
 * someone else's schedule and the output count per call is not known in
 * advance. SrPullConverter inverts that: it asks for input when it needs it,
 * which suits a file or offline source.
 *
 * Conversion is windowed-sinc interpolation through a shared SincFilter, so a
 * kernel is built once and used by every converter that references it.
 */
template <size_t MAXCHANNELS>
class SrPushConverter
{
  public:
    explicit SrPushConverter(const std::shared_ptr<SincFilter>& sincFilter)
        : m_bufferSize(static_cast<int>(sincFilter->getBufferSize(SrConvertMaxRatio, MAXCHANNELS)))
        , m_buffer(static_cast<size_t>(m_bufferSize))
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

    [[nodiscard]] size_t fetchBlock(const float currentRatio, const float* in, const size_t numSamples, float* data,
                                    const size_t maxNumSamples) noexcept
    {
        srData.ratio = std::clamp(currentRatio, 1.f / SrConvertMaxRatio, SrConvertMaxRatio);
        srData.dataOut = data;
        srData.outputFrames = static_cast<long>(maxNumSamples);
        srData.dataIn = in;
        srData.inputFrames = static_cast<long>(numSamples);

        process();
        assert(srData.inputFramesConsumed == static_cast<long>(numSamples));
        return static_cast<size_t>(srData.outputFramesGenerated);
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

    bool prepareData(const float* data_in, const int halfFilterChannelWidth, const int numChannels) noexcept
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
        const auto increment = std::lrint(floatIncrement * DStepsFloat);
        const auto startFilterIdx = std::lrint(inputIndex * floatIncrement * DStepsFloat);
        const size_t maxFilterIdx = m_sincFilter->halfCoeffWidth() * static_cast<size_t>(DSteps);

        // The mixed int/long inputs above were always implicitly promoted to size_t here (size_t
        // has equal-or-greater rank than long on every supported platform); the casts below make
        // that existing promotion explicit instead of relying on the compiler to do it silently.
        size_t numCoeff = (maxFilterIdx - static_cast<size_t>(startFilterIdx)) / static_cast<size_t>(increment);
        size_t filterIdx = static_cast<size_t>(startFilterIdx) + numCoeff * static_cast<size_t>(increment);
        float left[CHANNELS]{};
        if (scale == 1.f)
        {
            const auto fraction = static_cast<float>(filterIdx & (DSteps - 1)) / DStepsFloat;
            const auto dataIdx = static_cast<size_t>(m_bufferCurrent) - CHANNELS * numCoeff;
            m_sincFilter->processFixUp<CHANNELS>(numCoeff + 1, m_buffer.data(), dataIdx, fraction, filterIdx / DSteps,
                                                 left);
        }
        else
        {
            m_sincFilter->processFilterHalf<CHANNELS, DSteps, 1, -1>(
                static_cast<int32_t>(filterIdx), m_buffer.data(),
                static_cast<size_t>(m_bufferCurrent) - CHANNELS * numCoeff, static_cast<int32_t>(increment), left);
        }
        filterIdx = static_cast<size_t>(increment - startFilterIdx);
        numCoeff = (maxFilterIdx - filterIdx) / static_cast<size_t>(increment);
        filterIdx = filterIdx + numCoeff * static_cast<size_t>(increment);
        float right[CHANNELS]{};
        if (scale == 1.f)
        {
            const auto fraction = static_cast<float>(filterIdx & (DSteps - 1)) / DStepsFloat;
            const auto dataIdx = static_cast<size_t>(m_bufferCurrent) + CHANNELS * (1 + numCoeff) + (CHANNELS - 1);

            m_sincFilter->processFixDown<CHANNELS>(numCoeff + 1, m_buffer.data(), dataIdx, fraction, filterIdx / DSteps,
                                                   right);
        }
        else
        {
            m_sincFilter->processFilterHalf<CHANNELS, DSteps, -1, 0>(static_cast<int32_t>(filterIdx), m_buffer.data(),
                                                                     static_cast<size_t>(m_bufferCurrent) +
                                                                         CHANNELS * (1 + numCoeff),
                                                                     static_cast<int32_t>(increment), right);
        }

        for (size_t ch = 0; ch < CHANNELS; ++ch)
        {
            output[ch] = scale * (left[ch] + right[ch]);
        }
    }

    bool variProcess(const float targetRatio) noexcept
    {
        m_inCount = srData.inputFrames * static_cast<long>(MAXCHANNELS);
        m_outCount = srData.outputFrames * static_cast<long>(MAXCHANNELS);
        m_usedCount = m_outGenerated = 0;

        auto currentRatio = m_lastRatio;
        auto currentRatioReciprocal = 1.f / m_lastRatio;
        auto halfFilterChannelWidth{0u};
        const auto cnt = (m_sincFilter->halfCoeffWidth() + 2.0f) / m_sincFilter->increment();
        const auto mn = std::min(m_lastRatio, targetRatio);
        const auto count = mn < 1 ? cnt / mn : cnt;

        halfFilterChannelWidth = static_cast<unsigned int>(static_cast<long>(MAXCHANNELS) * (std::lrint(count) + 1));

        auto inputIndex = m_lastPosition;
        auto remainder = std::fmod(inputIndex, 1.f);

        m_bufferCurrent = static_cast<int>(
            (m_bufferCurrent + static_cast<long>(MAXCHANNELS) * std::lrint(inputIndex - remainder)) % m_bufferSize);
        inputIndex = remainder;

        static auto terminate = currentRatioReciprocal + 1e-10f;

        while (m_outGenerated < m_outCount)
        {
            size_t samplesAvailable =
                static_cast<size_t>((m_bufferEnd - m_bufferCurrent + m_bufferSize) % m_bufferSize);
            if (samplesAvailable <= halfFilterChannelWidth)
            {
                if (!prepareData(srData.dataIn, static_cast<int>(halfFilterChannelWidth),
                                 static_cast<int>(MAXCHANNELS)))
                {
                    return false;
                }
                samplesAvailable = static_cast<size_t>((m_bufferEnd - m_bufferCurrent + m_bufferSize) % m_bufferSize);
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
            if (m_outCount > 0 && std::abs(m_lastRatio - targetRatio) > 1e-10)
            {
                currentRatio = m_lastRatio + m_outGenerated * (targetRatio - m_lastRatio) / m_outCount;
                currentRatioReciprocal = 1.f / currentRatio;
            }

            inputIndex += currentRatioReciprocal;

            remainder = std::fmod(inputIndex, 1.0f);
            m_bufferCurrent = static_cast<int>(
                (m_bufferCurrent + static_cast<long>(MAXCHANNELS) * std::lrint(inputIndex - remainder)) % m_bufferSize);
            inputIndex = remainder;
        }
        m_lastPosition = inputIndex;
        m_lastRatio = targetRatio;
        srData.inputFramesConsumed = m_usedCount / static_cast<long>(MAXCHANNELS);
        srData.outputFramesGenerated = m_outGenerated / static_cast<long>(MAXCHANNELS);
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

    void* m_userCbData{nullptr};
    long m_savedFrames{};
    const float* m_savedData{nullptr};

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
