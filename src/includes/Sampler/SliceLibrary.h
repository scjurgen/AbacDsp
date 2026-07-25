#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "Analysis/Slicer.h"

namespace AbacDsp
{

// Stores and owns interleaved stereo audio slices in an internal pool.
// Slices are grouped into tracks; new tracks are appended and never overwrite
// existing data until clear() is called.
//
// Each slice references a contiguous region in the pool and is addressed by
// (track, indexInTrack). The pool grows monotonically up to maxFrames; if
// capacity is exceeded during extraction, remaining slices of that track are
// ignored.
//
// Preallocates storage to avoid allocations during extraction after
// construction. Not thread-safe.
class SliceLibrary
{
  public:
    static constexpr size_t kChannels = 2;

    struct SliceInfo
    {
        size_t track{0};
        size_t startFrame{0}; // pool offset
        size_t lengthFrames{0};
        float peak{0.f}; // max |sample| across both channels
        float rms{0.f};  // RMS across both channels
    };

    explicit SliceLibrary(const size_t maxFrames, const size_t maxSlices = 256)
        : m_maxFrames(maxFrames)
        , m_pool(maxFrames * kChannels, 0.f)
    {
        m_slices.reserve(maxSlices);
        m_trackStart.reserve(maxSlices); // a track can't outnumber the library's own slices
    }

    // Drops every track. Explicit action only: the library outlives the base
    // looper's own Clear/re-record so slices stay available across a session.
    void clear() noexcept
    {
        m_slices.clear();
        m_trackStart.clear();
        m_usedFrames = 0;
    }

    // Copies each slice's interleaved stereo audio out of the loop into the pool,
    // appended after any existing tracks. Slices are clamped to the loop end; a
    // slice that would overflow the pool stops extraction for this track (the
    // rest of that track's slices are dropped, earlier tracks are untouched).
    // Returns the new track index.
    size_t extractTrack(std::span<const float> interleavedLoop, std::span<const Slice> slices)
    {
        const size_t track = m_trackStart.size();
        m_trackStart.push_back(m_slices.size());
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
            m_slices.push_back({track, m_usedFrames, len, peakOf(src, len), rmsOf(src, len)});
            m_usedFrames += len;
        }
        return track;
    }

    [[nodiscard]] size_t trackCount() const noexcept
    {
        return m_trackStart.size();
    }

    [[nodiscard]] size_t sliceCount() const noexcept
    {
        return m_slices.size();
    }

    [[nodiscard]] size_t sliceCountInTrack(const size_t track) const noexcept
    {
        if (track >= m_trackStart.size())
        {
            return 0;
        }
        const size_t begin = m_trackStart[track];
        const size_t end = (track + 1 < m_trackStart.size()) ? m_trackStart[track + 1] : m_slices.size();
        return end - begin;
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

    // Flat slice list across all tracks, in extraction order.
    [[nodiscard]] const std::vector<SliceInfo>& slices() const noexcept
    {
        return m_slices;
    }

    [[nodiscard]] const SliceInfo& sliceInfo(const size_t track, const size_t indexInTrack) const noexcept
    {
        return m_slices[m_trackStart[track] + indexInTrack];
    }

    [[nodiscard]] float sample(const size_t track, const size_t indexInTrack, const size_t frame,
                               const size_t channel) const noexcept
    {
        const SliceInfo& s = sliceInfo(track, indexInTrack);
        return m_pool[(s.startFrame + frame) * kChannels + channel];
    }

  private:
    [[nodiscard]] static float peakOf(const float* interleaved, const size_t frames) noexcept
    {
        float peak = 0.f;
        for (size_t i = 0; i < frames * kChannels; ++i)
        {
            peak = std::max(peak, std::abs(interleaved[i]));
        }
        return peak;
    }

    [[nodiscard]] static float rmsOf(const float* interleaved, const size_t frames) noexcept
    {
        if (frames == 0)
        {
            return 0.f;
        }
        double sumSq = 0.0;
        for (size_t i = 0; i < frames * kChannels; ++i)
        {
            const double v = interleaved[i];
            sumSq += v * v;
        }
        return static_cast<float>(std::sqrt(sumSq / static_cast<double>(frames * kChannels)));
    }

    size_t m_maxFrames;
    std::vector<float> m_pool;
    std::vector<SliceInfo> m_slices;
    std::vector<size_t> m_trackStart; // m_trackStart[t] = index into m_slices where track t begins
    size_t m_usedFrames{0};
};
}
