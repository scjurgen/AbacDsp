#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

#include "Audio/AudioBuffer.h"
#include "Delays/VariSpeedTapeDelay.h"
#include "EffectBase.h"
#include "Filters/Sinc/sinc_4.h"
#include "Generators/ClickGenerator.h"
#include "GrooveDefaultPaths.h"
#include "GrooveLoopBuffer.h"
#include "Helpers/ConstructArray.h"
#include "Parameters/LinearParameter.h"
#include "Sampler/GrooveDrumPlayer.h"
#include "Sampler/GrooveKit.h"
#include "TapeLooperScriptEngine.h"

// ADL hooks so GrooveKit<nlohmann::json> can parse a groove's sidecar metadata;
// kept here (not in core) since the core library must stay JSON-library-free.
namespace AbacDsp
{
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GrooveSidecarRhythm, feel, timeSignature)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GrooveSidecar, idealBpm, rhythm, dominantSounds)
}

using GrooveKit = AbacDsp::GrooveKit<nlohmann::json>;

namespace TapeLooperDetail
{
constexpr float kBeatsPerBar = 4.f;
constexpr float kAssumedSampleRate = 48000.f;
constexpr float kMinBars = 1.f;
constexpr float kMaxBars = 32.f;
constexpr float kMinBpm = 50.f;
constexpr size_t kFreeTracks{3};

// Match VariSpeedTapeDelay's own constructor defaults, so wiring these in
// doesn't change the default sound.
constexpr float kDefaultWowDepth = 0.1f;
constexpr float kDefaultWowRate = 0.4f;
constexpr float kDefaultWowDrift = 0.05f;
constexpr float kDefaultFlutterDepth = 0.1f;
constexpr float kDefaultFlutterRate = 0.4f;

constexpr float kDefaultTrackGain = 1.f;
// Matches the Track Gain dial's own +12 dB ceiling (10^(12/20)).
constexpr float kMaxTrackGain = 4.f;

constexpr size_t framesForLoop(const float bars, const float bpm) noexcept
{
    return static_cast<size_t>(bars * kBeatsPerBar / bpm * 60.f * kAssumedSampleRate);
}

// Sized for the longest possible loop (32 bars @ 50 BPM); the margin keeps
// that loop's read distance clear of setReadHead()'s own safety clamp.
constexpr size_t kMaxLoopFrames = framesForLoop(kMaxBars, kMinBpm);
constexpr size_t kModulationMargin = 4800;
constexpr size_t kBufferSize = kMaxLoopFrames + kModulationMargin;
}

/**
 * @brief Varispeed kFreeTracks tape recorder (A, B, ...) plus a parallel MIDI-groove
 * track, all riding one shared tape-speed ratio - four independent transports,
 * not four speeds.
 *
 * Each track is a VariSpeedTapeDelay used as a fixed-length loop: the read
 * head trails the write head by a constant distance (bars * beats/bar / BPM),
 * and every block reads before it writes. Recording adds live input onto
 * that read-back (overdub); not recording writes it back unchanged,
 * sustaining the loop. The groove track is driven the same way from a
 * lookahead-rendered MIDI groove (reusing groover's player), so it
 * varispeeds with the tape tracks instead of playing at a fixed pitch.
 */
template <size_t BlockSize>
class TapeLooperImpl final : public EffectBase
{
  public:
    using TapeTrack = AbacDsp::VariSpeedTapeDelay<TapeLooperDetail::kBufferSize, 2, 1, BlockSize>;
    static_assert(TapeLooperScriptEngine::kTracks == TapeLooperDetail::kFreeTracks,
                  "TapeLooperScriptEngine's per-track pool must match TapeLooperDetail::kFreeTracks");

