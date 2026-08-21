#pragma once

#include <array>
#include <cstddef>

#include "Sampler/LoopRecorder.h"

namespace AbacDsp
{

/**
 * @ingroup sampler
 * @brief Fixed bank of independent loop parts (A-D): storage only.
 *
 * Each part is a fully independent LoopRecorder; parts are never required to
 * share a loop length or bar count. This class owns storage and an "active"
 * index only. Deciding when a switch may commit, how to crossfade, and what
 * a part selection means belongs to the caller (mirrors how SliceLibrary/
 * SequencerEngine are dumb engines driven by the example, not the DSP core).
 */
template <size_t BlockSize>
class LoopPartBank
{
  public:
    static constexpr size_t kMaxParts = 4;

    LoopPartBank(const float sampleRate, const float maxSecondsPerPart)
        : m_parts{LoopRecorder<BlockSize>{sampleRate, maxSecondsPerPart},
                  LoopRecorder<BlockSize>{sampleRate, maxSecondsPerPart},
                  LoopRecorder<BlockSize>{sampleRate, maxSecondsPerPart},
                  LoopRecorder<BlockSize>{sampleRate, maxSecondsPerPart}}
    {
    }

    [[nodiscard]] LoopRecorder<BlockSize>& part(const size_t index) noexcept
    {
        return m_parts[index];
    }

    [[nodiscard]] const LoopRecorder<BlockSize>& part(const size_t index) const noexcept
    {
        return m_parts[index];
    }

    [[nodiscard]] LoopRecorder<BlockSize>& active() noexcept
    {
        return m_parts[m_activeIndex];
    }

    [[nodiscard]] const LoopRecorder<BlockSize>& active() const noexcept
    {
        return m_parts[m_activeIndex];
    }

    [[nodiscard]] size_t activeIndex() const noexcept
    {
        return m_activeIndex;
    }

    // Storage only: does not touch any part's audio or transport state. The
    // caller decides when a switch may actually commit.
    void setActiveIndex(const size_t index) noexcept
    {
        m_activeIndex = index;
    }

    [[nodiscard]] bool hasContent(const size_t index) const noexcept
    {
        return m_parts[index].hasLoop();
    }

    [[nodiscard]] size_t loopLengthFrames(const size_t index) const noexcept
    {
        return m_parts[index].loopLengthFrames();
    }

  private:
    std::array<LoopRecorder<BlockSize>, kMaxParts> m_parts;
    size_t m_activeIndex{0};
};

}
