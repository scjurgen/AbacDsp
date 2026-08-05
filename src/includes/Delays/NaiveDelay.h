#pragma once

#include <algorithm>
#include <span>
#include <vector>

namespace AbacDsp
{

/**
 * @ingroup delays
 * @brief Integer-sample delay line with independent read and write cursors.
 *
 * Naive in the sense that the delay is whole samples only: there is no
 * interpolation, so setSize() steps the read cursor and any change clicks.
 * That also makes it the cheapest line here, one store and one load per sample,
 * and the exact one to measure the interpolating variants against.
 *
 * Length is clamped to MAXSIZE - 2, keeping the cursors apart even at maximum.
 * The buffer is a std::vector sized once in the constructor, so the delay never
 * allocates after construction but is not usable as a constexpr object.
 */
template <size_t MAXSIZE>
class NaiveDelay
{
  public:
    NaiveDelay()
        : m_buffer(MAXSIZE, 0.f)
    {
    }

    void setSize(const size_t newSize) noexcept
    {
        m_currentDelayWidth = std::min(newSize, MAXSIZE - 2);
        m_read = m_head - m_currentDelayWidth + MAXSIZE;
        while (m_read >= m_buffer.size())
        {
            m_read -= m_buffer.size();
        }
    }

    [[nodiscard]] float step(const float in) noexcept
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

    void processBlock(std::span<const float> source, std::span<float> target) noexcept
    {
        std::transform(source.begin(), source.end(), target.begin(), [this](const float in) { return step(in); });
    }

  private:
    size_t m_currentDelayWidth{MAXSIZE / 8};
    size_t m_head{0};
    size_t m_read{MAXSIZE - MAXSIZE / 8 - 1};
    std::vector<float> m_buffer;
};

}