    explicit TapeLooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_sincFilter(std::make_shared<AbacDsp::SincFilter>(sinc4))
        , m_tapeTrack(AbacDsp::constructArray<TapeTrack, TapeLooperDetail::kFreeTracks>(sampleRate, m_sincFilter))
        , m_grooveTape(sampleRate, m_sincFilter)
        , m_grooveSequencer(sampleRate)
        , m_loopBuffer(sampleRate)
        , m_clickGen(sampleRate)
        , m_trackGainSmoother(
              AbacDsp::constructArray<AbacDsp::LinearSmoothingParameter<BlockSize>, TapeLooperDetail::kFreeTracks>(
                  TapeLooperDetail::kDefaultTrackGain))
    {
        applyLoopLengthIfChanged();
        m_grooveKit.requestLoad(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, kAbacDspDefaultGrooveName,
                                AbacDsp::BurstConfig{sampleRate, m_bpmReq.load(std::memory_order_relaxed)});
        m_scriptEngine.setSampleRate(sampleRate);
        for (auto& smoother : m_trackGainSmoother)
        {
            smoother.setMin(0.f);
            smoother.setMax(TapeLooperDetail::kMaxTrackGain);
        }
    }

    // A reload resets the script's Lua globals, so resendUiParameters() re-syncs it to
    // each claimed slot's current value - otherwise it stays believing coded defaults.
    bool setScript(const std::string_view source)
    {
        const bool ok = m_scriptEngine.loadScript(source);
        if (ok)
        {
            resendUiParameters();
        }
        return ok;
    }

    void setImportResolver(TapeLooperScriptEngine::ImportResolver resolver)
    {
        m_scriptEngine.setImportResolver(std::move(resolver));
    }

    [[nodiscard]] bool hasScriptError() const noexcept
    {
        return m_scriptEngine.hasError();
    }

    [[nodiscard]] const std::string& scriptError() const noexcept
    {
        return m_scriptEngine.lastError();
    }

    // Shown by the popup editor's Reset button, not the engine's own default script.
    [[nodiscard]] static std::string scriptSkeleton()
    {
        return std::string(TapeLooperScriptEngine::kFullSkeletonScript);
    }

    [[nodiscard]] const TapeLooperScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    void setLuaParam1(const float value) noexcept
    {
        m_luaParamValues[0] = value;
    }

    void setLuaParam2(const float value) noexcept
    {
        m_luaParamValues[1] = value;
    }

    void setLuaParam3(const float value) noexcept
    {
        m_luaParamValues[2] = value;
    }

    void setLuaParam4(const float value) noexcept
    {
        m_luaParamValues[3] = value;
    }

    void setLuaParam5(const float value) noexcept
    {
        m_luaParamValues[4] = value;
    }

    void setLuaParam6(const float value) noexcept
    {
        m_luaParamValues[5] = value;
    }

    void setLuaParam7(const float value) noexcept
    {
        m_luaParamValues[6] = value;
    }

    void setLuaParam8(const float value) noexcept
    {
        m_luaParamValues[7] = value;
    }

    void setTapeSpeed(const float value) noexcept
    {
        m_tapeSpeedReq.store(value, std::memory_order_relaxed);
    }

    void setBars(const float value) noexcept
    {
        m_barsReq.store(value, std::memory_order_relaxed);
    }

    void setClearA(const bool value) noexcept
    {
        if (value)
        {
            m_clearReq[0].store(true, std::memory_order_relaxed);
        }
    }

    void setClearB(const bool value) noexcept
    {
        if (value)
        {
            m_clearReq[1].store(true, std::memory_order_relaxed);
        }
    }

    void setClearC(const bool value) noexcept
    {
        if (value)
        {
            m_clearReq[2].store(true, std::memory_order_relaxed);
        }
    }

    void setInputGain(const float value) noexcept
    {
        m_inputGainReq.store(std::pow(10.f, value / 20.f), std::memory_order_relaxed);
    }

    void setGrooveLevel(const float value) noexcept
    {
        m_grooveLevelReq.store(std::pow(10.f, value / 20.f), std::memory_order_relaxed);
    }

    void setRecordA(const bool value) noexcept
    {
        m_recordReq[0].store(value, std::memory_order_relaxed);
    }

    void setPlayA(const bool value) noexcept
    {
        m_playReq[0].store(value, std::memory_order_relaxed);
    }

    void setRecordB(const bool value) noexcept
    {
        m_recordReq[1].store(value, std::memory_order_relaxed);
    }

    void setPlayB(const bool value) noexcept
    {
        m_playReq[1].store(value, std::memory_order_relaxed);
    }

