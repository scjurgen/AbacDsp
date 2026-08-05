#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "Analysis/Slicer.h"
#include "Audio/AudioBuffer.h"

namespace AbacDsp
{

/**
 * @ingroup sampler
 * @brief Plays slices of a loop buffer, one launch at a time, overlapping through a voice pool.
 *
 * A slice cut anywhere but a zero crossing starts and ends on a step, so every
 * voice fades both edges. That fade is why launching a new slice cannot simply
 * stop the previous one: it has to keep running until its tail is done, which
 * is what the voice pool is for.
 *
 * The loop audio is borrowed as a span, not copied, so a slice bank costs only
 * its boundary list. The slice vector is reserved up front so a freshly cut
 * bank can be published from the audio thread without allocating.
 */
template <size_t BlockSize>
class SlicePlayer
{
  public:
    static constexpr size_t kChannels = 2;
    static constexpr size_t kMaxVoices = 16;
    static constexpr size_t kMaxSlices = 256;

    explicit SlicePlayer(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setFadeMs(2.f);
        // Reserve so setSlices() can be called from the audio thread (when a freshly
        // sliced bank is published) without allocating.
        m_slices.reserve(kMaxSlices);
    }

    void setLoop(std::span<const float> interleavedStereo, const size_t loopLengthFrames) noexcept
    {
        m_loop = interleavedStereo;
        m_loopLength = loopLengthFrames;
    }

    void setSlices(std::span<const Slice> slices)
    {
        m_slices.assign(slices.begin(), slices.end());
    }

    void setFadeMs(const float ms) noexcept
    {
        m_fadeFrames = std::max<size_t>(1, static_cast<size_t>(ms / 1000.f * m_sampleRate));
    }

    [[nodiscard]] size_t fadeFrames() const noexcept
    {
        return m_fadeFrames;
    }

    [[nodiscard]] size_t sliceCount() const noexcept
    {
        return m_slices.size();
    }

    // playLengthFrames == 0 plays the full slice; otherwise the slice is capped
    // to that many frames (to the grid step) with a fade-out at the cap.
    void triggerSlice(const size_t sliceIndex, const size_t playLengthFrames = 0) noexcept
    {
        if (sliceIndex >= m_slices.size() || m_loopLength == 0)
        {
            return;
        }
        const Slice& slice = m_slices[sliceIndex];
        const size_t playLen =
            (playLengthFrames == 0) ? slice.lengthFrames : std::min(slice.lengthFrames, playLengthFrames);
        if (playLen == 0)
        {
            return;
        }
        Voice& voice = allocateVoice();
        voice.active = true;
        voice.sliceStart = slice.startFrame;
        voice.sliceLen = slice.lengthFrames;
        voice.playLen = playLen;
        voice.pos = 0;
        voice.effectiveFade = std::max<size_t>(1, std::min(m_fadeFrames, playLen / 2));
        voice.startOrder = m_triggerCounter++;
    }

    void reset() noexcept
    {
        for (Voice& voice : m_voices)
        {
            voice.active = false;
        }
    }

    void processBlock(AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = 0.f;
            out(i, 1) = 0.f;
        }
        for (Voice& voice : m_voices)
        {
            if (voice.active)
            {
                renderVoice(voice, out);
            }
        }
    }

    [[nodiscard]] size_t activeVoiceCount() const noexcept
    {
        size_t count = 0;
        for (const Voice& voice : m_voices)
        {
            count += voice.active ? 1 : 0;
        }
        return count;
    }

  private:
    /// @brief One playing slice: its position, its fade state, and which slice it is.
    struct Voice
    {
        bool active{false};
        size_t sliceStart{0};
        size_t sliceLen{0};
        size_t playLen{0};
        size_t pos{0};
        size_t effectiveFade{1};
        uint64_t startOrder{0};
    };

    [[nodiscard]] Voice& allocateVoice() noexcept
    {
        for (Voice& voice : m_voices)
        {
            if (!voice.active)
            {
                return voice;
            }
        }
        // Steal the oldest voice; re-init fades in from zero so the new slice's
        // leading edge stays click-free.
        Voice* oldest = &m_voices[0];
        for (Voice& voice : m_voices)
        {
            if (voice.startOrder < oldest->startOrder)
            {
                oldest = &voice;
            }
        }
        return *oldest;
    }

    void renderVoice(Voice& voice, AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float gain = edgeGain(voice);
            const size_t frame = voice.sliceStart + voice.pos;
            if (voice.pos < voice.sliceLen && frame * kChannels + 1 < m_loop.size())
            {
                out(i, 0) += m_loop[frame * kChannels] * gain;
                out(i, 1) += m_loop[frame * kChannels + 1] * gain;
            }
            if (++voice.pos >= voice.playLen)
            {
                voice.active = false;
                return;
            }
        }
    }

    [[nodiscard]] static float edgeGain(const Voice& voice) noexcept
    {
        const float fade = static_cast<float>(voice.effectiveFade);
        const float fadeIn = static_cast<float>(voice.pos + 1) / fade;
        const float fadeOut = static_cast<float>(voice.playLen - voice.pos) / fade;
        return std::clamp(std::min(fadeIn, fadeOut), 0.f, 1.f);
    }

    float m_sampleRate;
    size_t m_fadeFrames{1};

    std::span<const float> m_loop{};
    size_t m_loopLength{0};
    std::vector<Slice> m_slices;

    std::array<Voice, kMaxVoices> m_voices{};
    uint64_t m_triggerCounter{1};
};

}
