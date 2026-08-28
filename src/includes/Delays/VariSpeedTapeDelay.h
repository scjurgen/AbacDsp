#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

#include "Helpers/ConstructArray.h"
#include "Modulation/Flutter.h"
#include "Modulation/Wow.h"
#include "Numbers/MultichannelInterpolation.h"
#include "Numbers/TimeDistanceSmoother.h"
#include "Parameters/SmoothingParameter.h"
#include "SamplerateConverter/SrPushConverter.h"

namespace AbacDsp
{
/**
 * @ingroup delays
 * @brief Multi-head tape delay whose transport speed is a real, rate-limited variable.
 *
 * Unlike a delay that moves its read head, this varies the rate at which
 * material is laid down: input is resampled by the transport ratio before it
 * reaches the ring buffer, so the buffer holds tape, not samples. Read heads
 * then run at a fixed rate over it. Everything already recorded keeps the pitch
 * it was recorded at, which is what a real machine does and a read-head delay
 * cannot reproduce.
 *
 * Speed changes obey an exponential model: the time to reach a new ratio is
 * proportional to the number of octaves between them, so doubling always takes
 * the same time regardless of where the transport started. Braking is slower
 * than acceleration, as it is on a real transport with no active brake.
 *
 * Wow and flutter scale with the transport ratio, since both are geometry
 * defects of a moving mechanism rather than fixed-frequency modulations. The
 * buffer keeps six frames of wrap padding for the Catmull-Rom interpolator.
 * @see https://en.wikipedia.org/wiki/Wow_and_flutter
 */
template <size_t BufferSize, size_t NumChannels, size_t NumReadHeads, size_t TileSize>
class VariSpeedTapeDelay
{
    // TODO: no tape-defects noise coloring yet (dropout/hiss). A prior reference
    // implementation had one (pink noise shaped by a biquad); it was naive and
    // deliberately not ported. A proper model is still open.
  public:
    /// Octaves per second when speeding up.
    static constexpr auto accelPerSec = 6.f;
    /// Octaves per second when slowing down, deliberately below accelPerSec.
    static constexpr auto brakePerSec = 3.f;
    static constexpr auto NoisePeakQ = 0.1f;
    using TapeInterpolation = MultichannelInterpolation<NumChannels>;
    using DefectsInterpolation = MultichannelInterpolation<NumChannels>;

