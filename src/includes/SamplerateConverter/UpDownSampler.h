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
#include "SamplerateConverter/SrPushConverter.h"

// TODO(JS): smooth ratio changes (abrupt jumps cause a short burst); a bool force flag
// would keep the current immediate change:
// void setRatio(const float ratio, const bool force=true)

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
 *
 * Latency is ratio dependent (re-blocker and sinc delay are counted in internal-rate
 * samples) but continuous. The output is silent until the output FIFO holds a small
 * cushion, which absorbs per-block count jitter; a large abrupt ratio change can still
 * underrun, which repeats the last sample and is counted by underruns().
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


    void setRatio(const float ratio) noexcept
    {
        assert(ratio >= kMinRatio);
        assert(ratio <= kMaxRatio);
        m_ratio = std::clamp(ratio, kMinRatio, kMaxRatio);
    }

    [[nodiscard]] float ratio() const noexcept
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

    void runProcessor(const InternalBuffer& in, InternalBuffer& out)
    {
        m_processor.processBlock(std::span<const float>{&in(0, 0), FixedFrameSize},
                                 std::span<float>{&out(0, 0), FixedFrameSize});
    }

    void processChunk(const std::span<const float> source, const std::span<float> target) noexcept
    {
        const auto internalCount =
            m_toInternal.fetchBlock(m_ratio, source.data(), source.size(), m_internal.data(), m_internal.size());

        SpanView view{m_internal.data(), internalCount};
        m_reblocker.processBlock(view);

        const auto hostCount = m_toHost.fetchBlock(1.f / m_ratio, m_internal.data(), internalCount, m_hostChunk.data(),
                                                   m_hostChunk.size());
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

    void popFifo(const std::span<float> target) noexcept
    {
        if (!m_started)
        {
            if (m_fifoSize < kFifoCushion + target.size())
            {
                std::ranges::fill(target, 0.f);
                return;
            }
            m_started = true;
        }
        const auto available = std::min(m_fifoSize, target.size());
        for (size_t i = 0; i < available; ++i)
        {
            m_lastOut = m_fifo[m_fifoRead];
            target[i] = m_lastOut;
            m_fifoRead = m_fifoRead + 1 == m_fifo.size() ? 0 : m_fifoRead + 1;
        }
        m_fifoSize -= available;
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
    size_t m_underruns{0};
};

}
