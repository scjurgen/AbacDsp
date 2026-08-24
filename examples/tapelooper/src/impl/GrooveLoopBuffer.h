#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

// Fixed-capacity circular stereo buffer sitting between GrooveDrumPlayer's
// generation and the real-time output read: the write cursor is kept a small
// lookahead ahead of the read cursor (see GrooverImpl::processBlock()), so
// generation and playback are decoupled by a constant delay rather than tied
// 1:1 to the same sample. Capacity is generous (default 60 s) purely so the
// ring never wraps oddly; the steady-state gap it actually holds is tiny.
class GrooveLoopBuffer
{
  public:
    explicit GrooveLoopBuffer(const float sampleRate, const float capacitySeconds = 60.f)
        : m_capacityFrames(std::max<size_t>(1, static_cast<size_t>(sampleRate * capacitySeconds)))
        , m_ring(m_capacityFrames * 2, 0.f)
    {
    }

    void writeFrame(const float left, const float right) noexcept
    {
        const size_t index = m_writeFrame % m_capacityFrames;
        m_ring[index * 2] = left;
        m_ring[index * 2 + 1] = right;
        ++m_writeFrame;
    }

    [[nodiscard]] std::array<float, 2> readFrame() noexcept
    {
        const size_t index = m_readFrame % m_capacityFrames;
        const std::array<float, 2> frame{m_ring[index * 2], m_ring[index * 2 + 1]};
        ++m_readFrame;
        return frame;
    }

    // Unread material ahead of the read cursor. Cursors grow unbounded (only
    // indexed mod capacity), so a write catching up exactly to a read can't be
    // mistaken for empty - the classic circular-buffer full/empty ambiguity.
    [[nodiscard]] size_t framesAhead() const noexcept
    {
        return m_writeFrame - m_readFrame;
    }

    void reset() noexcept
    {
        m_writeFrame = 0;
        m_readFrame = 0;
    }

  private:
    size_t m_capacityFrames;
    std::vector<float> m_ring; // interleaved stereo
    size_t m_writeFrame{0};
    size_t m_readFrame{0};
};
