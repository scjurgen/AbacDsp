#pragma once

#include <array>
#include <vector>
#include <cassert>

namespace AbacDsp
{
template <size_t BlockSize, size_t CHANNELS, size_t MAXSIZE, size_t NUMREADHEADS = 1>
    requires(BlockSize > 0 && MAXSIZE > BlockSize && CHANNELS > 1)
class ParallelPlainDelay
{
  public:
    ParallelPlainDelay()
    {
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            m_buffer[c].resize(BlockSize + (MAXSIZE / BlockSize + 1) * BlockSize);
        }
    }

    void setSize(const size_t index, const size_t newSize)
    {
        assert(index < m_read[0].size());
        m_currentDelayWidth[index] = std::min(newSize, MAXSIZE - 1);
        m_read[0][index] = m_head + MAXSIZE - m_currentDelayWidth[index];
        while (m_read[0][index] >= m_buffer[index].size())
        {
            m_read[0][index] -= m_buffer[index].size();
        }
        for (size_t headIdx = 1; headIdx <= m_deltaTaps.size(); ++headIdx)
        {
            m_read[headIdx][index] = m_head + MAXSIZE - m_currentDelayWidth[index] + m_deltaTaps[headIdx - 1][index];
            while (m_read[headIdx][index] >= m_buffer[index].size())
            {
                m_read[headIdx][index] -= m_buffer[index].size();
            }
        }
    }

    // set relative to first read head (will assert if trying on headIdx = 0)
    void setRelativeHead(const size_t headIdx, const size_t index, const int newDistance)
    {
        assert(headIdx > 0 && headIdx < m_read.size() && index < m_read[headIdx].size());
        m_deltaTaps[headIdx - 1][index] = newDistance;
        m_read[headIdx][index] = m_head + MAXSIZE - m_currentDelayWidth[index] + m_deltaTaps[headIdx - 1][index];
        while (m_read[headIdx][index] >= m_buffer[index].size())
        {
            m_read[headIdx][index] -= m_buffer[index].size();
        }
    }

    void processBlock(const std::array<std::array<float, BlockSize>, CHANNELS>& src,
                      std::array<std::array<float, BlockSize>, CHANNELS>& target) noexcept
    {
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                m_buffer[c][m_head + i] = src[c][i];
            }
        }
        // copy in data to end of buffer wrap if needed
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            if (m_head == 0)
            {
                std::copy_n(m_buffer[c].data(), BlockSize, m_buffer[c].data() + MAXSIZE);
            }
        }
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                target[c][i] = m_buffer[c][m_read[0][c] + i];
            }
        }
        m_head += BlockSize;
        if (m_head >= MAXSIZE)
        {
            m_head = 0;
        }
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            m_read[0][c] += BlockSize;
            if (m_read[0][c] >= MAXSIZE)
            {
                m_read[0][c] -= MAXSIZE;
            }
        }
    }

    void processHead(size_t idx, std::array<std::array<float, BlockSize>, CHANNELS>& target) noexcept
    {
        assert(idx > 0 && idx < m_read.size());
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                target[c][i] = m_buffer[c][m_read[idx][c] + i];
            }
        }
        for (size_t c = 0; c < CHANNELS; ++c)
        {
            m_read[idx][c] += BlockSize;
            if (m_read[idx][c] >= MAXSIZE)
            {
                m_read[idx][c] -= MAXSIZE;
            }
        }
    }

  private:
    alignas(16) std::array<int, CHANNELS> m_currentDelayWidth{MAXSIZE / 8};
    alignas(16) std::array<std::array<int, CHANNELS>, NUMREADHEADS - 1> m_deltaTaps{};
    alignas(16) std::array<std::array<int, CHANNELS>, NUMREADHEADS> m_read{};
    alignas(16) std::array<std::vector<float>, CHANNELS> m_buffer;
    size_t m_head{};
};
}