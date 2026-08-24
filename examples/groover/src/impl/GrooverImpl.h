#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "GrooveDefaultPaths.h"
#include "GrooveLoopBuffer.h"
#include "Sampler/GrooveDrumPlayer.h"
#include "Sampler/GrooveKit.h"

// ADL hooks so GrooveKit<nlohmann::json> can parse a groove's sidecar metadata;
// kept here (not in core) since the core library must stay JSON-library-free.
namespace AbacDsp
{
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GrooveSidecarRhythm, feel, timeSignature)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GrooveSidecar, rhythm, dominantSounds)
}

using GrooveKit = AbacDsp::GrooveKit<nlohmann::json>;

/**
 * @brief Standalone MIDI-groove drum player: pick a groove, play it, pass audio
 * through with its own gain. Extracted from the looper's groove feature.
 *
 * Renders into a GrooveLoopBuffer kept a lookahead ahead of the read cursor,
 * rather than calling GrooveDrumPlayer::advanceSample() straight into the
 * output; Play copies a background-rendered burst (see GrooveKit::BurstConfig)
 * into that buffer instead of computing it synchronously.
 */
template <size_t BlockSize>
class GrooverImpl final : public EffectBase
{
  public:
    explicit GrooverImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_grooveSequencer(sampleRate)
        , m_loopBuffer(sampleRate)
    {
        m_grooveKit.requestLoad(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, kAbacDspDefaultGrooveName,
                                AbacDsp::BurstConfig{sampleRate, m_bpm.load(std::memory_order_relaxed)});
    }

    void setPlay(const bool value) noexcept
    {
        if (value)
        {
            m_playPulse.store(true, std::memory_order_relaxed);
        }
    }

    void setHostSync(const bool value) noexcept
    {
        m_hostSyncReq.store(value, std::memory_order_relaxed);
    }

    void setBpm(const float value) noexcept
    {
        m_bpm.store(value, std::memory_order_relaxed);
    }

    void setGrooveVariation(const float value) noexcept
    {
        m_grooveVariationReq.store(static_cast<int>(value), std::memory_order_relaxed);
    }

    void setOutputLevel(const float value) noexcept
    {
        m_outputGain.store(std::pow(10.f, value / 20.f), std::memory_order_relaxed);
    }

    void setInputGain(const float value) noexcept
    {
        m_inputGain.store(std::pow(10.f, value / 20.f), std::memory_order_relaxed);
    }

    // Groove menu click: styleName is one of listGrooveNames()'s own entries.
    void requestLoadGroove(const std::string& styleName, const unsigned variationIndex)
    {
        {
            std::lock_guard lock(m_grooveStyleMutex);
            m_currentGrooveStyle = styleName;
        }
        m_grooveKit.requestLoadStyle(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, styleName, variationIndex,
                                     AbacDsp::BurstConfig{sampleRate(), m_bpm.load(std::memory_order_relaxed)});
    }

    [[nodiscard]] std::vector<std::string> listGrooveNames() const
    {
        return GrooveKit::listAvailableGrooves(kAbacDspMidiDrumsDir);
    }

    [[nodiscard]] std::string currentGrooveName() const
    {
        return m_grooveKit.currentGrooveName();
    }

