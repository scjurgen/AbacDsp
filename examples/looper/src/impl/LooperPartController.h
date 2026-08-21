#pragma once

#include <algorithm>
#include <cstddef>

#include "Audio/AudioBuffer.h"
#include "LooperTimingController.h"
#include "Sampler/LoopPartBank.h"
#include "Sampler/LoopRecorder.h"

// Queues a switch between two already-recorded parts and crossfades into it
// at the next bar boundary. Record-target switching (queuing into an empty
// part to record) is deferred to a later phase; see LooperTransportController.
template <size_t BlockSize>
class LooperPartController
{
  public:
    using Bank = AbacDsp::LoopPartBank<BlockSize>;

    LooperPartController(Bank& bank, LooperTimingController& timing, size_t& activePartIndex, const size_t fadeFrames)
        : m_bank(bank)
        , m_timing(timing)
        , m_activePartIndex(activePartIndex)
        , m_fadeFrames(std::max<size_t>(1, fadeFrames))
    {
    }

    // Refused (false) if target is invalid, already active, empty, recording,
    // or a switch is already pending/in progress; otherwise commits immediately
    // (nothing playing to wait for) or queues for the next onBarBoundary().
    bool requestSwitch(const size_t target) noexcept
    {
        if (target >= Bank::kMaxParts || target == m_activePartIndex || !m_bank.hasContent(target) || m_pending ||
            isCrossfading() || m_bank.active().state() == AbacDsp::LooperState::Recording)
        {
            return false;
        }
        if (isActivePartAudible())
        {
            m_pending = true;
            m_pendingTarget = target;
        }
        else
        {
            commitSwitch(target);
        }
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
            commitSwitch(m_pendingTarget);
        }
    }

    // Audio thread. Plain pass-through outside a crossfade; during one, mixes
    // the outgoing part's tail (fading out) against the incoming part's head
    // (fading in) -- both parts render, doubling cost only for the fade window.
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
        }
    }

  private:
    [[nodiscard]] bool isActivePartAudible() const noexcept
    {
        const auto s = m_bank.active().state();
        return s == AbacDsp::LooperState::Playing || s == AbacDsp::LooperState::Overdubbing;
    }

    // Flips the active index, resyncs the shared clock to the new part's own
    // meter timeline, and either starts a crossfade (the outgoing part was
    // audible) or cuts over immediately (it wasn't).
    void commitSwitch(const size_t target) noexcept
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
        m_bank.active().stop(); // guarantees playPos==0 regardless of where this part was left
        m_bank.active().play();

        if (wasAudible)
        {
            m_outgoingIndex = outgoing;
            m_crossfadeRemaining = m_fadeFrames;
        }
        else
        {
            m_crossfadeRemaining = 0;
        }
    }

    Bank& m_bank;
    LooperTimingController& m_timing;
    size_t& m_activePartIndex;
    size_t m_fadeFrames;
    bool m_pending{false};
    size_t m_pendingTarget{0};
    size_t m_outgoingIndex{0};
    size_t m_crossfadeRemaining{0};
};
