#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <random>
#include <vector>

#include "Helpers/ConstructArray.h"
#include "Modulation/Flutter.h"
#include "Modulation/Wow.h"
#include "Numbers/MultichannelInterpolation.h"
#include "Numbers/TimeDistanceSmoother.h"
#include "Parameters/SmoothingParameter.h"
#include "SamplerateConverter/SrPullConverter.h"
#include "SamplerateConverter/SrPushConverter.h"

namespace AbacDsp
{
/**
 * @ingroup delays
 * @brief organicchorus's own tape transport - a deliberate fork of VariSpeedTapeDelay, not a
 * shared class.
 *
 * The write clock only carries the base ratio plus an external (e.g. Ornstein-Uhlenbeck)
 * perturbation, so tape stays clean; Wow and Flutter modulate each read head's own advance
 * rate instead. `tapelooper` still uses VariSpeedTapeDelay, where Wow/Flutter modulate the
 * write clock and get printed into what's recorded - a fix to one class does not reach the
 * other.
 * @see https://en.wikipedia.org/wiki/Wow_and_flutter
 */
template <size_t BufferSize, size_t NumChannels, size_t NumReadHeads, size_t TileSize>
class OrganicChorusTransport
{
  public:
    /// Octaves per second when speeding up.
    static constexpr auto accelPerSec = 6.f;
    /// Octaves per second when slowing down, deliberately below accelPerSec.
    static constexpr auto brakePerSec = 3.f;
    using TapeInterpolation = MultichannelInterpolation<NumChannels>;

    /// Ratio range matches SrPullConverter's own supported range exactly (see setRatio()).
    static constexpr float kMinRatio = 1.f / 16.f;
    static constexpr float kMaxRatio = 16.f;

    // Sizes readBlock()'s low-rate reconstruction ring (see lowRateCallback()): one chunk per
    // SrPullConverter callback, ring large enough that a chunk from several tiles back can
    // never still be the most recent one SrPullConverter has a saved pointer into.
    static constexpr size_t kLowRateChunkFrames = 16;
    static constexpr size_t kLowRateRingFrames = TileSize * static_cast<size_t>(kMaxRatio) * 2;

