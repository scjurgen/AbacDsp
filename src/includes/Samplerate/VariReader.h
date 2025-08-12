#pragma once

#include <vector>
#include <algorithm>

#include "Samplerate/SrConverter.h"

namespace AbacDsp
{
template <size_t BufferSize>
class VariReader
{
public:
    VariReader(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
        : m_sampleRate(sampleRate)
          , m_buffer(BufferSize, 0.f)
          , m_srConverter(filterSet)
    {
    }

    void process(float* in, const size_t numFrames)
    {
        m_input = in;
        m_inputSize = numFrames;

        if (m_tmpOutput.size() < numFrames * 4)
        {
            m_tmpOutput.resize(numFrames * 4);
        }

        const auto producedFrames =
            m_srConverter.fetchBlock(m_ratio, in, numFrames, m_tmpOutput.data(), m_tmpOutput.size(), 1);
        if (producedFrames > 0)
        {
            writeToRingBuffer(m_tmpOutput.data(), producedFrames);
        }
    }

    void setRatio(const float ratio)
    {
        m_ratio = ratio;
    }

    bool endOfData() const
    {
        return m_readHead == m_writeHead;
    }

    float pull()
    {
        const auto r = m_buffer[m_readHead];
        m_readHead++;
        if (m_readHead >= BufferSize)
        {
            m_readHead = 0;
        }
        return r;
    }

    float readFractionalDistance(float delta)
    {
    }

    size_t getDeltaHeads() const
    {
        if (m_writeHead > m_readHead)
        {
            return m_writeHead - m_readHead;
        }
        return m_writeHead + BufferSize - m_readHead;
    }

protected:
    void writeToRingBuffer(const float* data, size_t frames)
    {
        const auto available = std::min(m_buffer.size() - m_writeHead, frames);
        std::copy_n(data, available, m_buffer.begin() + m_writeHead);
        m_writeHead = (m_writeHead + available) % m_buffer.size();

        if (available < frames)
        {
            const auto remain = frames - available;
            std::copy_n(data + available, remain, m_buffer.begin());
            m_writeHead = remain;
        }
    }

protected:
    float m_sampleRate;
    std::vector<float> m_buffer;

private:
    size_t m_readHead{};
    float m_ratio{1.f};
    size_t m_writeHead{};
    float* m_input{};
    size_t m_inputSize{0};
    std::vector<float> m_tmpOutput;
    SrPushConverter m_srConverter;
};
}
