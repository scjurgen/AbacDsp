#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "Audio/FixedSizeProcessor.h"
#include "Filters/Sinc/sinc_4.h"
#include "SamplerateConverter/SrPushConverter.h"

namespace AbacDsp
{

// Runs a fixed-size ProcessFunction at a constant internal sample rate regardless of the host's
// actual rate: audio is pushed through SrPushConverter (host -> internal), chunked into
// FixedFrameSize blocks for the ProcessFunction via FixedSizeProcessor, then pushed back
// (internal -> host) into a small FIFO so processBlock() always returns exactly as many host-rate
// samples as it was given. At hostSampleRate == kInternalSampleRate both converters are skipped
// entirely: the host buffer's own pointers are handed straight to FixedSizeProcessor, so the
// common case pays no resampling cost.
template <size_t Channels, size_t FixedFrameSize, typename ExternalBufferType>
class InternalRateNormalizingProcessor
{
  public:
    using InternalBuffer = AudioBuffer<Channels, FixedFrameSize>;
    using ProcessFunction = std::function<void(const InternalBuffer&, InternalBuffer&)>;

    static constexpr float kInternalSampleRate{48000.f};

    InternalRateNormalizingProcessor(const float hostSampleRate, ProcessFunction processFunc)
        : m_passthrough(std::equal_to<float>{}(hostSampleRate, kInternalSampleRate))
        , m_ratioToInternal(kInternalSampleRate / hostSampleRate)
        , m_ratioToHost(hostSampleRate / kInternalSampleRate)
        , m_fixedProcessor(std::move(processFunc))
        , m_toInternal(makeSincFilter())
        , m_toHost(makeSincFilter())
    {
    }

    void processBlock(ExternalBufferType& buffer)
    {
        const auto numChannels = std::min(static_cast<size_t>(buffer.getNumChannels()), Channels);
        const auto numSamples = static_cast<size_t>(buffer.getNumSamples());

        if (m_passthrough)
        {
            PlanarView view{readPointers(buffer, numChannels), writePointers(buffer, numChannels), numSamples};
            m_fixedProcessor.processBlock(view);
            return;
        }
        processWithResampling(buffer, numChannels, numSamples);
    }

  private:
    class PlanarView
    {
      public:
        PlanarView(const std::array<const float*, Channels>& readPtrs, const std::array<float*, Channels>& writePtrs,
                   const size_t numSamples)
            : m_readPtrs(readPtrs)
            , m_writePtrs(writePtrs)
            , m_numSamples(numSamples)
        {
        }

        [[nodiscard]] int getNumChannels() const noexcept
        {
            return static_cast<int>(Channels);
        }

        [[nodiscard]] int getNumSamples() const noexcept
        {
            return static_cast<int>(m_numSamples);
        }

        [[nodiscard]] const float* getReadPointer(const int channel) const noexcept
        {
            return m_readPtrs[static_cast<size_t>(channel)];
        }

        [[nodiscard]] float* getWritePointer(const int channel) const noexcept
        {
            return m_writePtrs[static_cast<size_t>(channel)];
        }

      private:
        std::array<const float*, Channels> m_readPtrs;
        std::array<float*, Channels> m_writePtrs;
        size_t m_numSamples;
    };

    [[nodiscard]] static std::shared_ptr<SincFilter> makeSincFilter()
    {
        return std::make_shared<SincFilter>(sinc4);
    }

    [[nodiscard]] static std::array<const float*, Channels> readPointers(const ExternalBufferType& buffer,
                                                                         const size_t numChannels) noexcept
    {
        std::array<const float*, Channels> ptrs{};
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            ptrs[ch] = buffer.getReadPointer(static_cast<int>(ch));
        }
        return ptrs;
    }