    VariSpeedTapeDelay(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
        : m_sampleRate(sampleRate)
        , m_buffer((BufferSize + 6) * NumChannels, 0)
        , m_rdhd{constructArray<TimeDistanceSmoother<double>, NumReadHeads>(static_cast<double>(sampleRate))}
        , m_tmpOutput(static_cast<size_t>(sampleRate), 0)
        , m_srConverter(filterSet)
        , m_flutter(sampleRate / TileSize)
        , m_wow(sampleRate / TileSize)
    {
        for (size_t i = 0; i < NumReadHeads; ++i)
        {
            m_rdhd[i].setWrapPosition(BufferSize);
            m_rdhd[i].setCorrectionTime(.005f);
            setReadHead(i, static_cast<float>((i + 1) * 4800), true);
        }
        m_flutter.setRate(.4f);
        m_flutter.setDepth(0.1f);
        m_wow.setRate(0.4f);
        m_wow.setPerceptualDepth(0.1f);
        m_wow.setVariance(0.1f);
        m_wow.setDrift(0.05f);
    }

    void readBlock(const size_t hdIdx, std::array<float, NumChannels * TileSize>& out) noexcept
    {
        m_rdhd[hdIdx].setCurrentWritePosition(static_cast<double>(m_writeHead), m_ratio.getLastValue());
        for (size_t i = 0; i < TileSize; ++i)
        {
            m_rdhd[hdIdx].advancePosition();

            const auto indexBuffer = static_cast<size_t>(std::floor(m_rdhd[hdIdx].getPosition()));
            // Subtract in double first, or a large tape position would already
            // have lost the sub-sample fraction before narrowing to float.
            const float fraction = static_cast<float>(m_rdhd[hdIdx].getPosition() - static_cast<double>(indexBuffer));
            std::array<float, NumChannels> tmp{};
            TapeInterpolation::catmullRom(&m_buffer[indexBuffer * NumChannels], tmp.data(), fraction);
            for (size_t c = 0; c < NumChannels; ++c)
            {
                out[i * NumChannels + c] = tmp[c];
            }
        }
    }

    /// @brief Resamples one tile by the current transport ratio and lays it onto the tape.
    /// Output frame count varies with the ratio, which is why the ring write is a separate step.
    void feed(const std::array<float, NumChannels * TileSize>& in) noexcept
    {
        const auto w = m_wow.step();
        const auto f = m_flutter.step();
        const auto ratio = m_ratio.getValue(TileSize) * (1.0f + w) * f;

        m_input = in.data();
        m_inputSize = TileSize;
        const auto producedFrames =
            m_srConverter.fetchBlock(ratio, m_input, TileSize, m_tmpOutput.data(), m_tmpOutput.size());
        if (producedFrames > 0)
        {
            writeToRingBuffer(m_tmpOutput.data(), producedFrames);
        }
    }

    /// @brief Moves one head to delta frames behind the write head, gliding unless forced.
    /// Kept 1000 frames clear of both ends so wow and flutter cannot push it past the write head.
    void setReadHead(const size_t hdIdx, const float delta, const bool force = false) noexcept
    {
        constexpr float MaxModulationSafety{1000.f};
        const auto clampedDelta =
            std::clamp(delta, MaxModulationSafety, static_cast<float>(BufferSize) - 1 - MaxModulationSafety);
        if (force)
        {
            m_rdhd[hdIdx].forceReadPositionDistance(clampedDelta);
        }
        else
        {
            m_rdhd[hdIdx].newTargetDistance(clampedDelta, 0.5f);
        }
    }

    /// @brief Sets the transport speed, ratio 1.0 being nominal, reached over a rate-limited glide.
    /// Transition time is octaves-to-travel over accelPerSec or brakePerSec, so it is speed-independent.
    void setRatio(const float targetRatio, const bool force = false) noexcept
    {
        const auto ctRatio = std::clamp(targetRatio, 0.001f, 8.f);
        const auto last = std::clamp(m_ratio.getLastValue(), 0.001f, 8.f);
        const auto delta = std::abs(std::log2(ctRatio / last));
        const auto rate = ctRatio > last ? accelPerSec : brakePerSec;
        const auto transitionTime = delta / rate;
        m_ratio.newTransition(ctRatio, transitionTime, m_sampleRate, force);
        m_ratioTarget = ctRatio;
        m_wow.setRate(m_ratioTarget * m_wowRate);
        m_flutter.setRate(m_ratioTarget * m_flutterRate);
    }

    void reset() noexcept
    {
        std::ranges::fill(m_buffer, 0);
        std::ranges::fill(m_tmpOutput, 0);
    }

    [[nodiscard]] const std::vector<float>& getBuffer() const noexcept
    {
        return m_buffer;
    }

    // Write head position: a stable baseline for a caller (e.g. tapelooper's chunked loop
    // save) extracting the loop's audio across many calls without racing feed().
    [[nodiscard]] size_t writeHead() const noexcept
    {
        return m_writeHead;
    }

    /// @brief Current ring-buffer position of the given read head, in frames.
    [[nodiscard]] double readHead(const size_t hdIdx) const noexcept
    {
        return m_rdhd[hdIdx].getPosition();
    }

    // Copies the loopFrames frames currently comprising the loop - the ones most recently
    // written, ending at the write head - into out, wraparound-aware. loopFrames must not
    // exceed BufferSize; out must hold at least loopFrames * NumChannels samples.
    void extractLoop(const size_t loopFrames, const std::span<float> out) const noexcept
    {
        const auto frames = std::min(loopFrames, BufferSize);
        assert(out.size() >= frames * NumChannels);
        const auto start = (m_writeHead + BufferSize - frames) % BufferSize;
        copyWrapped(start, out, frames);
    }

    // Writes one chunk of a chunked loadLoop() sequence at ring-buffer offset chunkStart
    // (frames, from the start of a fresh loop); does not touch the write head - call
    // finishLoopLoad() once every chunk has been written.
    void installLoopChunk(const size_t chunkStart, const std::span<const float> chunkData) noexcept
    {
        const auto chunkFrames = chunkData.size() / NumChannels;
        assert(chunkStart + chunkFrames <= BufferSize);
        std::copy_n(chunkData.begin(), chunkFrames * NumChannels,
                    m_buffer.begin() + static_cast<std::ptrdiff_t>(chunkStart * NumChannels));
    }

    // Completes a chunked load: replicates writeToRingBuffer()'s own wrap-padding duplication
    // for the first 6 frames, then positions the write head so feed()/recording continues
    // seamlessly right after the loaded content.
    void finishLoopLoad(const size_t frames) noexcept
    {
        const auto count = std::min(frames, BufferSize);
        for (size_t i = 0; i < std::min<size_t>(count, 6); ++i)
        {
            std::copy_n(&m_buffer[i * NumChannels], NumChannels, &m_buffer[(i + BufferSize) * NumChannels]);
        }
        m_writeHead = count % BufferSize;
    }

    // Installs previously extracted loop audio in one call - a thin single-chunk wrapper over
    // installLoopChunk()/finishLoopLoad(), for a caller not chunking the write across blocks.
    void loadLoop(const std::span<const float> interleaved, const size_t frames) noexcept
    {
        const auto count = std::min(frames, BufferSize);
        assert(interleaved.size() >= count * NumChannels);
        installLoopChunk(0, interleaved.subspan(0, count * NumChannels));
        finishLoopLoad(count);
    }

    void setFlutterDepth(const float value) noexcept
    {
        m_flutter.setDepth(value);
    }

    void setFlutterRate(const float value) noexcept
    {
        m_flutterRate = value;
        m_flutter.setRate(m_ratioTarget * value);
    }

    void setWowDepth(const float value) noexcept
    {
        m_wow.setPerceptualDepth(value);
    }

    void setWowRate(const float value) noexcept
    {
        m_wowRate = value;
        m_wow.setRate(m_ratioTarget * value);
    }

    void setWowVariance(const float value) noexcept
    {
        m_wow.setVariance(value);
    }

    void setWowDrift(const float value) noexcept
    {
        m_wow.setDrift(value);
    }

  private:
    /// @brief Appends frames at the write head, taking a bulk copy when the run does not wrap.
    /// The first six frames are duplicated past the end so the interpolator can read across the seam.
    // Copies frames frames starting at ring position start (wrapping at BufferSize) into out,
    // as at most two contiguous runs - avoids a per-frame modulo for a potentially large copy.
    void copyWrapped(const size_t start, const std::span<float> out, const size_t frames) const noexcept
    {
        const auto firstRun = std::min(frames, BufferSize - start);
        std::copy_n(&m_buffer[start * NumChannels], firstRun * NumChannels, out.data());
        if (firstRun < frames)
        {
            std::copy_n(&m_buffer[0], (frames - firstRun) * NumChannels, out.data() + firstRun * NumChannels);
        }
    }

    void writeToRingBuffer(const float* data, const size_t frames) noexcept
    {
        if (m_writeHead >= 6)
        {
            if (const auto availableFrames = std::min(BufferSize - m_writeHead, frames); availableFrames >= frames)
            {
                std::copy_n(data, availableFrames * NumChannels,
                            m_buffer.begin() + static_cast<std::ptrdiff_t>(m_writeHead * NumChannels));
                m_writeHead += availableFrames;
                return;
            }
        }
        for (size_t i = 0; i < frames; ++i)
        {
            for (size_t c = 0; c < NumChannels; ++c)
            {
                m_buffer[m_writeHead * NumChannels + c] = data[i * NumChannels + c];
                if (m_writeHead < 6)
                {
                    m_buffer[(m_writeHead + BufferSize) * NumChannels + c] = data[i * NumChannels + c];
                }
            }
            m_writeHead++;
            if (m_writeHead >= BufferSize)
            {
                m_writeHead = 0;
            }
        }
    }

    const float m_sampleRate;

  protected:
    std::vector<float> m_buffer;

  private:
    LinearSmoothing m_ratio{1.f};

    std::array<TimeDistanceSmoother<double>, NumReadHeads> m_rdhd;

    size_t m_writeHead{};

    const float* m_input{};
    size_t m_inputSize{0};
    std::vector<float> m_tmpOutput;
    SrPushConverter<NumChannels> m_srConverter;
    Flutter m_flutter;
    Wow m_wow;
    float m_ratioTarget{1.f};
    float m_flutterRate{1.f};
    float m_wowRate{1.f};
};

}
