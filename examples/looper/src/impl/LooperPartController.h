#pragma once

#include <algorithm>
#include <cstddef>

#include "Audio/AudioBuffer.h"
#include "LooperTimingController.h"
#include "LooperTransportController.h"
#include "Sampler/LoopPartBank.h"
#include "Sampler/LoopRecorder.h"

// Queues a switch between parts, committed at the next bar boundary: a
// crossfade into an already-recorded part, or -- muting the outgoing part
// first -- a fresh take via LooperTransportController's record machinery.
template <size_t BlockSize>
class LooperPartController
{
  public:
    using Bank = AbacDsp::LoopPartBank<BlockSize>;

    LooperPartController(Bank& bank, LooperTimingController& timing, LooperTransportController<BlockSize>& transport,
                         size_t& activePartIndex, const size_t fadeFrames)
        : m_bank(bank)
        , m_timing(timing)
        , m_transport(transport)
        , m_activePartIndex(activePartIndex)
        , m_fadeFrames(std::max<size_t>(1, fadeFrames))
    {
    }

    // Refused (false) if target is invalid, already active, empty, recording,
    // or a switch is already pending/in progress; otherwise commits immediately
    // (nothing playing to wait for) or queues for the next onBarBoundary().
    bool requestSwitch(const size_t target) noexcept
    {
        if (!targetIsUsable(target) || !m_bank.hasContent(target))
        {
            return false;
        }
        queueOrCommit(target, false);
        return true;
    }

    // Queues a fresh take on target, muting the outgoing part first. Unlike
    // requestSwitch(), target may already have content (overwritten, same as
    // re-recording today) or be empty; same-as-active is refused (not a switch).
    bool requestRecordSwitch(const size_t target) noexcept
    {
        if (!targetIsUsable(target))
        {
            return false;
        }
        queueOrCommit(target, true);
        return true;
    }

    [[nodiscard]] bool isSwitchPending() const noexcept
    {
        return m_pending;
    }

    [[nodiscard]] bool isCrossfading() const noexcept
    {
        return m_crossfadeRemaining > 0;
    }

    // Called once per bar boundary, same call site as
    // LooperImpl::applyMeterAtBarBoundary(). Commits a pending switch, if any.
    void onBarBoundary() noexcept
    {
        if (m_pending)
        {
            m_pending = false;
            commitSwitch(m_pendingTarget, m_pendingIsRecord);
        }
    }

    // Audio thread. Outside a crossfade, a plain pass-through; during one,
    // mixes the outgoing part's tail against the incoming part's head (silence,
    // for a record-target fade, since the target isn't triggered until it ends).
    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out) noexcept
    {
        if (!isCrossfading())
        {
            m_bank.active().processBlock(in, out);
            return;
        }
        AbacDsp::AudioBuffer<2, BlockSize> outgoing{};
        AbacDsp::AudioBuffer<2, BlockSize> incoming{};
        const AbacDsp::AudioBuffer<2, BlockSize> silence{};
        m_bank.part(m_outgoingIndex).processBlock(silence, outgoing);
        m_bank.active().processBlock(silence, incoming);
        const auto fadeF = static_cast<float>(m_fadeFrames);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float gIn = 1.f - static_cast<float>(m_crossfadeRemaining) / fadeF;
            const float gOut = 1.f - gIn;
            out(i, 0) = outgoing(i, 0) * gOut + incoming(i, 0) * gIn;
            out(i, 1) = outgoing(i, 1) * gOut + incoming(i, 1) * gIn;
            if (m_crossfadeRemaining > 0)
            {
                --m_crossfadeRemaining;
            }
        }
        if (!isCrossfading())
        {
            m_bank.part(m_outgoingIndex).stop();
            if (m_fadeIsRecordTransition)
            {
                m_fadeIsRecordTransition = false;
                m_transport.startFreshRecording();
            }
        }
    }

  private:
    [[nodiscard]] bool isActivePartAudible() const noexcept
    {
        const auto s = m_bank.active().state();
        return s == AbacDsp::LooperState::Playing || s == AbacDsp::LooperState::Overdubbing;
    }

    // Shared validity check for both request methods: in range, not the
    // active part, and not mid-Recording (no stable audio to switch from yet).
    [[nodiscard]] bool targetIsUsable(const size_t target) const noexcept
    {
        return target < Bank::kMaxParts && target != m_activePartIndex && !m_pending && !isCrossfading() &&
               m_bank.active().state() != AbacDsp::LooperState::Recording;
    }

    void queueOrCommit(const size_t target, const bool isRecord) noexcept
    {
        if (isActivePartAudible())
        {
            m_pending = true;
            m_pendingIsRecord = isRecord;
            m_pendingTarget = target;
        }
        else
        {
            commitSwitch(target, isRecord);
        }
    }

    // Flips the active index and resyncs the clock to the new part's own meter
    // timeline; if the outgoing part was audible, crossfades into playback or
    // silence-before-record, otherwise applies the target's start immediately.
    void commitSwitch(const size_t target, const bool isRecord) noexcept
    {
        const size_t outgoing = m_activePartIndex;
        if (m_bank.part(outgoing).state() == AbacDsp::LooperState::Overdubbing)
        {
            m_bank.part(outgoing).endOverdub();
        }
        const bool wasAudible = m_bank.part(outgoing).state() == AbacDsp::LooperState::Playing;

        m_activePartIndex = target;
        m_bank.setActiveIndex(target); // keeps LoopPartBank's own active() in sync
        m_timing.resyncTimekeeperToLoopStart();

        if (wasAudible)
        {
            m_outgoingIndex = outgoing;
            m_crossfadeRemaining = m_fadeFrames;
            m_fadeIsRecordTransition = isRecord;
            return;
        }
        m_crossfadeRemaining = 0;
        if (isRecord)
        {
            m_transport.startFreshRecording();
        }
        else
        {
            m_bank.active().stop(); // guarantees playPos==0 regardless of where this part was left
            m_bank.active().play();
        }
    }

    Bank& m_bank;
    LooperTimingController& m_timing;
    LooperTransportController<BlockSize>& m_transport;
    size_t& m_activePartIndex;
    size_t m_fadeFrames;
    bool m_pending{false};
    bool m_pendingIsRecord{false};
    size_t m_pendingTarget{0};
    size_t m_outgoingIndex{0};
    size_t m_crossfadeRemaining{0};
    bool m_fadeIsRecordTransition{false};
};
