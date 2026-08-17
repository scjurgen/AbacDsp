#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace AbacDsp
{

/**
 * @ingroup delays
 * @brief One shared integer-sample delay buffer with several independent read taps.
 *
 * Same whole-samples-only, no-interpolation contract as NaiveDelay - see that class -
 * but reworked around a single write() advancing one shared buffer, with any number of
 * readTap(index) calls reading it back at independently configured offsets. Use this
 * instead of an array of NaiveDelay when several taps read the same underlying signal
 * (e.g. Resonik's resonance chains), so only one buffer's worth of memory is held
 * regardless of tap count. Each tap's width is clamped to MAXSIZE - 2, keeping its read
 * cursor apart from the write head even at maximum.
 */
template <size_t MAXSIZE, size_t MAXTAPS>
class MultiTapDelay
{
  public:
    MultiTapDelay()
        : m_buffer(MAXSIZE, 0.f)
    {
    }

    void setTapDelay(const size_t tapIndex, const size_t newSize) noexcept
    {
        const size_t width = std::min(newSize, MAXSIZE - 2);
        m_tapRead[tapIndex] = m_head - width + MAXSIZE;
        while (m_tapRead[tapIndex] >= m_buffer.size())
        {
            m_tapRead[tapIndex] -= m_buffer.size();
        }
    }

    void write(const float in) noexcept
    {
        m_buffer[m_head++] = in;
        if (m_head >= m_buffer.size())
        {
            m_head = 0;
        }
    }

    [[nodiscard]] float readTap(const size_t tapIndex) noexcept
    {
        const float result = m_buffer[m_tapRead[tapIndex]++];
        if (m_tapRead[tapIndex] >= m_buffer.size())
        {
            m_tapRead[tapIndex] = 0;
        }
        return result;
    }

  private:
    std::array<size_t, MAXTAPS> m_tapRead{};
    size_t m_head{0};
    std::vector<float> m_buffer;
};

}
