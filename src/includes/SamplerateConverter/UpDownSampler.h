#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "Audio/FixedSizeProcessor.h"
#include "Filters/Sinc/sinc_7_128.h"
#include "Parameters/OctaveGlide.h"
#include "SamplerateConverter/SrPushConverter.h"

// TODO(JS): check if we need a Stereo or multitrack version (maybe with a template channelsize)


namespace AbacDsp
{

template <typename T>
concept SpanBlockProcessor = requires(T& processor, std::span<const float> source, std::span<float> target) {
    processor.processBlock(source, target);
};

/**
 * @ingroup srconverter
 * @brief Runs a mono processor at hostRate * ratio, with a live-settable ratio.
 *
 * Audio is down-converted by the ratio, re-blocked into FixedFrameSize chunks for the
 * wrapped Processor, and converted back. ratio > 1 oversamples, ratio < 1 undersamples.
 * The ratio is clamped to [1/16, 16] and it always converts, also at exactly 1.0.
 * setRatio() glides at 1/8000 octave per sample up and 1/16000 down unless forced.
 *
 * Latency is ratio dependent but continuous, and does not depend on the host block size.
 * The up stage uses the previous chunk's ratio, matching the data still in flight, and a
 * proportional correction keeps the output FIFO at its cushion while the ratio moves, so a
 * glide does not run it dry. A remaining dropout repeats the last sample, see underruns().
 * processBlock() never allocates, and source and target may alias.
 */
template <SpanBlockProcessor Processor, size_t FixedFrameSize>
class UpDownSampler
{
  public:
    static constexpr size_t kMaxFactor{16};
    static constexpr float kMaxRatio{static_cast<float>(kMaxFactor)};
    static constexpr float kMinRatio{1.f / kMaxRatio};

    template <typename... Args>
    explicit UpDownSampler(const size_t maxBlockSize, Args&&... processorArgs)
        : m_maxBlockSize(std::max<size_t>(maxBlockSize, 1))
        , m_processor(std::forward<Args>(processorArgs)...)
        , m_reblocker([this](const InternalBuffer& in, InternalBuffer& out) { runProcessor(in, out); })
        , m_sinc(std::make_shared<SincFilter>(init_7_128))
        , m_toInternal(m_sinc)
        , m_toHost(m_sinc)
        , m_internal(m_maxBlockSize * kMaxFactor + kHeadroom)
        , m_hostChunk(m_maxBlockSize + kHeadroom + kMaxFactor * FixedFrameSize)
        , m_fifo(m_hostChunk.size() + m_maxBlockSize + kFifoCushion)
    {
    }

    UpDownSampler(const UpDownSampler&) = delete;
    UpDownSampler& operator=(const UpDownSampler&) = delete;
    UpDownSampler(UpDownSampler&&) = delete;
    UpDownSampler& operator=(UpDownSampler&&) = delete;


    /// Sets the target ratio; the ratio glides to it unless force is set.
    void setRatio(const float ratio, const bool force = false) noexcept
    {
        assert(ratio >= kMinRatio);
        assert(ratio <= kMaxRatio);
        m_targetRatio = std::clamp(ratio, kMinRatio, kMaxRatio);
        m_glide.setTarget(m_targetRatio, force);
        if (force)
        {
            m_ratio = m_targetRatio;
        }
    }

    /// The target ratio last set.
    [[nodiscard]] float ratio() const noexcept
    {
        return m_targetRatio;
    }

    /// The ratio used for the most recent chunk, which trails the target during a glide.
    [[nodiscard]] float currentRatio() const noexcept
    {
        return m_ratio;
    }

    [[nodiscard]] Processor& processor() noexcept
    {
        return m_processor;
    }

    [[nodiscard]] const Processor& processor() const noexcept
    {
        return m_processor;
    }

    [[nodiscard]] size_t underruns() const noexcept
    {
        return m_underruns;
    }

    /// Clears the converters, re-blocker and FIFO, but not the wrapped processor.
    void reset()
    {
        m_toInternal.reset();
        m_toHost.reset();
        m_reblocker.reset();
        m_fifoRead = 0;
        m_fifoWrite = 0;
        m_fifoSize = 0;
        m_started = false;
        m_hasPreviousRatio = false;
        m_lastOut = 0.f;
    }

    void processBlock(std::span<const float> source, std::span<float> target) noexcept
    {
        assert(source.size() == target.size());
        const auto total = std::min(source.size(), target.size());
        for (size_t offset = 0; offset < total; offset += m_maxBlockSize)
        {
            const auto count = std::min(m_maxBlockSize, total - offset);
            processChunk(source.subspan(offset, count), target.subspan(offset, count));
        }
    }

  private:
    using InternalBuffer = AudioBuffer<1, FixedFrameSize>;

    class SpanView
    {
      public:
        SpanView(float* data, const size_t numSamples) noexcept
            : m_data(data)
            , m_numSamples(numSamples)
        {
        }

        [[nodiscard]] int getNumChannels() const noexcept
        {
            return 1;
        }

        [[nodiscard]] int getNumSamples() const noexcept
        {
            return static_cast<int>(m_numSamples);
        }

        [[nodiscard]] const float* getReadPointer(const int) const noexcept
        {
            return m_data;
        }

