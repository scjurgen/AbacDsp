#pragma once

#include <algorithm>
#include <cstddef>
#include <random>
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

    OrganicChorusTransport(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
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

    // Wow/Flutter are stepped once per call, at tile rate (they are constructed at
    // sampleRate/TileSize) - correct only when each tile calls readBlock() exactly once per
    // head, true for every current caller (one head per voice).
    void readBlock(const size_t hdIdx, std::array<float, NumChannels * TileSize>& out) noexcept
    {
        m_rdhd[hdIdx].setCurrentWritePosition(static_cast<double>(m_writeHead), m_lastFeedRatio);
        const auto w = m_wow.step();
        const auto f = m_flutter.step();
        const auto rateMultiplier = static_cast<double>((1.0f + w) * f);
        for (size_t i = 0; i < TileSize; ++i)
        {
            m_rdhd[hdIdx].advancePosition(rateMultiplier);

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
    /// Kept m_readHeadSafetyMargin frames clear of both ends so Wow/Flutter cannot push
    /// it past the write head; see setReadHeadSafetyMargin() for sizing that margin.
    void setReadHead(const size_t hdIdx, const float delta, const bool force = false) noexcept
    {
        const auto clampedDelta =
            std::clamp(delta, m_readHeadSafetyMargin, static_cast<float>(BufferSize) - 1 - m_readHeadSafetyMargin);
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
    // sized to this caller's own, measured worst-case modulation excursion instead.
    void setReadHeadSafetyMargin(const float samples) noexcept
    {
        m_readHeadSafetyMargin = std::clamp(samples, 8.f, static_cast<float>(BufferSize) / 2.f);
    }

  private:
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
    Flutter m_flutter;
    Wow m_wow;
    float m_ratioTarget{1.f};
    float m_flutterRate{1.f};
    float m_wowRate{1.f};
    float m_readHeadSafetyMargin{1000.f};
    float m_externalRatioPerturbation{0.f};
};

}
