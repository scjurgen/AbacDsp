#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace AbacDsp
{

template <size_t MAXSIZE>
class NaiveDelay
{
  public:
    NaiveDelay()
        : m_buffer(MAXSIZE, 0.f)
    {
    }

    void setSize(const size_t newSize)
    {
        m_currentDelayWidth = std::min(newSize, MAXSIZE - 2);
        m_read = m_head - m_currentDelayWidth + MAXSIZE;
        while (m_read >= m_buffer.size())
        {
            m_read -= m_buffer.size();
        }
    }

    float step(const float in)
    {
        m_buffer[m_head++] = in;
        if (m_head >= m_buffer.size())
        {
            m_head = 0;
        }
        const float result = m_buffer[m_read++];
        if (m_read >= m_buffer.size())
        {
            m_read = 0;
        }
        return result;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](const float in) { return step(in); });
    }

  private:
    size_t m_currentDelayWidth{MAXSIZE / 8};
    size_t m_head{0};
    size_t m_read{MAXSIZE - MAXSIZE / 8 - 1};
    std::vector<float> m_buffer;
};

}