    void setRecordC(const bool value) noexcept
    {
        m_recordReq[2].store(value, std::memory_order_relaxed);
    }

    void setPlayC(const bool value) noexcept
    {
        m_playReq[2].store(value, std::memory_order_relaxed);
    }

    void setGroovePlay(const bool value) noexcept
    {
        m_groovePlayReq.store(value, std::memory_order_relaxed);
    }

    void setBpm(const float value) noexcept
    {
        m_bpmReq.store(value, std::memory_order_relaxed);
    }

    void setGrooveVariation(const float value) noexcept
    {
        m_grooveVariationReq.store(value, std::memory_order_relaxed);
    }

    void setWowDepthA(const float value) noexcept
    {
        m_wowDepthReq[0].store(value, std::memory_order_relaxed);
    }

    void setWowDepthB(const float value) noexcept
    {
        m_wowDepthReq[1].store(value, std::memory_order_relaxed);
    }

    void setWowDepthC(const float value) noexcept
    {
        m_wowDepthReq[2].store(value, std::memory_order_relaxed);
    }

    void setWowRateA(const float value) noexcept
    {
        m_wowRateReq[0].store(value, std::memory_order_relaxed);
    }

    void setWowRateB(const float value) noexcept
    {
        m_wowRateReq[1].store(value, std::memory_order_relaxed);
    }

    void setWowRateC(const float value) noexcept
    {
        m_wowRateReq[2].store(value, std::memory_order_relaxed);
    }

    void setWowDriftA(const float value) noexcept
    {
        m_wowDriftReq[0].store(value, std::memory_order_relaxed);
    }

    void setWowDriftB(const float value) noexcept
    {
        m_wowDriftReq[1].store(value, std::memory_order_relaxed);
    }

    void setWowDriftC(const float value) noexcept
    {
        m_wowDriftReq[2].store(value, std::memory_order_relaxed);
    }

    void setFlutterDepthA(const float value) noexcept
    {
        m_flutterDepthReq[0].store(value, std::memory_order_relaxed);
    }

    void setFlutterDepthB(const float value) noexcept
    {
        m_flutterDepthReq[1].store(value, std::memory_order_relaxed);
    }

    void setFlutterDepthC(const float value) noexcept
    {
        m_flutterDepthReq[2].store(value, std::memory_order_relaxed);
    }

    void setFlutterRateA(const float value) noexcept
    {
        m_flutterRateReq[0].store(value, std::memory_order_relaxed);
    }

    void setFlutterRateB(const float value) noexcept
    {
        m_flutterRateReq[1].store(value, std::memory_order_relaxed);
    }

    void setFlutterRateC(const float value) noexcept
    {
        m_flutterRateReq[2].store(value, std::memory_order_relaxed);
    }

    void setTrackGainA(const float valueDb) noexcept
    {
        m_trackGainReq[0].store(std::pow(10.f, valueDb / 20.f), std::memory_order_relaxed);
    }

    void setTrackGainB(const float valueDb) noexcept
    {
        m_trackGainReq[1].store(std::pow(10.f, valueDb / 20.f), std::memory_order_relaxed);
    }

    void setTrackGainC(const float valueDb) noexcept
    {
        m_trackGainReq[2].store(std::pow(10.f, valueDb / 20.f), std::memory_order_relaxed);
    }

    // Groove menu click: styleName is one of listGrooveNames()'s own entries.
    void requestLoadGroove(const std::string& styleName, const unsigned variationIndex)
    {
        {
            std::lock_guard lock(m_grooveStyleMutex);
            m_currentGrooveStyle = styleName;
        }
        m_grooveKit.requestLoadStyle(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, styleName, variationIndex,
                                     AbacDsp::BurstConfig{sampleRate(), m_bpmReq.load(std::memory_order_relaxed)});
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
        return m_groovePlaying;
    }

    [[nodiscard]] bool canEditBpm() const noexcept
    {
        return true;
    }

    [[nodiscard]] std::string consumeGrooveInfoText() const
    {
        std::lock_guard lock(m_infoTextMutex);
        return std::exchange(m_pendingInfoText, std::string());
    }

