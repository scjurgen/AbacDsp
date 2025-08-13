#pragma once

#include <vector>
#include <algorithm>

#include "Samplerate/SrPushConverter.h"
#include "Numbers/Interpolation.h"
namespace AbacDsp
{
template <size_t BufferSize, size_t NumChannels>
class VariReader
{
  public:
    VariReader(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
        : m_sampleRate(sampleRate)
        , m_buffer((BufferSize + 6) * NumChannels, 0)
        , m_srConverter(filterSet)
        , m_tmpOutput(16384, 0)
    {
    }

    void readBlock(float* out, const size_t numFrames)
    {
        std::generate_n(out, numFrames,
                        [&]()
                        {
                            size_t index = std::floor(m_delayReadHead);
                            const float fraction = m_delayReadHead - index;

                            m_delayReadHead += m_delayReadHeadAdvance;
                            if (m_delayReadHead >= BufferSize)
                            {
                                m_delayReadHead -= BufferSize;
                            }
                            return Interpolation<float>::bspline_43x(&m_buffer[index], fraction);
                        });
    }

    void process(const float* in, const size_t numFrames)
    {
        m_input = in;
        m_inputSize = numFrames;
        const auto producedFrames =
            m_srConverter.fetchBlock(m_ratio, in, numFrames, m_tmpOutput.data(), m_tmpOutput.size());
        if (producedFrames > 0)
        {
            writeToRingBuffer(m_tmpOutput.data(), producedFrames);
        }
    }

    void setReadHead(const float delta)
    {
        m_delayReadHead = m_writeHead + BufferSize - delta;
        if (m_delayReadHead >= BufferSize)
        {
            m_delayReadHead -= BufferSize;
        }
        m_readHead = m_delayReadHead;
    }

    void setRatio(const float ratio)
    {
        m_ratio = ratio;
        m_delayReadHeadAdvance = 1.f / ratio;
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

    size_t getDeltaHeads() const
    {
        if (m_writeHead > m_readHead)
        {
            return m_writeHead - m_readHead;
        }
        return m_writeHead + BufferSize - m_readHead;
    }

    void reset()
    {
        std::ranges::fill(m_buffer, 0);
        std::ranges::fill(m_tmpOutput, 0);
    }

  protected:
    void writeToRingBuffer(const float* data, size_t frames)
    {
        const auto available = std::min(BufferSize - m_writeHead, frames * NumChannels);
        std::copy_n(data, available, m_buffer.begin() + m_writeHead);
        m_writeHead = (m_writeHead + available) % BufferSize;

        if (available < frames * NumChannels) // wrap to start
        {
            const auto remain = frames * NumChannels - available;
            std::copy_n(data + available, remain, m_buffer.begin());
            m_writeHead = remain;
        }
        if (m_writeHead < 6 * NumChannels)
        {
            std::copy_n(m_buffer.begin(), 6 * NumChannels, m_buffer.begin() + BufferSize);
        }
    }

  protected:
    float m_sampleRate;
    std::vector<float> m_buffer;

  private:
    size_t m_readHead{};
    float m_ratio{1.f};
    size_t m_writeHead{};
    float m_delayReadHead{};
    float m_delayReadHeadAdvance{1.f};
    const float* m_input{};
    size_t m_inputSize{0};
    std::vector<float> m_tmpOutput;
    SrPushConverter<NumChannels> m_srConverter;
};
}
