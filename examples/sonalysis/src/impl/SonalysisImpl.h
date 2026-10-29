#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <span>
#include <stop_token>
#include <thread>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Helpers/SpscRingBuffer.h"
#include "SonalysisAnalysisEngine.h"
#include "SonalysisSnapshots.h"

/**
 * Measurement-only processor. The audio thread copies each block into a lock-free ring and
 * optionally passes it through; one worker thread drains the ring, runs the AnalysisEngine
 * and publishes snapshots under a mutex that only the worker and the GUI take. Settings
 * arrive as atomics from any thread and are applied by the worker. Frames that do not fit
 * into the ring are dropped and counted.
 */
template <size_t BlockSize>
class SonalysisImpl final : public EffectBase
{
  public:
    static constexpr float kRingSeconds{0.5f};
    static constexpr auto kPollInterval{std::chrono::milliseconds(5)};
    static constexpr auto kPeakFallRates = std::to_array<float>({6.f, 11.8f, 24.f, 48.f});
    static constexpr auto kRmsWindows = std::to_array<float>({0.05f, 0.3f, 1.f});
    static constexpr auto kPeakHolds = std::to_array<float>({0.f, 1.f, 3.f, std::numeric_limits<float>::infinity()});
    static constexpr auto kFftSizes = std::to_array<size_t>({2048, 4096, 8192, 16384});
    static constexpr auto kFftFallRates =
        std::to_array<float>({std::numeric_limits<float>::infinity(), 12.f, 24.f, 48.f});

    explicit SonalysisImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_ring(2 * static_cast<size_t>(kRingSeconds * sampleRate))
        , m_engine(sampleRate)
        , m_worker([this](const std::stop_token& stop) { run(stop); })
    {
    }

    void setPeakFall(const size_t index) noexcept
    {
        m_peakFall.store(pick(kPeakFallRates, index), std::memory_order_relaxed);
    }

    void setRmsWindow(const size_t index) noexcept
    {
        m_rmsWindow.store(pick(kRmsWindows, index), std::memory_order_relaxed);
    }

    void setPeakHold(const size_t index) noexcept
    {
        m_peakHold.store(pick(kPeakHolds, index), std::memory_order_relaxed);
    }

