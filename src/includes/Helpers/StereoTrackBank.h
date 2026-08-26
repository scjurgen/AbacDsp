#pragma once

#include <array>
#include <cstddef>

#include "Helpers/ConstructArray.h"

/**
 * @file
 * @ingroup helpers
 * @brief Owns NumTracks independent left/right instances of a per-track effect.
 *
 * A recurring multi-track pattern: one instance of some stateful DSP class per
 * (track, channel), all constructed with the same arguments, channels tracked
 * independently but usually addressed together per track. Builds on
 * constructArray() for the actual per-instance construction.
 */

namespace AbacDsp
{

template <typename T, std::size_t NumTracks>
class StereoTrackBank
{
  public:
    template <typename... Args>
    explicit StereoTrackBank(const Args&... args)
        : m_channel{constructArray<T, NumTracks>(args...), constructArray<T, NumTracks>(args...)}
    {
    }

    [[nodiscard]] T& left(const std::size_t track) noexcept
    {
        return m_channel[0][track];
    }

    [[nodiscard]] T& right(const std::size_t track) noexcept
    {
        return m_channel[1][track];
    }

    [[nodiscard]] const T& left(const std::size_t track) const noexcept
    {
        return m_channel[0][track];
    }

    [[nodiscard]] const T& right(const std::size_t track) const noexcept
    {
        return m_channel[1][track];
    }

    // Visits every (track, channel) instance, e.g. for one-time post-construction setup.
    template <typename Fn>
    void forEach(Fn&& fn)
    {
        for (auto& channel : m_channel)
        {
            for (auto& item : channel)
            {
                fn(item);
            }
        }
    }

    // Visits left(track) then right(track), e.g. to apply the same parameters to both.
    template <typename Fn>
    void forEachAtTrack(const std::size_t track, Fn&& fn)
    {
        fn(m_channel[0][track]);
        fn(m_channel[1][track]);
    }

    [[nodiscard]] static constexpr std::size_t tracks() noexcept
    {
        return NumTracks;
    }

  private:
    std::array<std::array<T, NumTracks>, 2> m_channel;
};

}
