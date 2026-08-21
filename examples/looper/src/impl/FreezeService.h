#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "Analysis/FftMisc.h"
#include "Analysis/Slicer.h"
#include "Sampler/LoopPartBank.h"
#include "Sampler/SliceLibrary.h"

// Manual "freeze": slices the current loop into transient-aligned candidates
// plus their waveform thumbnails, on a background worker (Slicer::adaptiveTransientSlices
// allocates and runs an STFT, never acceptable on the audio thread), via the
// same generation-counter handshake as LoopStorageService.
//
// This service only analyzes; installing a completed freeze's result into a
// new SliceLibrary track and rebuilding the sequencer pattern is the caller's
// job -- see pollCompletion() and LooperImpl::checkFreezeCompletion().
template <size_t BlockSize>
class FreezeService
{
  public:
    // Onset-detection snap grid (16ths) used when slicing a frozen loop; the
    // pattern itself is sample-accurate (see LooperImpl::rebuildPatternForCurrentLoop()),
    // not on this grid.
    static constexpr size_t kOnsetSnapStepsPerBeat = 4;
    static constexpr size_t kThumbFftLength = 2 * AbacDsp::SliceLibrary::kThumbHeight;

    struct FreezeResult
    {
        std::vector<AbacDsp::Slice> slices;
        std::vector<float> thumbnails; // one kThumbFloats block per slice, same order
    };

    explicit FreezeService(const AbacDsp::LoopPartBank<BlockSize>& bank)
        : m_bank(bank)
        , m_freezeFft(kThumbFftLength)
    {
        m_freezeThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_freezeWaitMutex);
                    m_freezeCv.wait(lock, stopToken, [this, lastHandled]
                                    { return m_freezeRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_freezeRequestGen.load(std::memory_order_acquire);
                    runFreezeAnalysis(lastHandled);
                }
            });
    }

    // Refused while already pending; guarded by the caller (recording/overdub/
    // empty-loop checks). samplesPerBeat sizes the onset-snap grid.
    void requestFreeze(const size_t samplesPerBeat)
    {
        if (m_freezePending)
        {
            return;
        }
        m_freezeSamplesPerBeat = samplesPerBeat;
        m_freezePendingGen = m_freezeRequestGen.load(std::memory_order_relaxed) + 1;
        m_freezePending = true;
        m_freezeRequestGen.store(m_freezePendingGen, std::memory_order_release);
        m_freezeCv.notify_one();
    }

    [[nodiscard]] bool isPending() const noexcept
    {
        return m_freezePending;
    }

    // Audio thread, polled every block: returns the analysis result once the
    // worker's done-generation catches up (nullopt while still pending).
    [[nodiscard]] std::optional<FreezeResult> pollCompletion()
    {
        if (!m_freezePending || m_freezeDoneGen.load(std::memory_order_acquire) != m_freezePendingGen)
        {
            return std::nullopt;
        }
        m_freezePending = false;
        FreezeResult result;
        result.slices = std::move(m_freezeResultSlices);
        result.thumbnails = std::move(m_freezeResultThumbnails);
        return result;
    }

  private:
    // Runs on m_freezeThread only: Slicer::adaptiveTransientSlices allocates and
    // runs an STFT, never acceptable on the audio thread.
    void runFreezeAnalysis(const uint64_t gen)
    {
        const auto loop = m_bank.active().loopView();
        const size_t loopLen = m_bank.active().loopLengthFrames();
        if (loopLen == 0)
        {
            m_freezeResultSlices.clear();
            m_freezeResultThumbnails.clear();
            m_freezeDoneGen.store(gen, std::memory_order_release);
            return;
        }
        AbacDsp::Slicer::downmixToMono(loop, loopLen, m_freezeMono);
        const size_t stepFrames = (m_freezeSamplesPerBeat == 0)
                                      ? loopLen
                                      : std::max<size_t>(1, m_freezeSamplesPerBeat / kOnsetSnapStepsPerBeat);
        const auto grid = AbacDsp::Slicer::gridBoundaries(loopLen, stepFrames);
        m_freezeResultSlices = AbacDsp::Slicer::adaptiveTransientSlices(
            m_freezeMono, loopLen, AbacDsp::Slicer::AdaptiveParams{}, AbacDsp::Slicer::TransientParams{}, grid);
        m_freezeResultThumbnails.assign(m_freezeResultSlices.size() * AbacDsp::SliceLibrary::kThumbFloats, 0.f);
        for (size_t i = 0; i < m_freezeResultSlices.size(); ++i)
        {
            computeSliceThumbnail(m_freezeResultSlices[i],
                                  std::span<float>{m_freezeResultThumbnails}.subspan(
                                      i * AbacDsp::SliceLibrary::kThumbFloats, AbacDsp::SliceLibrary::kThumbFloats));
        }
        m_freezeDoneGen.store(gen, std::memory_order_release);
    }

    // Reads only within the slice's own bounds, so it never bleeds into a
    // neighbouring one. Worker thread only, same as runFreezeAnalysis().
    void computeSliceThumbnail(const AbacDsp::Slice& slice, std::span<float> dst)
    {
        constexpr size_t kThumbWidth = AbacDsp::SliceLibrary::kThumbWidth;
        constexpr size_t kThumbHeight = AbacDsp::SliceLibrary::kThumbHeight;
        const size_t sliceEnd = slice.startFrame + slice.lengthFrames;
        const size_t hop = std::max<size_t>(1, slice.lengthFrames / kThumbWidth);
        std::array<float, kThumbFftLength> frame{};
        std::vector<float> magnitudes(kThumbHeight, 0.f);
        for (size_t t = 0; t < kThumbWidth; ++t)
        {
            frame.fill(0.f);
            const size_t start = slice.startFrame + t * hop;
            for (size_t i = 0; i < kThumbFftLength && start + i < sliceEnd && start + i < m_freezeMono.size(); ++i)
            {
                frame[i] = m_freezeMono[start + i];
            }
            m_freezeFft.compute(std::vector<float>{frame.begin(), frame.end()}, magnitudes);
            std::copy_n(magnitudes.data(), kThumbHeight, dst.data() + t * kThumbHeight);
        }
    }

    const AbacDsp::LoopPartBank<BlockSize>& m_bank;
    AbacDsp::HannWindowMagnitudesFft m_freezeFft; // worker-owned, see runFreezeAnalysis()

    bool m_freezePending{false};
    uint64_t m_freezePendingGen{0};
    size_t m_freezeSamplesPerBeat{0};
    std::atomic<uint64_t> m_freezeRequestGen{0};
    std::atomic<uint64_t> m_freezeDoneGen{0};
    std::vector<float> m_freezeMono;                  // worker-owned scratch
    std::vector<AbacDsp::Slice> m_freezeResultSlices; // worker writes, audio thread reads once done
    std::vector<float> m_freezeResultThumbnails;      // one kThumbFloats block per candidate, same order
    std::mutex m_freezeWaitMutex;                     // guards only the worker's own condvar wait
    std::condition_variable_any m_freezeCv;
    std::jthread m_freezeThread;
};