    [[nodiscard]] static std::array<float*, Channels> writePointers(ExternalBufferType& buffer,
                                                                    const size_t numChannels) noexcept
    {
        std::array<float*, Channels> ptrs{};
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            ptrs[ch] = buffer.getWritePointer(static_cast<int>(ch));
        }
        return ptrs;
    }

    static void interleave(const ExternalBufferType& buffer, const size_t numChannels, const size_t numSamples,
                           std::vector<float>& dest)
    {
        dest.resize(numSamples * Channels);
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            const auto* const src = buffer.getReadPointer(static_cast<int>(ch));
            for (size_t i = 0; i < numSamples; ++i)
            {
                dest[i * Channels + ch] = src[i];
            }
        }
    }

    static void deinterleave(const std::vector<float>& source, const size_t numSamples,
                             std::array<std::vector<float>, Channels>& dest)
    {
        for (size_t ch = 0; ch < Channels; ++ch)
        {
            dest[ch].resize(numSamples);
            for (size_t i = 0; i < numSamples; ++i)
            {
                dest[ch][i] = source[i * Channels + ch];
            }
        }
    }

    static void interleavePlanar(const std::array<std::vector<float>, Channels>& source, const size_t numSamples,
                                 std::vector<float>& dest)
    {
        dest.resize(numSamples * Channels);
        for (size_t ch = 0; ch < Channels; ++ch)
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
                dest[i * Channels + ch] = source[ch][i];
            }
        }
    }

    [[nodiscard]] static size_t convertedFrameHeadroom(const size_t numSamples, const float ratio) noexcept
    {
        return static_cast<size_t>(static_cast<float>(numSamples) * ratio) + 64;
    }

    void processWithResampling(ExternalBufferType& buffer, const size_t numChannels, const size_t numSamples)
    {
        interleave(buffer, numChannels, numSamples, m_hostIn);

        const auto internalMax = convertedFrameHeadroom(numSamples, m_ratioToInternal);
        m_internalIn.resize(internalMax * Channels);
        const auto internalGenerated =
            m_toInternal.fetchBlock(m_ratioToInternal, m_hostIn.data(), numSamples, m_internalIn.data(), internalMax);

        deinterleave(m_internalIn, internalGenerated, m_internalPlanar);

        std::array<const float*, Channels> readPtrs{};
        std::array<float*, Channels> writePtrs{};
        for (size_t ch = 0; ch < Channels; ++ch)
        {
            readPtrs[ch] = m_internalPlanar[ch].data();
            writePtrs[ch] = m_internalPlanar[ch].data();
        }
        PlanarView view{readPtrs, writePtrs, internalGenerated};
        m_fixedProcessor.processBlock(view);

        interleavePlanar(m_internalPlanar, internalGenerated, m_internalOut);

        const auto hostMax = convertedFrameHeadroom(internalGenerated, m_ratioToHost);
        m_hostOutChunk.resize(hostMax * Channels);
        const auto hostGenerated =
            m_toHost.fetchBlock(m_ratioToHost, m_internalOut.data(), internalGenerated, m_hostOutChunk.data(), hostMax);
        m_pendingHostOut.insert(m_pendingHostOut.end(), m_hostOutChunk.begin(),
                                m_hostOutChunk.begin() + static_cast<long>(hostGenerated * Channels));

        const auto available = m_pendingHostOut.size() / Channels;
        const auto toCopy = std::min(available, numSamples);
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* const dst = buffer.getWritePointer(static_cast<int>(ch));
            for (size_t i = 0; i < toCopy; ++i)
            {
                dst[i] = m_pendingHostOut[i * Channels + ch];
            }
            for (size_t i = toCopy; i < numSamples; ++i)
            {
                dst[i] = 0.f;
            }
        }
        m_pendingHostOut.erase(m_pendingHostOut.begin(),
                               m_pendingHostOut.begin() + static_cast<long>(toCopy * Channels));
    }

    const bool m_passthrough;
    const float m_ratioToInternal;
    const float m_ratioToHost;
    FixedSizeProcessor<Channels, FixedFrameSize, PlanarView> m_fixedProcessor;
    SrPushConverter<Channels> m_toInternal;
    SrPushConverter<Channels> m_toHost;

    std::vector<float> m_hostIn;
    std::vector<float> m_internalIn;
    std::array<std::vector<float>, Channels> m_internalPlanar;
    std::vector<float> m_internalOut;
    std::vector<float> m_hostOutChunk;
    std::vector<float> m_pendingHostOut;
};

}
