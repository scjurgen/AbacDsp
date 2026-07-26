#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "Analysis/Slicer.h"

namespace AbacDsp
{

// One slice's thumbnail (see SliceLibrary::thumbnail()) placed on a normalized
// 0..1 timeline, e.g. a sequencer pattern. Not a ring buffer, unlike SpectrumImageSet.
struct SequencerSliceThumbnail
{
    float normalizedStart{0.f};
    float normalizedWidth{0.f};
    size_t width{0};
    size_t height{0};
    const float* data{nullptr};
    float sampleRate{48000.f};
};

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
    static constexpr size_t kThumbWidth = 24;  // time frames
    static constexpr size_t kThumbHeight = 32; // frequency bins
    static constexpr size_t kThumbFloats = kThumbWidth * kThumbHeight;

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
        , m_thumbnailPool(maxSlices * kThumbFloats, 0.f)
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
    // `thumbnails`, if given, holds one kThumbFloats-float block per candidate in
    // `slices` (same order); blocks for slices this call skips or truncates on
    // are simply not copied, so the caller never has to mirror this method's own
    // skip logic. Returns the new track index.
    size_t extractTrack(std::span<const float> interleavedLoop, std::span<const Slice> slices,
                        std::span<const float> thumbnails = {})
    {
        const size_t track = m_trackStart.size();
        m_trackStart.push_back(m_slices.size());
        const size_t loopFrames = interleavedLoop.size() / kChannels;
        for (size_t i = 0; i < slices.size(); ++i)
        {
            const Slice& s = slices[i];
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
            copyThumbnail(i, thumbnails);
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

    // Flat kThumbWidth x kThumbHeight magnitude block for this slice, or an empty
    // span if none was supplied to extractTrack() or the slice index is stale.
    [[nodiscard]] std::span<const float> thumbnail(const size_t track, const size_t indexInTrack) const noexcept
    {
        if (track >= m_trackStart.size())
        {
            return {};
        }
        const size_t offset = (m_trackStart[track] + indexInTrack) * kThumbFloats;
        if (offset + kThumbFloats > m_thumbnailPool.size())
        {
            return {};
        }
        return std::span<const float>{&m_thumbnailPool[offset], kThumbFloats};
    }

  private:
    // candidateIndex indexes `thumbnails` (pre-skip candidates); destination is
    // the slice just appended to m_slices. No-ops if either span is short.
    void copyThumbnail(const size_t candidateIndex, std::span<const float> thumbnails) noexcept
    {
        const size_t srcOffset = candidateIndex * kThumbFloats;
        if (srcOffset + kThumbFloats > thumbnails.size())
        {
            return;
        }
        const size_t dstOffset = (m_slices.size() - 1) * kThumbFloats;
        if (dstOffset + kThumbFloats > m_thumbnailPool.size())
        {
            return;
        }
        std::copy_n(thumbnails.data() + srcOffset, kThumbFloats, m_thumbnailPool.data() + dstOffset);
    }

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
    std::vector<float> m_thumbnailPool; // one kThumbFloats-float block per slice, indexed like m_slices
    std::vector<SliceInfo> m_slices;
    std::vector<size_t> m_trackStart; // m_trackStart[t] = index into m_slices where track t begins
    size_t m_usedFrames{0};
};
}
