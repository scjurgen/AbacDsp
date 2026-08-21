#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include "Audio/AudioBuffer.h"

namespace AbacDsp
{

/// @ingroup sampler
/// @brief Transport state of the loop.
enum class LooperState : uint8_t
{
    Empty,       ///< No loop captured yet.
    Recording,   ///< Capturing the first loop.
    Playing,     ///< Looping playback.
    Overdubbing, ///< Playing while summing input into the loop.
    Stopped      ///< Has a loop, not playing.
};

/**
 * @ingroup sampler
 * @brief Stereo loop capture and playback with two ways of deciding where the loop ends.
 *
 * stopRecordFree() takes the loop length from where recording stopped;
 * stopRecordBarLocked() snaps it to the musical grid, which means the end can
 * land before or after the stop. Material recorded past a bar-locked end is not
 * discarded but folded back over the start, so a player who overshoots the
 * downbeat keeps the note they played.
 *
 * Every buffer, including the fold scratch, is sized at construction from the
 * maximum recording time, so nothing on the audio thread allocates. The scratch
 * covers half a bar at the slowest supported tempo.
 */
template <size_t BlockSize>
class LoopRecorder
{
  public:
    static constexpr size_t kChannels = 2;

    explicit LoopRecorder(const float sampleRate, const float maxSeconds = 180.f)
        : m_maxFrames(std::max<size_t>(1, static_cast<size_t>(sampleRate * maxSeconds)))
    {
        m_buffer.assign(m_maxFrames * kChannels, 0.f);
        m_overdubBuffer.assign(m_maxFrames * kChannels, 0.f);
        m_undoBuffer.assign(m_maxFrames * kChannels, 0.f);
        m_undoOverdubBuffer.assign(m_maxFrames * kChannels, 0.f);
        // Covers half a bar of relocate/fold at the slowest supported tempo.
        const size_t scratchFrames = std::max<size_t>(1, static_cast<size_t>(sampleRate * 8.f));
        m_postRollScratch.assign(scratchFrames * kChannels, 0.f);
        m_frontScratch.assign(scratchFrames * kChannels, 0.f);
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

    // Loop is exactly whatever was captured: no quantization, no relocate/fold.
    void stopRecordFree() noexcept
    {
        if (m_state != LooperState::Recording)
        {
            return;
        }
        finalizeFree();
    }

    // startOffset: tick minus trigger (positive = early start, relocated onto
    // the tail; negative = late start, gap backfilled from preRoll).
    void stopRecordBarLocked(const size_t loopLength, std::span<const float> preRoll, const long startOffset,
                             const size_t catchUpFrames) noexcept
    {
        if (m_state != LooperState::Recording)
        {
            return;
        }
        finalizeBarLocked(loopLength, preRoll, startOffset, catchUpFrames);
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
            finalizeFree();
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
        resetOverdub();
        m_state = LooperState::Empty;
        m_recordedFrames = 0;
        m_loopLengthFrames = 0;
        m_playPos = 0;
    }

    // Replaces the loop with externally supplied audio, bypassing record/finalize.
    void loadLoop(const std::span<const float> left, const std::span<const float> right) noexcept
    {
        const size_t frames = std::min({left.size(), right.size(), m_maxFrames});
        if (frames == 0)
        {
            clear();
            return;
        }
        resetOverdub();
        for (size_t frame = 0; frame < frames; ++frame)
        {
            m_buffer[frame * kChannels] = left[frame];
            m_buffer[frame * kChannels + 1] = right[frame];
        }
        m_recordedFrames = frames;
        m_loopLengthFrames = frames;
        m_playPos = 0;
        m_state = LooperState::Playing;
    }

    void loadOverdub(const std::span<const float> left, const std::span<const float> right) noexcept
    {
        const size_t frames = std::min({left.size(), right.size(), m_loopLengthFrames});
        if (frames == 0)
        {
            return;
        }
        for (size_t frame = 0; frame < frames; ++frame)
        {
            m_overdubBuffer[frame * kChannels] = left[frame];
            m_overdubBuffer[frame * kChannels + 1] = right[frame];
        }
        m_hasOverdub = true;
    }

    // Ends the take if called mid-overdub, same as endOverdub().
    void undoOverdub() noexcept
    {
        resetOverdub();
        if (m_state == LooperState::Overdubbing)
        {
            m_state = LooperState::Playing;
        }
    }

    // Ends the take if called mid-overdub, same as endOverdub().
    void mixDownOverdub() noexcept
    {
        for (size_t frame = 0; frame < m_loopLengthFrames; ++frame)
        {
            m_buffer[frame * kChannels] += m_overdubBuffer[frame * kChannels];
            m_buffer[frame * kChannels + 1] += m_overdubBuffer[frame * kChannels + 1];
        }
        resetOverdub();
        if (m_state == LooperState::Overdubbing)
        {
            m_state = LooperState::Playing;
        }
    }

    // Captures the current loop so a following record can be undone. Call
    // before beginRecord(); harmless (and correct) to call on an Empty loop,
    // since undoing a first-ever take should revert to Empty too.
    void snapshotForUndo() noexcept
    {
        m_undoLoopLengthFrames = m_loopLengthFrames;
        m_undoHasOverdub = m_hasOverdub;
        for (size_t frame = 0; frame < m_loopLengthFrames; ++frame)
        {
            m_undoBuffer[frame * kChannels] = m_buffer[frame * kChannels];
            m_undoBuffer[frame * kChannels + 1] = m_buffer[frame * kChannels + 1];
            m_undoOverdubBuffer[frame * kChannels] = m_overdubBuffer[frame * kChannels];
            m_undoOverdubBuffer[frame * kChannels + 1] = m_overdubBuffer[frame * kChannels + 1];
        }
        m_undoValid = true;
    }

    [[nodiscard]] bool hasUndoSnapshot() const noexcept
    {
        return m_undoValid;
    }

    // Restores whatever snapshotForUndo() last captured: aborts an
    // in-progress record, or reverts a just-finished one. One-shot, no redo.
    void undoRecord() noexcept
    {
        if (!m_undoValid)
        {
            return;
        }
        m_loopLengthFrames = m_undoLoopLengthFrames;
        m_hasOverdub = m_undoHasOverdub;
        for (size_t frame = 0; frame < m_loopLengthFrames; ++frame)
        {
            m_buffer[frame * kChannels] = m_undoBuffer[frame * kChannels];
            m_buffer[frame * kChannels + 1] = m_undoBuffer[frame * kChannels + 1];
            m_overdubBuffer[frame * kChannels] = m_undoOverdubBuffer[frame * kChannels];
            m_overdubBuffer[frame * kChannels + 1] = m_undoOverdubBuffer[frame * kChannels + 1];
        }
        m_recordedFrames = m_loopLengthFrames;
        m_playPos = 0;
        m_state = (m_loopLengthFrames > 0) ? LooperState::Playing : LooperState::Empty;
        m_undoValid = false;
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

    [[nodiscard]] bool hasOverdub() const noexcept
    {
        return m_hasOverdub;
    }

    [[nodiscard]] float overdubSample(const size_t frame, const size_t channel) const noexcept
    {
        return m_overdubBuffer[frame * kChannels + channel];
    }

    // Interleaved view over the finalized loop, for the slicer.
    [[nodiscard]] std::span<const float> loopView() const noexcept
    {
        return std::span<const float>{m_buffer.data(), m_loopLengthFrames * kChannels};
    }

  private:
    void finalizeFree() noexcept
    {
        if (m_recordedFrames == 0)
        {
            clear();
            return;
        }
        resetOverdub();
        m_loopLengthFrames = std::min(m_recordedFrames, m_maxFrames);
        applyBoundaryFades();
        m_playPos = 0;
        m_state = LooperState::Playing;
    }

    // See stopRecordBarLocked. No applyBoundaryFades() here: the fold already
    // handles the seam, and a plain taper would clobber real content there.
    void finalizeBarLocked(const size_t loopLength, std::span<const float> preRoll, const long startOffset,
                           const size_t catchUpFrames) noexcept
    {
        if (m_recordedFrames == 0)
        {
            clear();
            return;
        }
        resetOverdub();
        m_loopLengthFrames = std::min(loopLength, m_maxFrames);
        zeroTail(); // defensive: caller contract is recordedFrames >= loopLength

        const size_t shiftCap = std::min(m_loopLengthFrames / 2, m_frontScratch.size() / kChannels);
        const long shift = std::clamp(startOffset, -static_cast<long>(shiftCap), static_cast<long>(shiftCap));
        const size_t gap = (shift < 0) ? static_cast<size_t>(-shift) : 0;
        const size_t preRollFrames = std::min(preRoll.size() / kChannels, gap);
        const size_t earlyAvail = (shift > 0) ? std::min(static_cast<size_t>(shift), m_recordedFrames) : 0;

        const size_t overshoot = (m_recordedFrames > m_loopLengthFrames) ? m_recordedFrames - m_loopLengthFrames : 0;
        const size_t postAvail = std::min({overshoot, m_postRollScratch.size() / kChannels, m_loopLengthFrames});

        // Stash both regions before the relocate below can overwrite them.
        for (size_t i = 0; i < postAvail; ++i)
        {
            const size_t src = (m_loopLengthFrames + i) * kChannels;
            m_postRollScratch[i * kChannels] = m_buffer[src];
            m_postRollScratch[i * kChannels + 1] = m_buffer[src + 1];
        }
        for (size_t i = 0; i < earlyAvail; ++i)
        {
            m_frontScratch[i * kChannels] = m_buffer[i * kChannels];
            m_frontScratch[i * kChannels + 1] = m_buffer[i * kChannels + 1];
        }

        if (shift > 0)
        {
            // Early start: loop frame k = capture frame k+shift.
            const auto s = static_cast<size_t>(shift);
            for (size_t k = 0; k + s < m_recordedFrames && k < m_loopLengthFrames; ++k)
            {
                m_buffer[k * kChannels] = m_buffer[(k + s) * kChannels];
                m_buffer[k * kChannels + 1] = m_buffer[(k + s) * kChannels + 1];
            }
        }
        else if (gap > 0 && m_loopLengthFrames > gap)
        {
            // Late start: loop frame k = capture frame k-gap; [0, gap) is
            // backfilled directly from preRoll (placement, not a fold).
            for (size_t idx = m_loopLengthFrames - gap; idx-- > 0;)
            {
                m_buffer[(idx + gap) * kChannels] = m_buffer[idx * kChannels];
                m_buffer[(idx + gap) * kChannels + 1] = m_buffer[idx * kChannels + 1];
            }
            for (size_t k = 0; k < preRollFrames; ++k)
            {
                m_buffer[k * kChannels] = preRoll[k * kChannels];
                m_buffer[k * kChannels + 1] = preRoll[k * kChannels + 1];
            }
        }

        // Fold onto tail/front, full strength near the seam, fading outward.
        const auto fadeF = static_cast<float>(std::max<size_t>(m_fadeFrames, 1));
        for (size_t i = 0; i < earlyAvail; ++i)
        {
            const float gIn = (m_fadeFrames > 0) ? std::min(static_cast<float>(i + 1) / fadeF, 1.f) : 1.f;
            const size_t tail = (m_loopLengthFrames - earlyAvail + i) * kChannels;
            m_buffer[tail] += m_frontScratch[i * kChannels] * gIn;
            m_buffer[tail + 1] += m_frontScratch[i * kChannels + 1] * gIn;
        }
        for (size_t i = 0; i < postAvail; ++i)
        {
            const float gOut = (m_fadeFrames > 0) ? std::min(static_cast<float>(postAvail - i) / fadeF, 1.f) : 1.f;
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

    void resetOverdub() noexcept
    {
        for (size_t frame = 0; frame < m_loopLengthFrames; ++frame)
        {
            m_overdubBuffer[frame * kChannels] = 0.f;
            m_overdubBuffer[frame * kChannels + 1] = 0.f;
        }
        m_hasOverdub = false;
    }

    void recordBlock(const AudioBuffer<kChannels, BlockSize>& in, AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (m_recordedFrames >= m_maxFrames)
            {
                finalizeFree();
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
            const size_t base = m_playPos * kChannels;
            out(i, 0) = m_buffer[base] + m_overdubBuffer[base];
            out(i, 1) = m_buffer[base + 1] + m_overdubBuffer[base + 1];
            advancePlay();
        }
    }

    void overdubBlock(const AudioBuffer<kChannels, BlockSize>& in, AudioBuffer<kChannels, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const size_t base = m_playPos * kChannels;
            out(i, 0) = m_buffer[base] + m_overdubBuffer[base];
            out(i, 1) = m_buffer[base + 1] + m_overdubBuffer[base + 1];
            m_overdubBuffer[base] = m_overdubBuffer[base] * m_overdubDecay + in(i, 0);
            m_overdubBuffer[base + 1] = m_overdubBuffer[base + 1] * m_overdubDecay + in(i, 1);
            advancePlay();
        }
        m_hasOverdub = true;
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
    std::vector<float> m_overdubBuffer;
    std::vector<float> m_postRollScratch;
    std::vector<float> m_frontScratch;
    std::vector<float> m_undoBuffer;
    std::vector<float> m_undoOverdubBuffer;

    size_t m_fadeFrames{0};
    float m_overdubDecay{1.f};
    bool m_hasOverdub{false};
    size_t m_undoLoopLengthFrames{0};
    bool m_undoHasOverdub{false};
    bool m_undoValid{false};

    LooperState m_state{LooperState::Empty};
    size_t m_recordedFrames{0};
    size_t m_loopLengthFrames{0};
    size_t m_playPos{0};
};

}
