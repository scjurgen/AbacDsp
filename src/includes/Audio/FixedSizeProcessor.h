#pragma once

#include <cassert>
#include <cmath>
#include <functional>

#include "AudioBuffer.h"

namespace AbacDsp
{

inline size_t getSamplesPerMillisecond(float milliseconds, float sampleRate, size_t maxValue)
{
    auto size = static_cast<size_t>(std::round(milliseconds * 0.001f * sampleRate));
    return std::clamp<size_t>(size, 1, maxValue);
}

template <size_t Channels, size_t FixedFrameSize, typename ExternalBufferType>
class FixedSizeProcessor
{
  public:
    using InternalBuffer = AudioBuffer<Channels, FixedFrameSize>;
    using ProcessFunction = std::function<void(const InternalBuffer&, InternalBuffer&)>;

    explicit FixedSizeProcessor(ProcessFunction processFunc)
        : m_processFunc(std::move(processFunc))
    {
    }

    void processBlock(ExternalBufferType& buffer)
    {
        const auto totalNumChannels = static_cast<unsigned>(buffer.getNumChannels());
        const auto totalNumSamples = static_cast<unsigned>(buffer.getNumSamples());
        unsigned frameIndex = 0;

        while (frameIndex < totalNumSamples)
        {
            const auto framesToProcess =
                std::min<unsigned>(static_cast<unsigned>(FixedFrameSize) - m_feedAndEat, totalNumSamples - frameIndex);

            for (unsigned channel = 0; channel < std::min<unsigned>(totalNumChannels, Channels); ++channel)
            {
                const auto* inputChannelData = buffer.getReadPointer(static_cast<int>(channel)) + frameIndex;
                auto* outputChannelData = buffer.getWritePointer(static_cast<int>(channel)) + frameIndex;

                for (unsigned frame = 0; frame < framesToProcess; ++frame)
                {
                    m_inputBuffer(m_feedAndEat + frame, channel) = inputChannelData[frame];
                    outputChannelData[frame] = m_outputBuffer(m_feedAndEat + frame, channel);
                }
            }

            m_feedAndEat += framesToProcess;
            frameIndex += framesToProcess;

            if (m_feedAndEat == FixedFrameSize)
            {
                m_processFunc(m_inputBuffer, m_outputBuffer);
                m_feedAndEat = 0;
            }
        }
    }
    void processBlockOutputOnly(ExternalBufferType& buffer)
    {
        const auto totalNumChannels = static_cast<unsigned>(buffer.getNumChannels());
        const auto totalNumSamples = static_cast<unsigned>(buffer.getNumSamples());
        unsigned frameIndex = 0;

        while (frameIndex < totalNumSamples)
        {
            const auto framesToProcess =
                std::min<unsigned>(static_cast<unsigned>(FixedFrameSize) - m_feedAndEat, totalNumSamples - frameIndex);

            for (unsigned channel = 0; channel < std::min<unsigned>(totalNumChannels, Channels); ++channel)
            {
                auto* outputChannelData = buffer.getWritePointer(static_cast<int>(channel)) + frameIndex;

                for (unsigned frame = 0; frame < framesToProcess; ++frame)
                {
                    m_inputBuffer(m_feedAndEat + frame, channel) = 0;
                    outputChannelData[frame] = m_outputBuffer(m_feedAndEat + frame, channel);
                }
            }

            m_feedAndEat += framesToProcess;
            frameIndex += framesToProcess;

            if (m_feedAndEat == FixedFrameSize)
            {
                m_processFunc(m_inputBuffer, m_outputBuffer);
                m_feedAndEat = 0;
            }
        }
    }

  private:
    ProcessFunction m_processFunc;
    InternalBuffer m_inputBuffer{};
    InternalBuffer m_outputBuffer{};
    unsigned m_feedAndEat{0};
};
}