    [[nodiscard]] bool isGroovePlaying() const noexcept
    {
        return m_isPlaying;
    }

    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return m_hostSync;
    }

    [[nodiscard]] bool canEditBpm() const noexcept
    {
        return !m_hostSync;
    }

    // One-shot: the newly-installed groove's info text, consumed once (empty
    // after), same std::exchange shape as consumeLastSavedLoopName elsewhere.
    [[nodiscard]] std::string consumeGrooveInfoText() const
    {
        std::lock_guard lock(m_infoTextMutex);
        return std::exchange(m_pendingInfoText, std::string());
    }

    // Test-support only, thin pass-throughs for Groover_tests.cpp's integration tests.
    [[nodiscard]] size_t loopBufferFramesAheadForTest() const noexcept
    {
        return m_loopBuffer.framesAhead();
    }

    [[nodiscard]] bool hasBurstAudioForTest() const noexcept
    {
        return static_cast<bool>(m_grooveKit.installedBurstAudio());
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_grooveKit.pollAndInstall();
        checkGrooveInfoTextChanged();
        installProgramIfChanged();

        applyParameters(); // may toggle Play, priming from the groove just installed above

        const float bpm = (m_hostSync && hostTransport().isPlaying) ? static_cast<float>(hostTransport().bpm)
                                                                    : m_bpm.load(std::memory_order_relaxed);
        const auto samplesPerBeat = static_cast<size_t>(sampleRate() * 60.f / std::max(1.f, bpm));

        if (m_hostSync && hostTransport().isPlaying && m_grooveSequencer.syncToPpq(hostTransport().ppqPosition))
        {
            m_loopBuffer.reset();
        }

        if (m_isPlaying)
        {
            while (m_loopBuffer.framesAhead() < samplesPerBeat)
            {
                const auto frame = m_grooveSequencer.advanceSample(samplesPerBeat);
                m_loopBuffer.writeFrame(frame[0], frame[1]);
            }
        }

        const float outputGain = m_outputGain.load(std::memory_order_relaxed);
        const float inputGain = m_inputGain.load(std::memory_order_relaxed);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto grooveFrame = m_isPlaying ? m_loopBuffer.readFrame() : std::array<float, 2>{0.f, 0.f};
            out(i, 0) = in(i, 0) * inputGain + grooveFrame[0] * outputGain;
            out(i, 1) = in(i, 1) * inputGain + grooveFrame[1] * outputGain;
        }
    }

  private:
    void applyParameters() noexcept
    {
        m_hostSync = m_hostSyncReq.load(std::memory_order_relaxed);

        if (m_playPulse.exchange(false, std::memory_order_relaxed))
        {
            togglePlay();
        }

        const int grooveVariation = m_grooveVariationReq.load(std::memory_order_relaxed);
        if (grooveVariation != m_appliedGrooveVariation)
        {
            m_appliedGrooveVariation = grooveVariation;
            std::string style;
            {
                std::lock_guard lock(m_grooveStyleMutex);
                style = m_currentGrooveStyle;
            }
            if (!style.empty())
            {
                m_grooveKit.requestLoadStyle(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, style,
                                             static_cast<unsigned>(grooveVariation),
                                             AbacDsp::BurstConfig{sampleRate(), m_bpm.load(std::memory_order_relaxed)});
            }
        }
    }

    // A newly-installed program means stale, already-buffered audio was rendered
    // against the previous groove - flush it (mirrors the Play-press/host-sync-jump
    // flush points below, just triggered by a background load landing instead).
    void installProgramIfChanged() noexcept
    {
        const auto* program = m_grooveKit.program();
        if (program != m_lastProgram)
        {
            m_lastProgram = program;
            m_loopBuffer.reset();
        }
        m_grooveSequencer.setLibrary(m_grooveKit.library());
        m_grooveSequencer.setTrackNames(m_grooveKit.installedTrackNames());
        m_grooveSequencer.setGroove(program);
    }

    void togglePlay() noexcept
    {
        if (m_isPlaying)
        {
            m_isPlaying = false;
            return;
        }
        m_isPlaying = true;
        m_loopBuffer.reset();
        const auto burst = m_grooveKit.installedBurstAudio();
        if (burst && !burst->empty())
        {
            for (size_t i = 0; i * 2 < burst->size(); ++i)
            {
                m_loopBuffer.writeFrame((*burst)[i * 2], (*burst)[i * 2 + 1]);
            }
            m_grooveSequencer.primeTickState(m_grooveKit.installedBurstTickPos(),
                                             m_grooveKit.installedBurstNextTriggerIndex());
        }
        else
        {
            m_grooveSequencer.resetPosition();
        }
    }

    // Logs (via the status bar, once per actually-installed groove) instead of
    // on every block; mirrors LooperImpl's own logGrooveChangeIfAny().
    void checkGrooveInfoTextChanged()
    {
        const std::string& grooveName = m_grooveKit.installedGrooveName();
        if (grooveName.empty() || grooveName == m_lastGrooveName)
        {
            return;
        }
        m_lastGrooveName = grooveName;
        const std::string text = GrooveKit::formatGrooveInfoText(grooveName, m_grooveKit.installedMetadata());
        std::lock_guard lock(m_infoTextMutex);
        m_pendingInfoText = text;
    }

    GrooveKit m_grooveKit;
    AbacDsp::GrooveDrumPlayer m_grooveSequencer;
    GrooveLoopBuffer m_loopBuffer;
    const AbacDsp::GrooveProgram* m_lastProgram{nullptr};

    std::atomic<bool> m_playPulse{false};
    std::atomic<bool> m_hostSyncReq{false};
    std::atomic<float> m_bpm{120.f};
    std::atomic<int> m_grooveVariationReq{0};
    std::atomic<float> m_outputGain{1.f};
    std::atomic<float> m_inputGain{1.f};

    bool m_hostSync{false};
    bool m_isPlaying{false};
    int m_appliedGrooveVariation{0};

    std::mutex m_grooveStyleMutex;
    std::string m_currentGrooveStyle; // "<Genre>/<style>", empty until a menu pick

    std::string m_lastGrooveName; // audio thread only
    mutable std::mutex m_infoTextMutex;
    mutable std::string m_pendingInfoText;
};
