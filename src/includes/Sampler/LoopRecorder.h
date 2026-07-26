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

// Stereo loop capture/playback with a beat-quantized loop length. All storage is
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
        // Generous fixed capacity for the post-/front-roll staging areas used
        // by stopRecordBeatLocked() (Phase 8b): comfortably covers an eighth
        // note even at the slowest supported tempo, preallocated to avoid an
        // audio-thread allocation at record-stop.
        const size_t scratchFrames = std::max<size_t>(1, static_cast<size_t>(sampleRate));
        m_postRollScratch.assign(scratchFrames * kChannels, 0.f);
        m_frontScratch.assign(scratchFrames * kChannels, 0.f);
    }

    void setSamplesPerBeat(const size_t samplesPerBeat) noexcept
    {
        m_samplesPerBeat = samplesPerBeat;
    }

    // Linear fade-in over the first N frames and fade-out over the last N of the
    // finalized loop, applied at record-stop so the wrap seam (end -> start) is
    // click-free. Clamped to half the loop length for short loops.
    void setFadeFrames(const size_t fadeFrames) noexcept
    {
        m_fadeFrames = fadeFrames;
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

    // Phase 8b beat-locked finalize: `loopLength` is the exact, already
    // beat-quantized length (the caller computes it as the distance between two
    // beat ticks; see PLAN.md Phase 8b for the derivation, `y[k] = x[s+k]`).
    // Recording always starts the instant the trigger fires -- nothing waits
    // for the tick -- so this take's own frame 0 (call it `trig`) can land
    // either side of the tick `s`. `startOffset = s - trig`: positive means
    // the tick was still ahead of the trigger (an early/pickup start: frames
    // `[0, startOffset)` of this capture are real audio from *before* the
    // tick and get relocated onto the tail instead); negative means the tick
    // had already passed (a late start: the `-startOffset` frames between the
    // tick and the trigger were never captured here and are backfilled).
    // `preRoll` spans `[s-rollFrames, trig)` -- length `rollFrames -
    // startOffset`, clamped to `[0, 2*rollFrames]` -- captured by the caller
    // from a continuously-running ring buffer at the moment recording starts
    // (never later: the ring buffer's history is bounded). `recordedFrames()`
    // must already reach `loopLength + rollFrames - min(startOffset, 0)` --
    // the caller keeps this take in the Recording state until the post-roll
    // has actually happened, never synthesized. `catchUpFrames` is how far
    // real time has already moved past the stop tick by the time this actually
    // commits (post-roll wait plus block-boundary slop); played back from that
    // offset instead of frame 0, so playback stays phase-locked to the beat.
    void stopRecordBeatLocked(const size_t loopLength, std::span<const float> preRoll, const long startOffset,
                              const size_t rollFrames, const size_t catchUpFrames) noexcept
    {
        if (m_state != LooperState::Recording)
        {
            return;
        }
        finalizeBeatLocked(loopLength, preRoll, startOffset, rollFrames, catchUpFrames);
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

    // Replaces the loop with externally supplied audio, bypassing record/finalize.
    void loadLoop(const std::span<const float> left, const std::span<const float> right,
                  const size_t samplesPerBeat) noexcept
    {
        const size_t frames = std::min({left.size(), right.size(), m_maxFrames});
        if (frames == 0)
        {
            clear();
            return;
        }
        for (size_t frame = 0; frame < frames; ++frame)
        {
            m_buffer[frame * kChannels] = left[frame];
            m_buffer[frame * kChannels + 1] = right[frame];
        }
        m_samplesPerBeat = samplesPerBeat;
        m_recordedFrames = frames;
        m_loopLengthFrames = frames;
        m_playPos = 0;
        m_state = LooperState::Playing;
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
        m_loopLengthFrames = std::min(quantizeToBeat(m_recordedFrames), m_maxFrames);
        zeroTail();
        applyBoundaryFades();
        m_playPos = 0;
        m_state = LooperState::Playing;
    }

    [[nodiscard]] size_t quantizeToBeat(const size_t frames) const noexcept
    {
        if (m_samplesPerBeat == 0)
        {
            return frames;
        }
        size_t beats = (frames + m_samplesPerBeat / 2) / m_samplesPerBeat;
        if (beats == 0)
        {
            beats = 1;
        }
        return beats * m_samplesPerBeat;
    }

    // See stopRecordBeatLocked. Replaces applyBoundaryFades() for this path: a
    // plain amplitude taper to ~0 at the physical edges would be exactly wrong
    // once real pre-/post-roll content belongs there at full volume.
    void finalizeBeatLocked(const size_t loopLength, std::span<const float> preRoll, const long startOffset,
                            const size_t rollFramesIn, const size_t catchUpFrames) noexcept
    {
        if (m_recordedFrames == 0)
        {
            clear();
            return;
        }
        m_loopLengthFrames = std::min(loopLength, m_maxFrames);
        const size_t roll = std::min({rollFramesIn, m_loopLengthFrames / 2, m_postRollScratch.size() / kChannels,
                                      m_frontScratch.size() / kChannels});
        const long shift = std::clamp(startOffset, -static_cast<long>(roll), static_cast<long>(roll));
        const size_t preRollFrames = preRoll.size() / kChannels; // caller contract: roll - shift

        // Stash whatever the shift below is about to overwrite before it can:
        // the post-roll tail (always -- the front fold needs it regardless of
        // direction) and, for an early start, the redundant pre-tick frames at
        // the very front (the tail fold needs those instead of a ring-buffer
        // snapshot for that portion, since they are real audio this take
        // already captured itself).
        const size_t postAvail = std::min(roll, m_recordedFrames);
        for (size_t i = 0; i < postAvail; ++i)
        {
            const size_t src = (m_recordedFrames - postAvail + i) * kChannels;
            m_postRollScratch[i * kChannels] = m_buffer[src];
            m_postRollScratch[i * kChannels + 1] = m_buffer[src + 1];
        }
        const size_t earlyAvail = (shift > 0) ? std::min(static_cast<size_t>(shift), m_recordedFrames) : 0;
        for (size_t i = 0; i < earlyAvail; ++i)
        {
            m_frontScratch[i * kChannels] = m_buffer[i * kChannels];
            m_frontScratch[i * kChannels + 1] = m_buffer[i * kChannels + 1];
        }

        if (shift > 0)
        {
            // Early start: loop frame k = capture frame k+shift. Forward
            // iteration is safe (the source index is always ahead of the
            // destination, so a not-yet-read frame is never clobbered).
            const auto s = static_cast<size_t>(shift);
            for (size_t k = 0; k + s < m_recordedFrames && k < m_loopLengthFrames; ++k)
            {
                m_buffer[k * kChannels] = m_buffer[(k + s) * kChannels];
                m_buffer[k * kChannels + 1] = m_buffer[(k + s) * kChannels + 1];
            }
        }
        else if (shift < 0)
        {
            // Late start: loop frame k = capture frame k-gap for k >= gap;
            // backward iteration avoids clobbering. [0, gap) is backfilled
            // from preRoll below (real audio spanning the tick, captured
            // before this take's own frame 0).
            const auto gap = static_cast<size_t>(-shift);
            if (gap > 0 && m_loopLengthFrames > gap)
            {
                for (size_t idx = m_loopLengthFrames - gap; idx-- > 0;)
                {
                    m_buffer[(idx + gap) * kChannels] = m_buffer[idx * kChannels];
                    m_buffer[(idx + gap) * kChannels + 1] = m_buffer[idx * kChannels + 1];
                }
            }
            for (size_t k = 0; k < gap && k < m_loopLengthFrames && k < preRollFrames; ++k)
            {
                m_buffer[k * kChannels] = preRoll[(roll + k) * kChannels];
                m_buffer[k * kChannels + 1] = preRoll[(roll + k) * kChannels + 1];
            }
        }

        // Fold the pre-roll window onto the tail and the post-roll onto the
        // front, each fading in/out at its far edge (away from the beat) and
        // full volume near it. Additive: in the base-take case the
        // destination is otherwise silence, so in practice this is
        // placement, not a mix. The tail-fold source is preRoll for the part
        // before this take's own frame 0, and the stashed early frames for
        // whatever's left (early start only -- see PLAN.md Phase 8b).
        const size_t fade = std::min(m_fadeFrames, roll);
        const auto fadeF = static_cast<float>(std::max<size_t>(fade, 1));
        for (size_t i = 0; i < roll; ++i)
        {
            float tailL = 0.f;
            float tailR = 0.f;
            if (i < preRollFrames)
            {
                tailL = preRoll[i * kChannels];
                tailR = preRoll[i * kChannels + 1];
            }
            else if (i - preRollFrames < earlyAvail)
            {
                const size_t j = (i - preRollFrames) * kChannels;
                tailL = m_frontScratch[j];
                tailR = m_frontScratch[j + 1];
            }
            const float gIn = (fade > 0) ? std::min(static_cast<float>(i + 1) / fadeF, 1.f) : 1.f;
            const size_t tail = (m_loopLengthFrames - roll + i) * kChannels;
            m_buffer[tail] += tailL * gIn;
            m_buffer[tail + 1] += tailR * gIn;

            if (i >= postAvail)
            {
                continue;
            }
            const float gOut = (fade > 0) ? std::min(static_cast<float>(roll - i) / fadeF, 1.f) : 1.f;
            const size_t front = i * kChannels;
            m_buffer[front] += m_postRollScratch[i * kChannels] * gOut;
            m_buffer[front + 1] += m_postRollScratch[i * kChannels + 1] * gOut;
        }

        m_playPos = (m_loopLengthFrames > 0) ? (catchUpFrames % m_loopLengthFrames) : 0;
        m_state = LooperState::Playing;
    }

    // Mirrors SlicePlayer's edge-fade convention: gain = min(fadeIn, fadeOut),
    // clamped [0,1], reaching exactly 1 at the frame just inside each fade window.
    void applyBoundaryFades() noexcept
    {
        if (m_fadeFrames == 0 || m_loopLengthFrames == 0)
        {
            return;
        }
        const size_t fade = std::min(m_fadeFrames, m_loopLengthFrames / 2);
        if (fade == 0)
        {
            return;
        }
        const auto fadeF = static_cast<float>(fade);
        for (size_t k = 0; k < fade; ++k)
        {
            const float gain = static_cast<float>(k + 1) / fadeF;
            const size_t headFrame = k;
            const size_t tailFrame = m_loopLengthFrames - 1 - k;
            m_buffer[headFrame * kChannels] *= gain;
            m_buffer[headFrame * kChannels + 1] *= gain;
            m_buffer[tailFrame * kChannels] *= gain;
            m_buffer[tailFrame * kChannels + 1] *= gain;
        }
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
    std::vector<float> m_postRollScratch;
    std::vector<float> m_frontScratch;

    size_t m_samplesPerBeat{0};
    size_t m_fadeFrames{0};
    float m_overdubDecay{1.f};

    LooperState m_state{LooperState::Empty};
    size_t m_recordedFrames{0};
    size_t m_loopLengthFrames{0};
    size_t m_playPos{0};
};

}
