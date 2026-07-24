#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include "Audio/AudioBuffer.h"

namespace AbacDsp
{

enum class LooperState : uint8_t
{
    Empty,       // no loop captured yet
    Recording,   // capturing the first loop
    Playing,     // looping playback
    Overdubbing, // playing while summing input into the loop
    Stopped      // has a loop, not playing
};

// Stereo loop capture/playback with a bar-quantized loop length. All storage is
// preallocated; nothing allocates on the audio thread. Slicing and beat-locked
// slice playback live in separate components; this owns the raw loop buffer.
template <size_t BlockSize>
class LoopRecorder
{
  public:
    static constexpr size_t kChannels = 2;

    explicit LoopRecorder(const float sampleRate, const float maxSeconds = 60.f)
        : m_maxFrames(std::max<size_t>(1, static_cast<size_t>(sampleRate * maxSeconds)))
    {
        m_buffer.assign(m_maxFrames * kChannels, 0.f);
    }

    void setSamplesPerBar(const size_t samplesPerBar) noexcept
    {
        m_samplesPerBar = samplesPerBar;
    }

    // decay < 1 fades the existing loop as new material is layered on top.
    void setOverdubDecay(const float decay) noexcept
    {
        m_overdubDecay = decay;
    }

    void beginRecord() noexcept
    {
        m_recordedFrames = 0;
        m_playPos = 0;
        m_state = LooperState::Recording;
    }

    void stopRecord() noexcept
    {
        if (m_state != LooperState::Recording)
        {
            return;
        }
        finalizeLoop();
    }

    void play() noexcept
    {
        if (hasLoop())
        {
            m_state = LooperState::Playing;
        }
    }

    void pause() noexcept
    {
        if (m_state == LooperState::Playing || m_state == LooperState::Overdubbing)
        {
            m_state = LooperState::Stopped;
        }
    }

    void stop() noexcept
    {
        if (m_state == LooperState::Recording)
        {
            finalizeLoop();
        }
        m_playPos = 0;
        if (hasLoop())
        {
            m_state = LooperState::Stopped;
        }
    }

    void beginOverdub() noexcept
    {
        if (hasLoop())
        {
            m_state = LooperState::Overdubbing;
        }
    }

    void endOverdub() noexcept
    {
        if (m_state == LooperState::Overdubbing)
        {
            m_state = LooperState::Playing;
        }
    }

    void clear() noexcept
    {
        m_state = LooperState::Empty;
        m_recordedFrames = 0;
        m_loopLengthFrames = 0;
        m_playPos = 0;
    }

    void processBlock(const AudioBuffer<kChannels, BlockSize>& in, AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        switch (m_state)
        {
            case LooperState::Recording:
                recordBlock(in, out);
                break;
            case LooperState::Playing:
                playBlock(out);
                break;
            case LooperState::Overdubbing:
                overdubBlock(in, out);
                break;
            case LooperState::Empty:
            case LooperState::Stopped:
                silence(out);
                break;
        }
    }

    [[nodiscard]] LooperState state() const noexcept
    {
        return m_state;
    }

    [[nodiscard]] bool hasLoop() const noexcept
    {
        return m_loopLengthFrames > 0;
    }

    [[nodiscard]] size_t loopLengthFrames() const noexcept
    {
        return m_loopLengthFrames;
    }

    [[nodiscard]] size_t recordedFrames() const noexcept
    {
        return m_recordedFrames;
    }

    [[nodiscard]] size_t playPositionFrames() const noexcept
    {
        return m_playPos;
    }

    [[nodiscard]] size_t maxFrames() const noexcept
    {
        return m_maxFrames;
    }

    [[nodiscard]] float sample(const size_t frame, const size_t channel) const noexcept
    {
        return m_buffer[frame * kChannels + channel];
    }

    // Interleaved view over the finalized loop, for the slicer.
    [[nodiscard]] std::span<const float> loopView() const noexcept
    {
        return std::span<const float>{m_buffer.data(), m_loopLengthFrames * kChannels};
    }

  private:
    void finalizeLoop() noexcept
    {
        if (m_recordedFrames == 0)
        {
            clear();
            return;
        }
        m_loopLengthFrames = std::min(quantizeToBar(m_recordedFrames), m_maxFrames);
        zeroTail();
        m_playPos = 0;
        m_state = LooperState::Playing;
    }

    [[nodiscard]] size_t quantizeToBar(const size_t frames) const noexcept
    {
        if (m_samplesPerBar == 0)
        {
            return frames;
        }
        size_t bars = (frames + m_samplesPerBar / 2) / m_samplesPerBar;
        if (bars == 0)
        {
            bars = 1;
        }
        return bars * m_samplesPerBar;
    }

    // A bar-quantized length can exceed what was captured; the padding must be
    // silent even after a clear+shorter re-record left stale audio behind it.
    void zeroTail() noexcept
    {
        for (size_t frame = m_recordedFrames; frame < m_loopLengthFrames; ++frame)
        {
            m_buffer[frame * kChannels] = 0.f;
            m_buffer[frame * kChannels + 1] = 0.f;
        }
    }

    void recordBlock(const AudioBuffer<kChannels, BlockSize>& in, AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (m_recordedFrames >= m_maxFrames)
            {
                finalizeLoop();
                silenceFrom(out, i);
                return;
            }
            m_buffer[m_recordedFrames * kChannels] = in(i, 0);
            m_buffer[m_recordedFrames * kChannels + 1] = in(i, 1);
            ++m_recordedFrames;
            out(i, 0) = 0.f;
            out(i, 1) = 0.f;
        }
    }

    void playBlock(AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = m_buffer[m_playPos * kChannels];
            out(i, 1) = m_buffer[m_playPos * kChannels + 1];
            advancePlay();
        }
    }

    void overdubBlock(const AudioBuffer<kChannels, BlockSize>& in, AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const size_t base = m_playPos * kChannels;
            const float existingL = m_buffer[base];
            const float existingR = m_buffer[base + 1];
            out(i, 0) = existingL;
            out(i, 1) = existingR;
            m_buffer[base] = existingL * m_overdubDecay + in(i, 0);
            m_buffer[base + 1] = existingR * m_overdubDecay + in(i, 1);
            advancePlay();
        }
    }

    void advancePlay() noexcept
    {
        if (++m_playPos >= m_loopLengthFrames)
        {
            m_playPos = 0;
        }
    }

    static void silence(AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        silenceFrom(out, 0);
    }

    static void silenceFrom(AudioBuffer<kChannels, BlockSize>& out, const size_t start) noexcept
    {
        for (size_t i = start; i < BlockSize; ++i)
        {
            out(i, 0) = 0.f;
            out(i, 1) = 0.f;
        }
    }

    size_t m_maxFrames;
    std::vector<float> m_buffer;

    size_t m_samplesPerBar{0};
    float m_overdubDecay{1.f};

    LooperState m_state{LooperState::Empty};
    size_t m_recordedFrames{0};
    size_t m_loopLengthFrames{0};
    size_t m_playPos{0};
};

}
