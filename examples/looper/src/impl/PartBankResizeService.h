#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "Sampler/LoopPartBank.h"
#include "Sampler/LoopRecorder.h"

// RT-safe async resize of a LoopPartBank's per-part capacity, a third use of
// FreezeService/LoopStorageService's jthread + generation-counter handshake.
// The worker builds fresh LoopRecorder instances off-thread (the only place
// allocation happens); a commit only ever swaps pointers on the audio thread,
// stashing the displaced ones in a discard bin the worker frees on its next
// wake -- the audio thread never allocates or frees.
//
// Only guards re-entrancy (a resize already pending); whether a resize is
// currently *allowed* (e.g. the whole bank must be empty) is the caller's
// job, same as FreezeService leaves the recording/overdub/empty-loop checks
// to LooperTransportController. Likewise, adjusting the bank's active index
// after a partCount shrink is the caller's job, not this class's.
template <size_t BlockSize>
class PartBankResizeService
{
  public:
    using Bank = AbacDsp::LoopPartBank<BlockSize>;
    static constexpr size_t kMaxParts = Bank::kMaxParts;

    PartBankResizeService(Bank& bank, const float sampleRate)
        : m_bank(bank)
        , m_sampleRate(sampleRate)
    {
        m_worker = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_waitMutex);
                    m_cv.wait(lock, stopToken,
                              [this, lastHandled]
                              {
                                  return m_requestGen.load(std::memory_order_acquire) != lastHandled ||
                                         m_discardPending.load(std::memory_order_acquire);
                              });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    if (m_discardPending.load(std::memory_order_acquire))
                    {
                        freeDiscardBin();
                    }
                    const uint64_t gen = m_requestGen.load(std::memory_order_acquire);
                    if (gen != lastHandled)
                    {
                        lastHandled = gen;
                        runResize(gen);
                    }
                }
            });
    }

    // Audio thread. Refused (false) while a resize is already pending;
    // clamps partCount to [1, kMaxParts] and capacityFrames to at least
    // one block.
    bool requestResize(const size_t partCount, const size_t capacityFrames) noexcept
    {
        if (m_pending)
        {
            return false;
        }
        m_requestedPartCount = std::clamp(partCount, size_t{1}, kMaxParts);
        m_requestedCapacityFrames = std::max(capacityFrames, size_t{BlockSize});
        m_pendingGen = m_requestGen.load(std::memory_order_relaxed) + 1;
        m_pending = true;
        m_requestGen.store(m_pendingGen, std::memory_order_release);
        m_cv.notify_one();
        return true;
    }

    [[nodiscard]] bool isPending() const noexcept
    {
        return m_pending;
    }

    // Audio thread, polled every block. Returns true the block a resize
    // actually commits. A failed resize also clears isPending(), without
    // touching the bank; see consumeLastError().
    bool checkResizeCompletion() noexcept
    {
        if (!m_pending)
        {
            return false;
        }
        if (m_failedGen.load(std::memory_order_acquire) == m_pendingGen)
        {
            m_pending = false;
            return false;
        }
        if (m_doneGen.load(std::memory_order_acquire) != m_pendingGen)
        {
            return false;
        }
        for (size_t i = 0; i < kMaxParts; ++i)
        {
            if (!m_pendingParts[i].has_value())
            {
                continue;
            }
            m_discardBin[i] = std::move(m_bank.part(i));
            m_bank.part(i) = std::move(*m_pendingParts[i]);
            m_pendingParts[i].reset();
        }
        m_discardPending.store(true, std::memory_order_release);
        m_cv.notify_one();
        m_pending = false;
        return true;
    }

    // One-shot, consumed on read (same std::exchange-under-lock pattern as
    // LoopStorageService::consumeLastSavedLoopName()). Non-empty only after
    // a failed resize.
    [[nodiscard]] std::string consumeLastError()
    {
        std::lock_guard lock(m_errorMutex);
        return std::exchange(m_lastError, std::string{});
    }

  private:
    // Worker thread only: the sole place allocation happens. Publishes
    // m_doneGen on success or m_failedGen on failure, leaving
    // m_pendingParts empty in the failure case.
    void runResize(const uint64_t gen)
    {
        size_t allocatedBytes = 0;
        try
        {
            for (size_t i = 0; i < kMaxParts; ++i)
            {
                const size_t frames = (i < m_requestedPartCount) ? m_requestedCapacityFrames : 0;
                const float partSeconds = static_cast<float>(frames) / m_sampleRate;
                m_pendingParts[i].emplace(m_sampleRate, partSeconds);
                allocatedBytes += m_pendingParts[i]->approxByteSize();
            }
        }
        catch (const std::exception& e)
        {
            for (auto& slot : m_pendingParts)
            {
                slot.reset();
            }
            {
                std::lock_guard lock(m_errorMutex);
                m_lastError = std::string("Part resize failed: ") + e.what();
            }
            std::cout << "PartBankResizeService: resize FAILED (" << e.what() << "), keeping current buffers"
                      << std::endl;
            m_failedGen.store(gen, std::memory_order_release);
            return;
        }
        std::cout << "PartBankResizeService: allocated " << allocatedBytes << " bytes ("
                  << (static_cast<double>(allocatedBytes) / (1024.0 * 1024.0)) << " MB) for " << m_requestedPartCount
                  << " active part(s) at " << m_requestedCapacityFrames << " frames each" << std::endl;
        m_doneGen.store(gen, std::memory_order_release);
    }

    // Worker thread only: destroys whatever checkResizeCompletion() displaced,
    // actually freeing the old buffers off the audio thread.
    void freeDiscardBin()
    {
        size_t freedBytes = 0;
        for (auto& slot : m_discardBin)
        {
            if (slot.has_value())
            {
                freedBytes += slot->approxByteSize();
                slot.reset();
            }
        }
        m_discardPending.store(false, std::memory_order_release);
        if (freedBytes > 0)
        {
            std::cout << "PartBankResizeService: freed " << freedBytes << " bytes ("
                      << (static_cast<double>(freedBytes) / (1024.0 * 1024.0)) << " MB) from the previous capacity"
                      << std::endl;
        }
    }

    Bank& m_bank;
    float m_sampleRate;

    bool m_pending{false};
    uint64_t m_pendingGen{0};
    size_t m_requestedPartCount{0};
    size_t m_requestedCapacityFrames{0};
    std::atomic<uint64_t> m_requestGen{0};
    std::atomic<uint64_t> m_doneGen{0};
    std::atomic<uint64_t> m_failedGen{0};

    std::array<std::optional<AbacDsp::LoopRecorder<BlockSize>>, kMaxParts> m_pendingParts; // worker-owned until commit
    std::array<std::optional<AbacDsp::LoopRecorder<BlockSize>>, kMaxParts>
        m_discardBin; // audio thread displaces, worker frees
    std::atomic<bool> m_discardPending{false};

    std::mutex m_errorMutex;
    std::string m_lastError;

    std::mutex m_waitMutex; // guards only the worker's own condvar wait
    std::condition_variable_any m_cv;
    std::jthread m_worker;
};
