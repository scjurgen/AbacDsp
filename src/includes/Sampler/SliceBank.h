#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include "Analysis/Slicer.h"

namespace AbacDsp
{

// Owns extracted copies of a loop's slices in one contiguous preallocated pool,
// so playback reads from these copies rather than the live record buffer (which
// may be overdubbed or re-recorded underneath). Filled off the audio thread
// (allocation-free after construction: the pool is preallocated and the slice
// table is reserved), then handed to the playback path.
//
// The extracted slices are laid out back-to-back; bankSlices() returns them as
// Slice{startFrame = pool offset, lengthFrames}, so an unmodified SlicePlayer can
// treat pool() as its loop buffer.
class SliceBank
{
  public:
    static constexpr size_t kChannels = 2;

    explicit SliceBank(const size_t maxFrames, const size_t maxSlices = 256)
        : m_maxFrames(maxFrames)
        , m_pool(maxFrames * kChannels, 0.f)
    {
        m_bankSlices.reserve(maxSlices);
    }

    void clear() noexcept
    {
        m_bankSlices.clear();
        m_usedFrames = 0;
    }

    // Copy each slice's interleaved stereo audio out of the loop into the pool,
    // back-to-back. Slices are clamped to the loop end; a slice that would overflow
    // the pool stops extraction (the rest are dropped).
    void extract(std::span<const float> interleavedLoop, std::span<const Slice> slices)
    {
        clear();
        const size_t loopFrames = interleavedLoop.size() / kChannels;
        for (const Slice& s : slices)
        {
            if (s.startFrame >= loopFrames)
            {
                continue;
            }
            const size_t len = std::min(s.lengthFrames, loopFrames - s.startFrame);
            if (len == 0)
            {
                continue;
            }
            if (m_usedFrames + len > m_maxFrames)
            {
                break;
            }
            const float* src = interleavedLoop.data() + s.startFrame * kChannels;
            std::copy_n(src, len * kChannels, m_pool.data() + m_usedFrames * kChannels);
            m_bankSlices.push_back({m_usedFrames, len});
            m_usedFrames += len;
        }
    }

    [[nodiscard]] size_t sliceCount() const noexcept
    {
        return m_bankSlices.size();
    }

    [[nodiscard]] size_t usedFrames() const noexcept
    {
        return m_usedFrames;
    }

    [[nodiscard]] size_t maxFrames() const noexcept
    {
        return m_maxFrames;
    }

    // Whole extracted pool as an interleaved stereo view (only the used prefix).
    [[nodiscard]] std::span<const float> pool() const noexcept
    {
        return std::span<const float>{m_pool.data(), m_usedFrames * kChannels};
    }

    // Slices addressed into pool() (startFrame == pool offset).
    [[nodiscard]] const std::vector<Slice>& bankSlices() const noexcept
    {
        return m_bankSlices;
    }

    [[nodiscard]] float sample(const size_t sliceIndex, const size_t frame, const size_t channel) const noexcept
    {
        const Slice& b = m_bankSlices[sliceIndex];
        return m_pool[(b.startFrame + frame) * kChannels + channel];
    }

  private:
    size_t m_maxFrames;
    std::vector<float> m_pool;
    std::vector<Slice> m_bankSlices;
    size_t m_usedFrames{0};
};

}