        [[nodiscard]] float* getWritePointer(const int) const noexcept
        {
            return m_data;
        }

      private:
        float* m_data;
        size_t m_numSamples;
    };

    static constexpr size_t kHeadroom{4096};
    static constexpr size_t kFifoCushion{16};
    /// Glide speed in octaves per host sample: 6 and 3 octaves per second at 48 kHz.
    static constexpr float kGlideUpOctavesPerSample{1.f / 8000.f};
    static constexpr float kGlideDownOctavesPerSample{1.f / 16000.f};
    /// Level correction: the error beyond the dead-band over kControlSettle, capped. A surplus
    /// only adds latency, so its dead-band is wider than that of a deficit.
    static constexpr float kControlDeficitDeadband{8.f};
    static constexpr float kControlSurplusDeadband{16.f};
    static constexpr float kControlSettle{128.f};
    static constexpr float kControlMaxCorrection{0.05f};

    void runProcessor(const InternalBuffer& in, InternalBuffer& out)
    {
        m_processor.processBlock(std::span<const float>{&in(0, 0), FixedFrameSize},
                                 std::span<float>{&out(0, 0), FixedFrameSize});
    }

    /// Proportional correction of the up-stage ratio that holds the FIFO at its cushion.
    /// Zero inside the dead-band and before the first output, so a constant ratio is untouched.
    [[nodiscard]] float levelCorrection() const noexcept
    {
        if (!m_started)
        {
            return 0.f;
        }
        const auto error = static_cast<float>(kFifoCushion) - static_cast<float>(m_fifoSize);
        const auto deadband = error > 0.f ? kControlDeficitDeadband : kControlSurplusDeadband;
        const auto beyond = std::max(std::abs(error) - deadband, 0.f);
        return std::clamp(std::copysign(beyond, error) / kControlSettle, -kControlMaxCorrection, kControlMaxCorrection);
    }

    void processChunk(const std::span<const float> source, const std::span<float> target) noexcept
    {
        m_ratio = m_glide.getValue(source.size());
        // The up stage lags by one chunk: the data still in flight was made at the previous ratio.
        const auto upRatio = m_hasPreviousRatio ? m_previousRatio : m_ratio;
        m_previousRatio = m_ratio;
        m_hasPreviousRatio = true;

        const auto internalCount =
            m_toInternal.fetchBlock(m_ratio, source.data(), source.size(), m_internal.data(), m_internal.size());

        SpanView view{m_internal.data(), internalCount};
        m_reblocker.processBlock(view);

        const auto hostCount = m_toHost.fetchBlock((1.f / upRatio) * (1.f + levelCorrection()), m_internal.data(),
                                                   internalCount, m_hostChunk.data(), m_hostChunk.size());
        pushFifo(std::span<const float>{m_hostChunk.data(), hostCount});
        popFifo(target);
    }

    void pushFifo(const std::span<const float> samples) noexcept
    {
        assert(m_fifoSize + samples.size() <= m_fifo.size());
        for (const float sample : samples)
        {
            if (m_fifoSize == m_fifo.size())
            {
                return;
            }
            m_fifo[m_fifoWrite] = sample;
            m_fifoWrite = m_fifoWrite + 1 == m_fifo.size() ? 0 : m_fifoWrite + 1;
            ++m_fifoSize;
        }
    }

    void readFifo(const std::span<float> out) noexcept
    {
        for (float& sample : out)
        {
            m_lastOut = m_fifo[m_fifoRead];
            sample = m_lastOut;
            m_fifoRead = m_fifoRead + 1 == m_fifo.size() ? 0 : m_fifoRead + 1;
        }
        m_fifoSize -= out.size();
    }

    void popFifo(const std::span<float> target) noexcept
    {
        if (!m_started)
        {
            const auto real =
                m_fifoSize > kFifoCushion ? std::min(m_fifoSize - kFifoCushion, target.size()) : size_t{0};
            const auto padding = target.size() - real;
            std::fill_n(target.begin(), padding, 0.f);
            readFifo(target.subspan(padding));
            m_started = real > 0;
            return;
        }
        const auto available = std::min(m_fifoSize, target.size());
        readFifo(target.first(available));
        if (available < target.size())
        {
            ++m_underruns;
            std::fill(target.begin() + static_cast<std::ptrdiff_t>(available), target.end(), m_lastOut);
        }
    }

    const size_t m_maxBlockSize;
    Processor m_processor;
    FixedSizeProcessor<1, FixedFrameSize, SpanView> m_reblocker;
    std::shared_ptr<SincFilter> m_sinc;
    SrPushConverter<1> m_toInternal;
    SrPushConverter<1> m_toHost;

    std::vector<float> m_internal;
    std::vector<float> m_hostChunk;
    std::vector<float> m_fifo;
    size_t m_fifoRead{0};
    size_t m_fifoWrite{0};
    size_t m_fifoSize{0};
    bool m_started{false};
    float m_lastOut{0.f};
    float m_ratio{1.f};
    float m_targetRatio{1.f};
    OctaveGlide m_glide{1.f, 1.f, kGlideUpOctavesPerSample, kGlideDownOctavesPerSample};
    float m_previousRatio{1.f};
    bool m_hasPreviousRatio{false};
    size_t m_underruns{0};
};

}