    void setHoldReset(const bool pressed) noexcept
    {
        if (pressed)
        {
            m_holdResetRequests.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void setFftSize(const size_t index) noexcept
    {
        m_fftSize.store(pick(kFftSizes, index), std::memory_order_relaxed);
    }

    void setFftFall(const size_t index) noexcept
    {
        m_fftFall.store(pick(kFftFallRates, index), std::memory_order_relaxed);
    }

    void setSpectrumPeakHold(const bool enabled) noexcept
    {
        m_spectrumPeakHold.store(enabled, std::memory_order_relaxed);
    }

    void setReferencePitch(const float hz) noexcept
    {
        m_referencePitch.store(hz, std::memory_order_relaxed);
    }

    void setPitchSource(const size_t index) noexcept
    {
        m_pitchSource.store(static_cast<Sonalysis::PitchSource>(std::min<size_t>(index, 2)), std::memory_order_relaxed);
    }

    void setPitchTrackingEnabled(const bool enabled) noexcept
    {
        m_pitchTracking.store(enabled, std::memory_order_relaxed);
    }

    void setBeatReset(const bool pressed) noexcept
    {
        if (pressed)
        {
            m_beatResetRequests.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void setThru(const bool enabled) noexcept
    {
        m_thru.store(enabled, std::memory_order_relaxed);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out) noexcept
    {
        const float thruGain = m_thru.load(std::memory_order_relaxed) ? 1.f : 0.f;
        std::array<float, 2 * BlockSize> interleaved{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            interleaved[2 * i] = in(i, 0);
            interleaved[2 * i + 1] = in(i, 1);
            out(i, 0) = in(i, 0) * thruGain;
            out(i, 1) = in(i, 1) * thruGain;
        }
        const size_t written = m_ring.push(interleaved);
        if (written < interleaved.size()) [[unlikely]]
        {
            m_droppedFrames.fetch_add((interleaved.size() - written) / 2, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] Sonalysis::LevelSnapshot levelSnapshot() const
    {
        const std::scoped_lock lock(m_publishMutex);
        return m_published.levels;
    }

    [[nodiscard]] Sonalysis::SpectrumSnapshot spectrumSnapshot() const
    {
        const std::scoped_lock lock(m_publishMutex);
        return m_published.spectrum;
    }

    [[nodiscard]] Sonalysis::PitchSnapshot pitchSnapshot() const
    {
        const std::scoped_lock lock(m_publishMutex);
        return m_published.pitch;
    }

    [[nodiscard]] Sonalysis::XySnapshot xySnapshot() const
    {
        const std::scoped_lock lock(m_publishMutex);
        return m_published.xy;
    }

    [[nodiscard]] Sonalysis::BeatSnapshot beatSnapshot() const
    {
        const std::scoped_lock lock(m_publishMutex);
        return m_published.beat;
    }

    [[nodiscard]] uint64_t droppedFrames() const noexcept
    {
        return m_droppedFrames.load(std::memory_order_relaxed);
    }

  private:
    struct Published
    {
        Sonalysis::LevelSnapshot levels{};
        Sonalysis::SpectrumSnapshot spectrum{};
        Sonalysis::PitchSnapshot pitch{};
        Sonalysis::XySnapshot xy{};
        Sonalysis::BeatSnapshot beat{};
    };

    template <typename T, size_t N>
    [[nodiscard]] static T pick(const std::array<T, N>& table, const size_t index) noexcept
    {
        return table[std::min(index, N - 1)];
    }

    [[nodiscard]] Sonalysis::Settings currentSettings() const noexcept
    {
        constexpr auto relaxed = std::memory_order_relaxed;
        return {m_peakFall.load(relaxed),       m_rmsWindow.load(relaxed),   m_peakHold.load(relaxed),
                m_fftSize.load(relaxed),        m_fftFall.load(relaxed),     m_spectrumPeakHold.load(relaxed),
                m_referencePitch.load(relaxed), m_pitchSource.load(relaxed), m_pitchTracking.load(relaxed)};
    }

    void applyResetRequests() noexcept
    {
        const auto holdResets = m_holdResetRequests.load(std::memory_order_relaxed);
        if (holdResets != m_holdResetsSeen)
        {
            m_holdResetsSeen = holdResets;
            m_engine.resetHold();
        }
        const auto beatResets = m_beatResetRequests.load(std::memory_order_relaxed);
        if (beatResets != m_beatResetsSeen)
        {
            m_beatResetsSeen = beatResets;
            m_engine.resetBeat();
        }
    }

    // Bounded by one ring's worth per pass so a starved worker still publishes.
    void drainRing()
    {
        std::array<float, 2 * Sonalysis::AnalysisEngine::kMaxChunkFrames> chunk{};
        for (size_t budget = m_ring.capacity(); budget > 0;)
        {
            const size_t count = m_ring.pop(chunk);
            if (count == 0)
            {
                return;
            }
            m_engine.analyse(std::span<const float>(chunk.data(), count));
            budget -= std::min(budget, count);
        }
    }

    void publish()
    {
        Published next{m_engine.levels(), m_engine.spectrum(), m_engine.pitch(), m_engine.xy(), m_engine.beat()};
        const std::scoped_lock lock(m_publishMutex);
        m_published = next;
    }

    void run(const std::stop_token& stop)
    {
        while (!stop.stop_requested())
        {
            m_engine.apply(currentSettings());
            applyResetRequests();
            drainRing();
            publish();
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    AbacDsp::SpscRingBuffer<float> m_ring;
    std::atomic<uint64_t> m_droppedFrames{0};
    std::atomic<bool> m_thru{false};
    std::atomic<float> m_peakFall{kPeakFallRates[1]};
    std::atomic<float> m_rmsWindow{kRmsWindows[1]};
    std::atomic<float> m_peakHold{kPeakHolds[2]};
    std::atomic<size_t> m_fftSize{kFftSizes[2]};
    std::atomic<float> m_fftFall{kFftFallRates[2]};
    std::atomic<bool> m_spectrumPeakHold{false};
    std::atomic<float> m_referencePitch{440.f};
    std::atomic<Sonalysis::PitchSource> m_pitchSource{Sonalysis::PitchSource::Mix};
    std::atomic<bool> m_pitchTracking{false};
    std::atomic<uint32_t> m_holdResetRequests{0};
    std::atomic<uint32_t> m_beatResetRequests{0};
    uint32_t m_holdResetsSeen{0};
    uint32_t m_beatResetsSeen{0};
    Sonalysis::AnalysisEngine m_engine;
    mutable std::mutex m_publishMutex;
    Published m_published{};
    // Last member: the thread starts after everything above exists and is joined first.
    std::jthread m_worker;
};