    OrganicChorusTransport(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
        : m_sampleRate(sampleRate)
        , m_buffer((BufferSize + 6) * NumChannels, 0)
        , m_rdhd{constructArray<TimeDistanceSmoother<double>, NumReadHeads>(static_cast<double>(sampleRate))}
        , m_tmpOutput(static_cast<size_t>(sampleRate), 0)
        , m_srConverter(filterSet)
        , m_readConverters{constructArray<SrPullConverter, NumReadHeads>(filterSet)}
        , m_readReconstructionDelay(static_cast<float>(filterSet->halfCoeffWidth()) /
                                        static_cast<float>(filterSet->increment()) +
                                    static_cast<float>(kLowRateChunkFrames))
        , m_flutter(sampleRate / TileSize)
        , m_wow(sampleRate / TileSize)
    {
        for (size_t i = 0; i < NumReadHeads; ++i)
        {
            m_rdhd[i].setWrapPosition(BufferSize);
            m_rdhd[i].setCorrectionTime(.005f);
            setReadHead(i, static_cast<float>((i + 1) * 4800), true);
            m_lowRateRing[i].resize(kLowRateRingFrames * NumChannels, 0.f);
        }
        m_flutter.setRate(.4f);
        m_flutter.setDepth(0.1f);
        m_wow.setRate(0.4f);
        m_wow.setPerceptualDepth(0.1f);
        m_wow.setVariance(0.1f);
        m_wow.setDrift(0.05f);
    }

    // Catmull-Rom reconstructs the modulated read position only up to the tape's own native
    // rate; a per-tile SrPullConverter call resamples that up to TileSize's output rate, the
    // same split feed() already uses the other way. Wow/Flutter step once per call, at tile rate.
    void readBlock(const size_t hdIdx, std::array<float, NumChannels * TileSize>& out) noexcept
    {
        // 1.0, not m_lastFeedRatio: this now walks the tape at its own native pace, one buffer
        // sample per step (Wow/Flutter aside) - the transport ratio is applied below instead.
        m_rdhd[hdIdx].setCurrentWritePosition(static_cast<double>(m_writeHead), 1.0);
        const auto w = m_wow.step();
        const auto f = m_flutter.step();
        const auto rateMultiplier = static_cast<double>((1.0f + w) * f);

        auto cb = lowRateCallback(hdIdx, rateMultiplier);
        const auto pullRatio = std::clamp(static_cast<float>(1.0 / m_lastFeedRatio), kMinRatio, kMaxRatio);
        m_readConverters[hdIdx].fetchBlock(pullRatio, out.data(), TileSize, NumChannels, cb);
    }

    /// @brief Resamples one tile by the transport's own ratio and lays it onto the tape.
    /// Clean of Wow/Flutter by design here - see the class comment - output frame count still
    /// varies with the ratio, which is why the ring write is a separate step.
    void feed(const std::array<float, NumChannels * TileSize>& in) noexcept
    {
        const auto ratio = m_ratio.getValue(TileSize) * (1.0f + m_externalRatioPerturbation);
        m_lastFeedRatio = static_cast<double>(ratio);

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
    /// Compensates for the read reconstruction's own group delay (see m_readReconstructionDelay)
    /// so a caller's requested distance is what the reconstructed signal actually arrives at.
    void setReadHead(const size_t hdIdx, const float delta, const bool force = false) noexcept
    {
        const auto compensated = delta - m_readReconstructionDelay;
        const auto clampedDelta = std::clamp(compensated, m_readHeadSafetyMargin,
                                             static_cast<float>(BufferSize) - 1 - m_readHeadSafetyMargin);
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
        const auto ctRatio = std::clamp(targetRatio, kMinRatio, kMaxRatio);
        const auto last = std::clamp(m_ratio.getLastValue(), kMinRatio, kMaxRatio);
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

    // Write head position: a stable baseline for measuring read/write drift (see
    // OrganicChorusVoice_test.cpp's safety-margin measurements).
    [[nodiscard]] size_t writeHead() const noexcept
    {
        return m_writeHead;
    }

    /// @brief Current ring-buffer position of the given read head, in frames.
    [[nodiscard]] double readHead(const size_t hdIdx) const noexcept
    {
        return m_rdhd[hdIdx].getPosition();
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

    // Reseeds Wow's own random components, so multiple instances sharing the same rate
    // settings still drift independently rather than in lockstep.
    void seed(const std::mt19937::result_type value) noexcept
    {
        m_wow.seed(value);
    }

    // Raises the read-head drift-correction threshold past Wow/Flutter's own excursion,
    // so the intended modulation isn't corrected away as if it were drift.
    void setReadHeadCorrectionThreshold(const size_t hdIdx, const float samples) noexcept
    {
        m_rdhd[hdIdx].setDistanceCorrectionThreshold(samples);
    }

    // A caller-driven multiplicative ratio perturbation on the write side, combined the same
    // way the base ratio is - unlike setRatio(), this bypasses the accel/brake glide entirely,
    // so it actually reaches feed() instead of being damped out by continuous retargeting.
    void setExternalRatioPerturbation(const float value) noexcept
    {
        m_externalRatioPerturbation = value;
    }

    // How long a triggered correction takes to complete.
    void setReadHeadCorrectionTime(const size_t hdIdx, const float seconds) noexcept
    {
        m_rdhd[hdIdx].setCorrectionTime(seconds);
    }

    // Overrides the default 1000-frame read-head clamp margin (see setReadHead()) with one
    // sized to this caller's own, measured worst-case modulation excursion; floor is never
    // below m_readReconstructionDelay, the kernel's own structural minimum.
    void setReadHeadSafetyMargin(const float samples) noexcept
    {
        m_readHeadSafetyMargin = std::clamp(samples, m_readReconstructionDelay, static_cast<float>(BufferSize) / 2.f);
    }

  private:
    // Generates the next chunk of the modulated, tape-native-rate reconstruction (Catmull-Rom
    // over the raw ring buffer) into m_lowRateRing, and returns a callback handing that chunk
    // to SrPullConverter - see readBlock() and the class comment on kLowRateRingFrames.
    [[nodiscard]] std::function<long(float**, size_t)> lowRateCallback(const size_t hdIdx,
                                                                       const double rateMultiplier) noexcept
    {
        return [this, hdIdx, rateMultiplier](float** ptr, size_t) -> long
        {
            auto& ring = m_lowRateRing[hdIdx];
            auto& writePos = m_lowRateWritePos[hdIdx];
            const auto chunk = std::min(kLowRateChunkFrames, kLowRateRingFrames - writePos);
            for (size_t i = 0; i < chunk; ++i)
            {
                m_rdhd[hdIdx].advancePosition(rateMultiplier);
                // Hard runtime floor, re-checked every step: no static margin alone can prove
                // safety against an indefinitely-sustained Wow excursion (see Wow.h).
                if (m_rdhd[hdIdx].getCurrentDelta() < m_readReconstructionDelay)
                {
                    m_rdhd[hdIdx].forceReadPositionDistance(m_readReconstructionDelay);
                }
                const auto indexBuffer = static_cast<size_t>(std::floor(m_rdhd[hdIdx].getPosition()));
                // Subtract in double first, or a large tape position would already have lost
                // the sub-sample fraction before narrowing to float.
                const float fraction =
                    static_cast<float>(m_rdhd[hdIdx].getPosition() - static_cast<double>(indexBuffer));
                TapeInterpolation::catmullRom(&m_buffer[indexBuffer * NumChannels], &ring[(writePos + i) * NumChannels],
                                              fraction);
            }
            *ptr = &ring[writePos * NumChannels];
            writePos = (writePos + chunk) % kLowRateRingFrames;
            return static_cast<long>(chunk);
        };
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
    std::vector<float> m_buffer;
    LinearSmoothing m_ratio{1.f};
    // Exact ratio feed() last resampled at, including m_externalRatioPerturbation -
    // m_ratio.getLastValue() alone omits it, which is what readBlock() used to track instead.
    double m_lastFeedRatio{1.0};

    std::array<TimeDistanceSmoother<double>, NumReadHeads> m_rdhd;

    size_t m_writeHead{};

    const float* m_input{};
    size_t m_inputSize{0};
    std::vector<float> m_tmpOutput;
    SrPushConverter<NumChannels> m_srConverter;
    std::array<SrPullConverter, NumReadHeads> m_readConverters;
    // Read reconstruction's total added delay in tape-domain samples (kernel group delay plus
    // kLowRateChunkFrames, which adds 1:1); subtracted in setReadHead() to compensate for it.
    const float m_readReconstructionDelay;
    std::array<std::vector<float>, NumReadHeads> m_lowRateRing;
    std::array<size_t, NumReadHeads> m_lowRateWritePos{};
    Flutter m_flutter;
    Wow m_wow;
    float m_ratioTarget{1.f};
    float m_flutterRate{1.f};
    float m_wowRate{1.f};
    float m_readHeadSafetyMargin{1000.f};
    float m_externalRatioPerturbation{0.f};
};

}