    // Test-support only, thin pass-through for Tapelooper_tests.cpp.
    [[nodiscard]] size_t grooveLoopBufferFramesAheadForTest() const noexcept
    {
        return m_loopBuffer.framesAhead();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_grooveKit.pollAndInstall();
        checkGrooveInfoTextChanged();
        installGrooveProgramIfChanged();
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();
        applyScriptCommands();
        applyParameters();

        for (size_t track = 0; track < m_tapeTrack.size(); ++track)
        {
            auto& tape = m_tapeTrack[track];
            tape.setWowDepth(m_wowDepth[track]);
            tape.setWowRate(m_wowRate[track]);
            tape.setWowDrift(m_wowDrift[track]);
            tape.setFlutterDepth(m_flutterDepth[track]);
            tape.setFlutterRate(m_flutterRate[track]);
            tape.setRatio(m_tapeSpeed);
        }
        m_grooveTape.setRatio(m_tapeSpeed);

        std::array<float, 2 * BlockSize> gainedIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            gainedIn[i * 2] = in(i, 0) * m_inputGain;
            gainedIn[i * 2 + 1] = in(i, 1) * m_inputGain;
        }

        std::array<float, 2 * BlockSize> mix{};
        processTapeTracks(gainedIn, mix);
        renderGrooveTrack(mix);

