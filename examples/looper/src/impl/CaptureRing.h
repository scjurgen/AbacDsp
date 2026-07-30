#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Audio/AudioBuffer.h"

// Always-on raw-input capture ring, independent of the recorder's own state,
// so a bar-locked take's start can reach back to audio that arrived before
// its own trigger (see LooperImpl::beginBarLockedRecord()).
template <size_t BlockSize>
class CaptureRing
{
  public:
    // Sized to cover half a bar of late-start backfill at the slowest
    // supported tempo.
    explicit CaptureRing(const float sampleRate)
        : m_ringCapacityFrames(std::max<size_t>(BlockSize, static_cast<size_t>(sampleRate * 8.f)))
        , m_ring(m_ringCapacityFrames * 2, 0.f)
        , m_preRoll(m_ringCapacityFrames * 2, 0.f) // only the actual gap length is used
    {
    }

    // Writes this block's raw input into the ring at absPos.
    void update(const AbacDsp::AudioBuffer<2, BlockSize>& in, const uint64_t absPos) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto pos = static_cast<size_t>((absPos + i) % m_ringCapacityFrames);
            m_ring[pos * 2] = in(i, 0);
            m_ring[pos * 2 + 1] = in(i, 1);
        }
    }

    // Snapshots the late-start gap [tickAbs, trigAbs); empty if not late.
    void snapshotPreRoll(const uint64_t tickAbs, const uint64_t trigAbs) noexcept
    {
        if (trigAbs <= tickAbs)
        {
            m_preRollLen = 0;
            return;
        }
        const uint64_t gap = trigAbs - tickAbs;
        m_preRollLen = std::min(m_preRoll.size() / 2, static_cast<size_t>(gap));
        for (size_t f = 0; f < m_preRollLen; ++f)
        {
            const auto ringPos = static_cast<size_t>((tickAbs + f) % m_ringCapacityFrames);
            m_preRoll[f * 2] = m_ring[ringPos * 2];
            m_preRoll[f * 2 + 1] = m_ring[ringPos * 2 + 1];
        }
    }

    // Valid only until the next snapshotPreRoll() call.
    [[nodiscard]] std::span<const float> preRoll() const noexcept
    {
        return {m_preRoll.data(), m_preRollLen * 2};
    }

  private:
    size_t m_ringCapacityFrames{0};
    std::vector<float> m_ring;    // always-on raw input capture, interleaved stereo
    std::vector<float> m_preRoll; // snapshot taken immediately when a bar-locked take starts
    size_t m_preRollLen{0};       // valid length within m_preRoll (the late-start gap)
};