        // Live input always reaches the output, so a performer can hear
        // themselves while recording rather than only the looped playback.
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = gainedIn[i * 2] + mix[i * 2];
            out(i, 1) = gainedIn[i * 2 + 1] + mix[i * 2 + 1];
        }
    }

  private:
    void applyParameters() noexcept
    {
        m_tapeSpeed = std::clamp(m_tapeSpeedReq.load(std::memory_order_relaxed), 0.001f, 8.f);
        m_bpm = m_bpmReq.load(std::memory_order_relaxed);
        m_inputGain = m_inputGainReq.load(std::memory_order_relaxed);
        m_grooveLevel = m_grooveLevelReq.load(std::memory_order_relaxed);
        applyLoopLengthIfChanged();

        for (size_t track = 0; track < m_tapeTrack.size(); ++track)
        {
            if (m_clearReq[track].exchange(false, std::memory_order_relaxed))
            {
                m_tapeTrack[track].reset();
            }
            m_recording[track] = m_recordReq[track].load(std::memory_order_relaxed);
            if (m_recording[track] != m_lastNotifiedRecording[track])
            {
                m_scriptEngine.notifyRecordStateChanged(track, m_recording[track]);
                m_lastNotifiedRecording[track] = m_recording[track];
            }
            m_playing[track] = m_playReq[track].load(std::memory_order_relaxed);
            m_wowDepth[track] = m_wowDepthReq[track].load(std::memory_order_relaxed);
            m_wowRate[track] = m_wowRateReq[track].load(std::memory_order_relaxed);
            m_wowDrift[track] = m_wowDriftReq[track].load(std::memory_order_relaxed);
            m_flutterDepth[track] = m_flutterDepthReq[track].load(std::memory_order_relaxed);
            m_flutterRate[track] = m_flutterRateReq[track].load(std::memory_order_relaxed);
            m_trackGainSmoother[track].setValue(m_trackGainReq[track].load(std::memory_order_relaxed));
        }

        const bool groovePlayReq = m_groovePlayReq.load(std::memory_order_relaxed);
        if (groovePlayReq && !m_groovePlaying)
        {
            m_grooveSequencer.resetPosition();
            m_loopBuffer.reset();
        }
        m_groovePlaying = groovePlayReq;
        m_useClick = m_grooveSourceReq.load(std::memory_order_relaxed);

        applyGrooveVariationIfChanged();
    }

    // Drains whatever the script requested this block into the same request atomics
    // host automation writes to, so the two sources share one apply path below and the
    // most recent write - script or host - simply wins.
    void applyScriptCommands() noexcept
    {
        if (const auto v = m_scriptEngine.drainTapeSpeedCommand())
        {
            m_tapeSpeedReq.store(*v, std::memory_order_relaxed);
        }
        if (const auto v = m_scriptEngine.drainBpmCommand())
        {
            m_bpmReq.store(*v, std::memory_order_relaxed);
        }
        if (const auto v = m_scriptEngine.drainGrooveVariationCommand())
        {
            m_grooveVariationReq.store(*v, std::memory_order_relaxed);
        }
        for (size_t track = 0; track < TapeLooperDetail::kFreeTracks; ++track)
        {
            if (const auto v = m_scriptEngine.drainRecordCommand(track))
            {
                m_recordReq[track].store(*v, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainPlayCommand(track))
            {
                m_playReq[track].store(*v, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackGainCommand(track))
            {
                m_trackGainReq[track].store(*v, std::memory_order_relaxed);
            }
        }
        if (const auto v = m_scriptEngine.drainGrooveSourceCommand())
        {
            m_grooveSourceReq.store(*v, std::memory_order_relaxed);
        }
    }

    // Same exact-equality reasoning as ResonikImpl's own notifyUiParametersIfChanged():
    // a stored float either stays bit-identical or is genuinely a new host/UI value.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void notifyUiParametersIfChanged() noexcept
    {
        for (size_t i = 0; i < TapeLooperScriptEngine::kMaxLuaParams; ++i)
        {
            if (m_luaParamValues[i] == m_lastNotifiedLuaParamValues[i])
            {
                continue;
            }
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }
#pragma GCC diagnostic pop

    // Notifies every slot's current value unconditionally, unlike
    // notifyUiParametersIfChanged() - see setScript()'s comment for why.
    void resendUiParameters() noexcept
    {
        for (size_t i = 0; i < TapeLooperScriptEngine::kMaxLuaParams; ++i)
        {
            m_scriptEngine.notifyUiParameterChanged(i, m_luaParamValues[i]);
            m_lastNotifiedLuaParamValues[i] = m_luaParamValues[i];
        }
    }

    // Loop length is bars * beats/bar / BPM, the same BPM the groove plays at.
    // Float equality mirrors GrooverImpl's applyHumanizeIfChanged(): a stored
    // value either stays bit-identical or is a genuinely new one.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void applyLoopLengthIfChanged() noexcept
    {
        const float bars = std::clamp(m_barsReq.load(std::memory_order_relaxed), TapeLooperDetail::kMinBars,
                                      TapeLooperDetail::kMaxBars);
        const float bpm = std::max(m_bpm, TapeLooperDetail::kMinBpm);
        if (bars == m_appliedBars && bpm == m_appliedBpmForLoopLength)
        {
            return;
        }
        m_appliedBars = bars;
        m_appliedBpmForLoopLength = bpm;
        const auto loopFrames = TapeLooperDetail::framesForLoop(bars, bpm);
        for (auto& tape : m_tapeTrack)
        {
            tape.setReadHead(0, static_cast<float>(loopFrames), true);
        }
    }
#pragma GCC diagnostic pop

    void applyGrooveVariationIfChanged()
    {
        const int grooveVariation = static_cast<int>(m_grooveVariationReq.load(std::memory_order_relaxed));
        if (grooveVariation == m_appliedGrooveVariation)
        {
            return;
        }
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
                                         AbacDsp::BurstConfig{sampleRate(), m_bpm});
        }
    }

    // A newly-installed program means stale, already-buffered audio was
    // rendered against the previous groove - flush it.
    void installGrooveProgramIfChanged()
    {
        const auto* program = m_grooveKit.program();
        if (program != m_lastGrooveProgram)
        {
            m_lastGrooveProgram = program;
            m_loopBuffer.reset();
        }
        m_grooveSequencer.setLibrary(m_grooveKit.library());
        m_grooveSequencer.setTrackNames(m_grooveKit.installedTrackNames());
        m_grooveSequencer.setGroove(program);
    }

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

    // Reads each track's current tape output before writing this block's
    // input: recording adds live input onto that read-back (overdub, not
    // replace); not recording writes it back unchanged, sustaining the loop.
    void processTapeTracks(const std::array<float, 2 * BlockSize>& in, std::array<float, 2 * BlockSize>& mix) noexcept
    {
        for (size_t track = 0; track < m_tapeTrack.size(); ++track)
        {
            std::array<float, 2 * BlockSize> tapeOut{};
            m_tapeTrack[track].readBlock(0, tapeOut);

            std::array<float, 2 * BlockSize> tapeIn = tapeOut;
            if (m_recording[track])
            {
                for (size_t i = 0; i < tapeIn.size(); ++i)
                {
                    tapeIn[i] += in[i];
                }
            }
            m_tapeTrack[track].feed(tapeIn);

            if (m_playing[track])
            {
                for (size_t i = 0; i < BlockSize; ++i)
                {
                    const auto gain = m_trackGainSmoother[track].getValue(i);
                    mix[i * 2] += tapeOut[i * 2] * gain;
                    mix[i * 2 + 1] += tapeOut[i * 2 + 1] * gain;
                }
            }
        }
    }

    [[nodiscard]] size_t samplesPerBeat() const noexcept
    {
        return static_cast<size_t>(sampleRate() * 60.f / std::max(1.f, m_bpm));
    }

    // Advances the click's own beat clock by BlockSize samples, writing a tempo-locked
    // click (accented on beat 1 of the bar) into grooveIn instead of the MIDI groove.
    void renderClick(std::array<float, 2 * BlockSize>& grooveIn) noexcept
    {
        const auto spb = std::max<size_t>(samplesPerBeat(), 1);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (m_clickPhase == 0)
            {
                const bool downbeat = m_clickBeatIndex % static_cast<size_t>(TapeLooperDetail::kBeatsPerBar) == 0;
                m_clickGen.trigger(downbeat ? AbacDsp::ClickAccent::Downbeat : AbacDsp::ClickAccent::Beat);
                m_clickBeatIndex = (m_clickBeatIndex + 1) % static_cast<size_t>(TapeLooperDetail::kBeatsPerBar);
            }
            const auto sample = m_clickGen.step0();
            grooveIn[i * 2] = sample;
            grooveIn[i * 2 + 1] = sample;
            m_clickPhase = (m_clickPhase + 1) % spb;
        }
    }

    void renderGrooveTrack(std::array<float, 2 * BlockSize>& mix) noexcept
    {
        if (m_groovePlaying && !m_useClick)
        {
            const auto spb = samplesPerBeat();
            while (m_loopBuffer.framesAhead() < spb)
            {
                const auto frame = m_grooveSequencer.advanceSample(spb);
                m_loopBuffer.writeFrame(frame[0], frame[1]);
            }
        }

        std::array<float, 2 * BlockSize> grooveOut{};
        m_grooveTape.readBlock(0, grooveOut);

        std::array<float, 2 * BlockSize> grooveIn{};
        if (m_groovePlaying)
        {
            if (m_useClick)
            {
                renderClick(grooveIn);
            }
            else
            {
                for (size_t i = 0; i < BlockSize; ++i)
                {
                    const auto frame = m_loopBuffer.readFrame();
                    grooveIn[i * 2] = frame[0];
                    grooveIn[i * 2 + 1] = frame[1];
                }
            }
        }
        m_grooveTape.feed(grooveIn);

        if (m_groovePlaying)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                mix[i * 2] += grooveOut[i * 2] * m_grooveLevel;
                mix[i * 2 + 1] += grooveOut[i * 2 + 1] * m_grooveLevel;
            }
        }
    }

    std::shared_ptr<AbacDsp::SincFilter> m_sincFilter;
    std::array<TapeTrack, TapeLooperDetail::kFreeTracks> m_tapeTrack;
    TapeTrack m_grooveTape;
    GrooveKit m_grooveKit;
    AbacDsp::GrooveDrumPlayer m_grooveSequencer;
    GrooveLoopBuffer m_loopBuffer;
    const AbacDsp::GrooveProgram* m_lastGrooveProgram{nullptr};
    AbacDsp::ClickGenerator m_clickGen;
    std::array<AbacDsp::LinearSmoothingParameter<BlockSize>, TapeLooperDetail::kFreeTracks> m_trackGainSmoother;
    TapeLooperScriptEngine m_scriptEngine;

    std::atomic<float> m_tapeSpeedReq{1.f};
    std::atomic<float> m_barsReq{8.f};
    std::array<std::atomic<bool>, TapeLooperDetail::kFreeTracks> m_clearReq{};
    std::atomic<float> m_inputGainReq{1.f};
    std::atomic<float> m_grooveLevelReq{1.f};
    std::array<std::atomic<bool>, TapeLooperDetail::kFreeTracks> m_recordReq{};
    std::array<std::atomic<bool>, TapeLooperDetail::kFreeTracks> m_playReq{};
    std::atomic<bool> m_groovePlayReq{false};
    std::atomic<float> m_bpmReq{120.f};
    std::atomic<float> m_grooveVariationReq{0.f};
    std::atomic<bool> m_grooveSourceReq{false}; // false = groove, true = click

    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_wowDepthReq{
        TapeLooperDetail::kDefaultWowDepth, TapeLooperDetail::kDefaultWowDepth, TapeLooperDetail::kDefaultWowDepth};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_wowRateReq{
        TapeLooperDetail::kDefaultWowRate, TapeLooperDetail::kDefaultWowRate, TapeLooperDetail::kDefaultWowRate};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_wowDriftReq{
        TapeLooperDetail::kDefaultWowDrift, TapeLooperDetail::kDefaultWowDrift, TapeLooperDetail::kDefaultWowDrift};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_flutterDepthReq{
        TapeLooperDetail::kDefaultFlutterDepth, TapeLooperDetail::kDefaultFlutterDepth,
        TapeLooperDetail::kDefaultFlutterDepth};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_flutterRateReq{
        TapeLooperDetail::kDefaultFlutterRate, TapeLooperDetail::kDefaultFlutterRate,
        TapeLooperDetail::kDefaultFlutterRate};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_trackGainReq{
        TapeLooperDetail::kDefaultTrackGain, TapeLooperDetail::kDefaultTrackGain, TapeLooperDetail::kDefaultTrackGain};

    float m_tapeSpeed{1.f};
    float m_appliedBars{0.f};
    float m_appliedBpmForLoopLength{0.f};
    float m_inputGain{1.f};
    float m_grooveLevel{1.f};
    std::array<bool, TapeLooperDetail::kFreeTracks> m_recording{};
    std::array<bool, TapeLooperDetail::kFreeTracks> m_playing{};
    std::array<float, TapeLooperDetail::kFreeTracks> m_wowDepth{
        TapeLooperDetail::kDefaultWowDepth, TapeLooperDetail::kDefaultWowDepth, TapeLooperDetail::kDefaultWowDepth};
    std::array<float, TapeLooperDetail::kFreeTracks> m_wowRate{
        TapeLooperDetail::kDefaultWowRate, TapeLooperDetail::kDefaultWowRate, TapeLooperDetail::kDefaultWowRate};
    std::array<float, TapeLooperDetail::kFreeTracks> m_wowDrift{
        TapeLooperDetail::kDefaultWowDrift, TapeLooperDetail::kDefaultWowDrift, TapeLooperDetail::kDefaultWowDrift};
    std::array<float, TapeLooperDetail::kFreeTracks> m_flutterDepth{TapeLooperDetail::kDefaultFlutterDepth,
                                                                    TapeLooperDetail::kDefaultFlutterDepth,
                                                                    TapeLooperDetail::kDefaultFlutterDepth};
    std::array<float, TapeLooperDetail::kFreeTracks> m_flutterRate{TapeLooperDetail::kDefaultFlutterRate,
                                                                   TapeLooperDetail::kDefaultFlutterRate,
                                                                   TapeLooperDetail::kDefaultFlutterRate};
    bool m_groovePlaying{false};
    bool m_useClick{false};
    size_t m_clickPhase{0};
    size_t m_clickBeatIndex{0};
    std::array<bool, TapeLooperDetail::kFreeTracks> m_lastNotifiedRecording{};
    std::array<float, TapeLooperScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, TapeLooperScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{-1.f, -1.f, -1.f, -1.f,
                                                                                          -1.f, -1.f, -1.f, -1.f};
    float m_bpm{120.f};
    int m_appliedGrooveVariation{0};

    std::mutex m_grooveStyleMutex;
    std::string m_currentGrooveStyle; // "<Genre>/<style>", empty until a menu pick

    std::string m_lastGrooveName; // audio thread only
    mutable std::mutex m_infoTextMutex;
    mutable std::string m_pendingInfoText;
};
