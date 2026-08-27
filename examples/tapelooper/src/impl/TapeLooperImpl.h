#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "ClockDisplayShared.h"
#include "Delays/MultiTapDelay.h"
#include "Delays/VariSpeedTapeDelay.h"
#include "Dynamics/Compressor.h"
#include "EffectBase.h"
#include "Filters/PoleMixingFilter.h"
#include "Filters/Sinc/sinc_4.h"
#include "GrooveDefaultPaths.h"
#include "Helpers/ConstructArray.h"
#include "Helpers/DebugClock.h"
#include "Helpers/StereoTrackBank.h"
#include "Modulation/RingModulator.h"
#include "Modulation/Tremolo.h"
#include "NonLinear/SimpleHysteresis.h"
#include "Parameters/LinearParameter.h"
#include "Reverbs/FdnTankGlide.h"
#include "Reverbs/ModulationDelayNoFeedback.h"
#include "Sampler/GrooveDrumPlayer.h"
#include "Sampler/GrooveKit.h"
#include "Sampler/GrooveNoteMap.h"
#include "TapeLooperLoopStorageService.h"
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

// Cutoff at the dial's own ceiling and zero resonance, so the filter is
// inaudible until touched, matching wow/flutter/gain's own default-sound
// preservation above.
constexpr float kDefaultFilterCutoff = 20000.f;
constexpr float kDefaultFilterResonance = 0.f;

// Filter mode has no dial anymore (Lua-only, via SetTrackFilter's modeName resolved
// directly through AbacDsp::findFilterIndex) - this is just the atomics' initial value.
const size_t kDefaultFilterModeIndex = AbacDsp::findFilterIndex("LP4");

// One FdnTankGlide instance per track (independent tails); order 16, not 32, bounds the
// 3x instance-count CPU cost. FdnTankGlide over FdnTank: proven in maxdiffuser, and its
// resize glides instead of clicking - see FdnTank's own class doc in Reverbs/FdnReverb.h.
constexpr size_t kReverbMaxSizePerElement = 48000;
constexpr size_t kReverbOrder = 16;
constexpr float kDefaultReverbSize = 15.f;
constexpr float kDefaultReverbDecay = 2000.f;
constexpr float kDefaultReverbSend = 0.f; // inaudible until a track's send is touched

// Per-instrument groove control (9b): gain/send are indexed by GrooveTag, not by
// track, since a script names instruments ("kick", "snare"), not raw track numbers.
constexpr float kDefaultInstrumentGain = 1.f;
constexpr float kDefaultInstrumentReverbSend = 0.f;

// Click substitutes for the groove track by loading this style (see MidiDrums/Metronome/) -
// the same requestLoadStyle() path any other groove-menu pick uses, not a separate mechanism.
constexpr std::string_view kMetronomeGrooveStyle = "Metronome/straight_4#4";
constexpr unsigned kMetronomeGrooveVariation = 0;

// Mirrors GrooveKit::splitStyleAndVariation()'s own "<style>_v<n>.mid" convention, so the
// constructor's initial requestLoad() has a matching style to fall back to once click mode
// (which drives the style directly) is switched off again.
[[nodiscard]] inline std::string styleFromGrooveFileName(const std::string_view relativeGrooveName)
{
    std::string_view stem = relativeGrooveName;
    if (const auto dot = stem.rfind('.'); dot != std::string_view::npos)
    {
        stem = stem.substr(0, dot);
    }
    const auto vPos = stem.rfind("_v");
    return std::string(vPos == std::string_view::npos ? stem : stem.substr(0, vPos));
}

// Distortion (7a): setFrequencyResponse()'s rate is exp(-2*pi*Hz/fs) - 0 Hz gives rate 1,
// an exact per-sample identity. Driven endpoint matches SimpleHysteresis's own
// "checkNonLinearity" test calibration (6000/12000).
constexpr float kHysteresisNeutralHz = 0.f;
constexpr float kHysteresisDrivenAttackHz = 6000.f;
constexpr float kHysteresisDrivenDecayHz = 12000.f;

// Echo (7b): one shared bus-independent delay line per track, per channel. Sized for the
// longest reachable time (a "1/1" division at the slowest allowed BPM).
constexpr float kMaxEchoBeats = 4.f;
constexpr size_t kMaxEchoDelaySamples = static_cast<size_t>(kMaxEchoBeats * 60.f / kMinBpm * kAssumedSampleRate) + 4;

struct SyncDivision
{
    std::string_view name;
    float quarterNotes;
};

// Same table/order as delay.json's own syncDivision drop - kept as its own copy since
// this file doesn't share a blueprint with that example.
constexpr auto kSyncDivisions = std::to_array<SyncDivision>({
    {"1/1", 4.f},
    {"1/2", 2.f},
    {"1/2.", 3.f},
    {"1/2T", 4.f / 3.f},
    {"1/4", 1.f},
    {"1/4.", 1.5f},
    {"1/4T", 2.f / 3.f},
    {"1/8", 0.5f},
    {"1/8.", 0.75f},
    {"1/8T", 1.f / 3.f},
    {"1/16", 0.25f},
    {"1/16.", 0.375f},
    {"1/16T", 1.f / 6.f},
});
constexpr size_t kDefaultEchoDivision = 4; // "1/4"

// Chorus (7c): typical short-delay modulation range: depth doubles as the wet/dry mix
// (0 = fully dry, matching every other effect's own default-sound preservation).
constexpr float kChorusBaseWidthMs = 18.f;
constexpr float kDefaultChorusRate = 0.5f;
constexpr size_t kChorusMaxSizeSamples = 4800; // 100 ms at 48 kHz, generous headroom over kChorusBaseWidthMs

// Compressor (7e): ratio 1 is a mathematically exact no-op regardless of threshold, so
// this needs no separate "off" scalar the way echo/chorus do.
constexpr float kDefaultCompThreshold = 0.f;
constexpr float kDefaultCompRatio = 1.f;
constexpr float kDefaultCompAttack = 10.f;
constexpr float kDefaultCompRelease = 100.f;

constexpr float kDefaultRingModFreq = 200.f;

constexpr float kDefaultTremoloRate = 4.f;

// Phase 8: matches the fixed order Phases 5-7 hardcoded, so an untouched track's processing
// is unchanged until a script calls SetTrackChain (reverb send is not itself a node).
constexpr TrackEffectChain kDefaultTrackEffectChain{
    {EffectNodeType::Filter, EffectNodeType::Distortion, EffectNodeType::Chorus, EffectNodeType::Echo,
     EffectNodeType::Compressor, EffectNodeType::RingMod, EffectNodeType::Tremolo},
    kMaxChainNodes};

constexpr size_t framesForLoop(const float bars, const float bpm) noexcept
{
    return static_cast<size_t>(bars * kBeatsPerBar / bpm * 60.f * kAssumedSampleRate);
}

// Sized for the longest possible loop (32 bars @ 50 BPM); the margin keeps
// that loop's read distance clear of setReadHead()'s own safety clamp.
constexpr size_t kMaxLoopFrames = framesForLoop(kMaxBars, kMinBpm);
constexpr size_t kModulationMargin = 4800;
constexpr size_t kBufferSize = kMaxLoopFrames + kModulationMargin;

// Loop save/load (9c): a full loop can be ~7M frames (32 bars @ 50 BPM), far too large to
// copy in one block - extraction/installation are spread across blocks in chunks this size.
constexpr size_t kLoopIoChunkFrames = 4096;

// Clock display (10): continuous iris feed from the final mixed output, decimated the
// same way looper decimates its own record spectrogram.
constexpr size_t kTapeSpectrogramDecimation = 4;
constexpr float kTapeSpectrogramWindowForward = 1.f / 12.f;
constexpr size_t kClockWaveformBuckets = 2048;
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
    using LoopLoadOutcome = typename TapeLooperLoopStorageService<TapeLooperDetail::kFreeTracks>::LoopLoadOutcome;
    using ReverbBus =
        AbacDsp::FdnTankGlide<TapeLooperDetail::kReverbMaxSizePerElement, TapeLooperDetail::kReverbOrder, BlockSize>;
    using EchoDelay = AbacDsp::MultiTapDelay<TapeLooperDetail::kMaxEchoDelaySamples, 1>;
    using ChorusDelay = AbacDsp::ModulationDelayNoFeedback<TapeLooperDetail::kChorusMaxSizeSamples>;
    static_assert(TapeLooperScriptEngine::kTracks == TapeLooperDetail::kFreeTracks,
                  "TapeLooperScriptEngine's per-track pool must match TapeLooperDetail::kFreeTracks");

    explicit TapeLooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_sincFilter(std::make_shared<AbacDsp::SincFilter>(sinc4))
        , m_tapeTrack(AbacDsp::constructArray<TapeTrack, TapeLooperDetail::kFreeTracks>(sampleRate, m_sincFilter))
        , m_grooveTape(sampleRate, m_sincFilter)
        , m_grooveSendTape(sampleRate, m_sincFilter)
        , m_grooveSequencer(sampleRate)
        , m_trackGainSmoother(
              AbacDsp::constructArray<AbacDsp::LinearSmoothingParameter<BlockSize>, TapeLooperDetail::kFreeTracks>(
                  TapeLooperDetail::kDefaultTrackGain))
        , m_filter(sampleRate)
        , m_reverb(AbacDsp::constructArray<ReverbBus, TapeLooperDetail::kFreeTracks>(sampleRate))
        , m_grooveReverb(sampleRate)
        , m_distortion(sampleRate)
        , m_chorus(sampleRate)
        , m_echo()
        , m_compressor(sampleRate)
        , m_ringMod(sampleRate)
        , m_tremolo(sampleRate)
        , m_instrumentGainReq(AbacDsp::constructArray<std::atomic<float>, TapeLooperScriptEngine::kInstrumentTags>(
              TapeLooperDetail::kDefaultInstrumentGain))
        , m_instrumentReverbSendReq(
              AbacDsp::constructArray<std::atomic<float>, TapeLooperScriptEngine::kInstrumentTags>(
                  TapeLooperDetail::kDefaultInstrumentReverbSend))
    {
        m_chorus.forEach([](ChorusDelay& chorus) { chorus.setWidthInMsecs(TapeLooperDetail::kChorusBaseWidthMs); });
        m_echo.forEach([](EchoDelay& echo) { echo.setTapDelay(0, 1); });
        for (auto& reverb : m_reverb)
        {
            reverb.setMinSize(TapeLooperDetail::kDefaultReverbSize * 0.5f);
            reverb.setMaxSize(TapeLooperDetail::kDefaultReverbSize);
            reverb.setDecay(TapeLooperDetail::kDefaultReverbDecay);
        }
        m_grooveReverb.setMinSize(TapeLooperDetail::kDefaultReverbSize * 0.5f);
        m_grooveReverb.setMaxSize(TapeLooperDetail::kDefaultReverbSize);
        m_grooveReverb.setDecay(TapeLooperDetail::kDefaultReverbDecay);
        m_appliedReverbSize = TapeLooperDetail::kDefaultReverbSize;
        applyLoopLengthIfChanged();
        m_grooveKit.requestLoad(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, kAbacDspDefaultGrooveName,
                                AbacDsp::BurstConfig{sampleRate, m_bpmReq.load(std::memory_order_relaxed)});
        m_currentGrooveStyle = TapeLooperDetail::styleFromGrooveFileName(kAbacDspDefaultGrooveName);
        m_scriptEngine.setSampleRate(sampleRate);
        for (auto& smoother : m_trackGainSmoother)
        {
            smoother.setMin(0.f);
            smoother.setMax(TapeLooperDetail::kMaxTrackGain);
        }
        m_extractionChunkLeftScratch.resize(TapeLooperDetail::kLoopIoChunkFrames);
        m_extractionChunkRightScratch.resize(TapeLooperDetail::kLoopIoChunkFrames);
        m_installChunkScratch.resize(TapeLooperDetail::kLoopIoChunkFrames * 2);

        m_barWaveform.assign(TapeLooperDetail::kClockWaveformBuckets, 0.f);
        m_loopWaveformPeaks.assign(TapeLooperDetail::kClockWaveformBuckets, 0.f);
        // Sample rate is that of the decimated feed, so the display's Nyquist axis
        // reflects what's actually analyzed. Sized for the longest possible loop so
        // the ring is fully painted at any BARS/BPM setting.
        const float tapeSpectrogramSampleRate =
            sampleRate / static_cast<float>(TapeLooperDetail::kTapeSpectrogramDecimation);
        m_tapeSpectrogram.setSampleRate(tapeSpectrogramSampleRate);
        m_tapeSpectrogram.setWindowForward(TapeLooperDetail::kTapeSpectrogramWindowForward);
        const auto decimatedMaxFrames =
            static_cast<float>(TapeLooperDetail::kMaxLoopFrames) / TapeLooperDetail::kTapeSpectrogramDecimation;
        const auto tapeSpectrogramSlices =
            static_cast<size_t>(decimatedMaxFrames / (1024.f * TapeLooperDetail::kTapeSpectrogramWindowForward)) + 64;
        m_tapeSpectrogram.setSlices(tapeSpectrogramSlices);
        m_tapeSpectrogramSliceBucket.assign(tapeSpectrogramSlices, 0);
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

    [[nodiscard]] float getBarPhase() const noexcept
    {
        const auto clock = computeLoopClock();
        return clock.samplesPerBar > 0
                   ? static_cast<float>(clock.barPositionFrames) / static_cast<float>(clock.samplesPerBar)
                   : 0.f;
    }

    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        const auto clock = computeLoopClock();
        return clock.loopFrames > 0
                   ? static_cast<float>(clock.loopPositionFrames) / static_cast<float>(clock.loopFrames)
                   : 0.f;
    }

    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return computeLoopClock().samplesPerBar;
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return static_cast<int>(TapeLooperDetail::kBeatsPerBar);
    }

    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        return static_cast<int>(m_appliedBars);
    }

    [[nodiscard]] std::string getBarBeatLabel() const
    {
        const auto clock = computeLoopClock();
        if (clock.samplesPerBar == 0)
        {
            return {};
        }
        const auto bar = 1 + clock.loopPositionFrames / clock.samplesPerBar;
        const auto beatLen =
            std::max<size_t>(1, clock.samplesPerBar / static_cast<size_t>(TapeLooperDetail::kBeatsPerBar));
        const auto beat = 1 + clock.barPositionFrames / beatLen;
        return std::to_string(bar) + "." + std::to_string(beat);
    }

    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return m_tapeSpectrogram.getImageSet();
    }

    [[nodiscard]] const std::vector<size_t>& getSpectrogramSliceBuckets() const noexcept
    {
        return m_tapeSpectrogramSliceBucket;
    }

    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        return m_loopWaveformPeaks;
    }

    // Required by the generic "signal" gauge wiring; here it is the bar-in-progress
    // trace shown on the clock display's inner disc.
    [[nodiscard]] const std::vector<float>& visualizeWaveData() const noexcept
    {
        return m_barWaveform;
    }

    [[nodiscard]] int getTrackClockStateA() const noexcept
    {
        return trackClockState(0);
    }

    [[nodiscard]] int getTrackClockStateB() const noexcept
    {
        return trackClockState(1);
    }

    [[nodiscard]] int getTrackClockStateC() const noexcept
    {
        return trackClockState(2);
    }

    [[nodiscard]] int getGrooveClockState() const noexcept
    {
        return m_groovePlaying ? 1 : 0;
    }

    [[nodiscard]] std::string consumeGrooveInfoText() const
    {
        std::lock_guard lock(m_infoTextMutex);
        return std::exchange(m_pendingInfoText, std::string());
    }

    // Test-support only, thin pass-through for Tapelooper_tests.cpp.
    [[nodiscard]] size_t grooveActiveVoiceCountForTest() const noexcept
    {
        return m_grooveSequencer.activeVoiceCount();
    }

    // Test-support only: exposes the tape-speed-scaled beat clock samplesPerBeat() drives.
    [[nodiscard]] size_t samplesPerBeatForTest() const noexcept
    {
        return samplesPerBeat();
    }

    // Test-support only: the applied groove-source mode, independent of whether the style it
    // resolves to has actually finished loading - see applyGrooveSourceIfChanged().
    [[nodiscard]] bool isUsingGrooveClickSourceForTest() const noexcept
    {
        return m_useClick;
    }

    // Test-support only.
    [[nodiscard]] bool isLoopSaveInProgressForTest() const noexcept
    {
        return m_extractionInProgress || m_loopStorage.isSavePending();
    }

    // Test-support only.
    [[nodiscard]] bool isLoopLoadInProgressForTest() const noexcept
    {
        return m_loopStorage.isLoadPending() || m_installInProgress;
    }

    void setLoopsDirectory(const std::string& dir)
    {
        m_loopStorage.setLoopsDirectory(dir);
    }

    [[nodiscard]] std::vector<std::string> listLoopNames() const
    {
        return m_loopStorage.listLoopNames();
    }

    [[nodiscard]] std::string currentLoopName() const
    {
        return m_loopStorage.currentLoopName();
    }

    // Message thread: sizes and dispatches the save's own scratch buffers (see
    // TapeLooperLoopStorageService::beginSave()) immediately; the audio thread picks up
    // m_extractionStartRequested next block to fill them via processLoopExtraction().
    void requestSaveLoopAs(const std::string& name, const std::string& patchParamsJson)
    {
        const auto bars = std::clamp(m_barsReq.load(std::memory_order_relaxed), TapeLooperDetail::kMinBars,
                                     TapeLooperDetail::kMaxBars);
        const auto bpm = std::max(m_bpmReq.load(std::memory_order_relaxed), TapeLooperDetail::kMinBpm);
        const auto loopFrames = TapeLooperDetail::framesForLoop(bars, bpm);
        m_loopStorage.beginSave(name, patchParamsJson, loopFrames, bars, bpm, sampleRate());
        m_extractionTotalFramesReq.store(loopFrames, std::memory_order_relaxed);
        m_extractionStartRequested.store(true, std::memory_order_release);
    }

    void requestLoadLoop(const std::string& name)
    {
        m_loopStorage.requestLoad(name);
    }

    bool deleteLoopNamed(const std::string& name)
    {
        return m_loopStorage.deleteLoopNamed(name);
    }

    bool renameLoopNamed(const std::string& oldName, const std::string& newName)
    {
        return m_loopStorage.renameLoopNamed(oldName, newName);
    }

    [[nodiscard]] LoopLoadOutcome consumeLoopLoadOutcome()
    {
        return m_loopStorage.consumeLoadOutcome();
    }

    // No-op: LoopLoadOutcome::hasConflict is always false (tapelooper doesn't embed a
    // second, WAV-side BPM to conflict with) - kept only to satisfy the generic menu's interface.
    void resolveLoopLoadBpm(float) noexcept {}

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        m_grooveKit.pollAndInstall();
        checkGrooveInfoTextChanged();
        installGrooveProgramIfChanged();
        m_scriptEngine.tickBlock(BlockSize);
        notifyUiParametersIfChanged();
        applyScriptCommands();
        applyParameters();
        checkLoopSaveStart();
        processLoopExtraction();
        m_loopStorage.checkSaveCompletion();
        checkLoopLoadCompletion();
        processLoopInstall();

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
        m_grooveSendTape.setRatio(m_tapeSpeed);

        std::array<float, 2 * BlockSize> gainedIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            gainedIn[i * 2] = in(i, 0) * m_inputGain;
            gainedIn[i * 2 + 1] = in(i, 1) * m_inputGain;
        }

        std::array<float, 2 * BlockSize> mix{};
        // Tracks stay silent for the duration of a chunked load - see processLoopInstall()'s
        // own comment for why overwriting their buffers mid-playback isn't safe otherwise.
        if (!m_installInProgress)
        {
            processTapeTracks(gainedIn, mix);
        }
        renderGrooveTrack(mix);

        // Live input always reaches the output, so a performer can hear
        // themselves while recording rather than only the looped playback.
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = gainedIn[i * 2] + mix[i * 2];
            out(i, 1) = gainedIn[i * 2 + 1] + mix[i * 2 + 1];
        }
        feedClockDisplays(out);
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

            const auto cutoffHz = m_filterCutoffReq[track].load(std::memory_order_relaxed);
            const auto resonance = m_filterResonanceReq[track].load(std::memory_order_relaxed);
            const auto modeIndex = m_filterModeReq[track].load(std::memory_order_relaxed);
            m_filter.forEachAtTrack(track,
                                    [cutoffHz, resonance, modeIndex](AbacDsp::Filter1Pole4StageSmooth& filter)
                                    {
                                        filter.setCutoffFrequency(cutoffHz);
                                        filter.setResonance(resonance);
                                        filter.setFilterCoefficients(AbacDsp::poleMixingList[modeIndex].cf);
                                    });

            m_reverbSend[track] = m_reverbSendReq[track].load(std::memory_order_relaxed);
            m_reverb[track].setDecay(m_reverbDecayReq.load(std::memory_order_relaxed));

            applyTrackEffectParameters(track);
        }

        applyReverbSizeIfChanged();
        m_grooveReverb.setDecay(m_reverbDecayReq.load(std::memory_order_relaxed));
        applyGrooveInstrumentParameters();

        const bool groovePlayReq = m_groovePlayReq.load(std::memory_order_relaxed);
        if (groovePlayReq && !m_groovePlaying)
        {
            m_grooveSequencer.resetPosition();
            m_cleanLoopPositionFrames = 0.0;
        }
        m_groovePlaying = groovePlayReq;
        applyGrooveSourceIfChanged();
        applyGrooveVariationIfChanged();
        advanceCleanLoopClock();
    }

    // Drive interpolates SimpleHysteresis toward its own calibrated non-linear extreme;
    // chorus/ring-mod store their mix fraction for processTapeTracks() to blend with, since
    // neither effect object does dry/wet mixing itself.
    void applyTrackEffectParameters(const size_t track) noexcept
    {
        const auto drive = std::clamp(m_driveReq[track].load(std::memory_order_relaxed), 0.f, 1.f);
        const auto hysteresisAttackHz =
            std::lerp(TapeLooperDetail::kHysteresisNeutralHz, TapeLooperDetail::kHysteresisDrivenAttackHz, drive);
        const auto hysteresisDecayHz =
            std::lerp(TapeLooperDetail::kHysteresisNeutralHz, TapeLooperDetail::kHysteresisDrivenDecayHz, drive);
        m_distortion.forEachAtTrack(track,
                                    [hysteresisAttackHz, hysteresisDecayHz](AbacDsp::SimpleHysteresis& hysteresis)
                                    { hysteresis.setFrequencyResponse(hysteresisAttackHz, hysteresisDecayHz); });

        const auto chorusDepth = std::clamp(m_chorusDepthReq[track].load(std::memory_order_relaxed), 0.f, 1.f);
        const auto chorusRateHz = std::max(m_chorusRateReq[track].load(std::memory_order_relaxed), 0.f);
        m_chorusMix[track] = chorusDepth;
        m_chorus.forEachAtTrack(track,
                                [chorusDepth, chorusRateHz](ChorusDelay& chorus)
                                {
                                    chorus.setModDepth(chorusDepth);
                                    chorus.setModSpeed(chorusRateHz);
                                });

        const auto echoDivisionIndex = std::min(m_echoDivisionReq[track].load(std::memory_order_relaxed),
                                                TapeLooperDetail::kSyncDivisions.size() - 1);
        m_echoFeedback[track] = std::clamp(m_echoFeedbackReq[track].load(std::memory_order_relaxed), 0.f, 0.95f);
        applyEchoDelayIfChanged(track, echoDivisionIndex);

        const auto compThresholdDb = m_compThresholdReq[track].load(std::memory_order_relaxed);
        const auto compRatio = m_compRatioReq[track].load(std::memory_order_relaxed);
        const auto compAttackMs = m_compAttackReq[track].load(std::memory_order_relaxed);
        const auto compReleaseMs = m_compReleaseReq[track].load(std::memory_order_relaxed);
        m_compressor.forEachAtTrack(track,
                                    [compThresholdDb, compRatio, compAttackMs, compReleaseMs](AbacDsp::Compressor& comp)
                                    {
                                        comp.setThresholdDb(compThresholdDb);
                                        comp.setRatio(compRatio);
                                        comp.setAttackMs(compAttackMs);
                                        comp.setReleaseMs(compReleaseMs);
                                    });

        const auto ringModFreqHz = std::max(m_ringModFreqReq[track].load(std::memory_order_relaxed), 0.f);
        m_ringModMix[track] = std::clamp(m_ringModMixReq[track].load(std::memory_order_relaxed), 0.f, 1.f);
        m_ringMod.forEachAtTrack(track,
                                 [ringModFreqHz](AbacDsp::RingModulator& rm) { rm.setFrequency(ringModFreqHz); });

        const auto tremoloRateHz = std::max(m_tremoloRateReq[track].load(std::memory_order_relaxed), 0.f);
        const auto tremoloDepth = m_tremoloDepthReq[track].load(std::memory_order_relaxed);
        const auto tremoloDrive = m_tremoloDriveReq[track].load(std::memory_order_relaxed);
        m_tremolo.forEachAtTrack(track,
                                 [tremoloRateHz, tremoloDepth, tremoloDrive](AbacDsp::Tremolo& tremolo)
                                 {
                                     tremolo.setRate(tremoloRateHz);
                                     tremolo.setDepth(tremoloDepth);
                                     tremolo.setDrive(tremoloDrive);
                                 });
    }

    // MultiTapDelay's setTapDelay() hard-jumps the read head, so re-applying an unchanged
    // width is a numeric no-op (see class comment) but a changed one clicks - unlike
    // ModulationDelayNoFeedback, it has no glide/fade mode to avoid that.
    void applyEchoDelayIfChanged(const size_t track, const size_t divisionIndex) noexcept
    {
        const auto quarterNotes = TapeLooperDetail::kSyncDivisions[divisionIndex].quarterNotes;
        const auto delaySamples =
            static_cast<size_t>(quarterNotes * 60.f / std::max(m_bpm, TapeLooperDetail::kMinBpm) * sampleRate());
        if (delaySamples == m_appliedEchoDelaySamples[track])
        {
            return;
        }
        m_appliedEchoDelaySamples[track] = delaySamples;
        m_echo.forEachAtTrack(track, [delaySamples](EchoDelay& echo) { echo.setTapDelay(0, delaySamples); });
    }

    // Resolves every instrument tag's gain/send against the currently installed kit's
    // tag -> track map; a tag the kit has no piece for is silently skipped, per-track
    // gain applies directly, and send is cached for renderGrooveTrack() to consume.
    void applyGrooveInstrumentParameters() noexcept
    {
        m_instrumentSendByTrack.fill(0.f);
        for (size_t tagIndex = 1; tagIndex < TapeLooperScriptEngine::kInstrumentTags; ++tagIndex)
        {
            const auto track = m_grooveKit.trackForTag(static_cast<AbacDsp::GrooveTag>(tagIndex));
            if (!track || *track >= AbacDsp::GrooveDrumPlayer::kMaxTracks)
            {
                continue;
            }
            m_grooveSequencer.setTrackGain(*track, m_instrumentGainReq[tagIndex].load(std::memory_order_relaxed));
            m_instrumentSendByTrack[*track] = m_instrumentReverbSendReq[tagIndex].load(std::memory_order_relaxed);
        }
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
            if (const auto v = m_scriptEngine.drainTrackFilterCommand(track))
            {
                m_filterCutoffReq[track].store(v->cutoffHz, std::memory_order_relaxed);
                m_filterResonanceReq[track].store(v->resonance, std::memory_order_relaxed);
                if (v->modeIndex)
                {
                    m_filterModeReq[track].store(*v->modeIndex, std::memory_order_relaxed);
                }
            }
            if (const auto v = m_scriptEngine.drainTrackReverbSendCommand(track))
            {
                m_reverbSendReq[track].store(*v, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackWowCommand(track))
            {
                m_wowDepthReq[track].store(v->depth, std::memory_order_relaxed);
                m_wowRateReq[track].store(v->rate, std::memory_order_relaxed);
                m_wowDriftReq[track].store(v->drift, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackFlutterCommand(track))
            {
                m_flutterDepthReq[track].store(v->depth, std::memory_order_relaxed);
                m_flutterRateReq[track].store(v->rate, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackDriveCommand(track))
            {
                m_driveReq[track].store(*v, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackChorusCommand(track))
            {
                m_chorusDepthReq[track].store(v->depth, std::memory_order_relaxed);
                m_chorusRateReq[track].store(v->rateHz, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackEchoCommand(track))
            {
                m_echoDivisionReq[track].store(v->divisionIndex, std::memory_order_relaxed);
                m_echoFeedbackReq[track].store(v->feedback, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackCompressorCommand(track))
            {
                m_compThresholdReq[track].store(v->thresholdDb, std::memory_order_relaxed);
                m_compRatioReq[track].store(v->ratio, std::memory_order_relaxed);
                m_compAttackReq[track].store(v->attackMs, std::memory_order_relaxed);
                m_compReleaseReq[track].store(v->releaseMs, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackRingModCommand(track))
            {
                m_ringModFreqReq[track].store(v->freqHz, std::memory_order_relaxed);
                m_ringModMixReq[track].store(v->mix, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackTremoloCommand(track))
            {
                m_tremoloRateReq[track].store(v->rateHz, std::memory_order_relaxed);
                m_tremoloDepthReq[track].store(v->depth, std::memory_order_relaxed);
                m_tremoloDriveReq[track].store(v->drive, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainTrackChainCommand(track))
            {
                m_chain[track] = *v;
            }
        }
        for (size_t tagIndex = 0; tagIndex < TapeLooperScriptEngine::kInstrumentTags; ++tagIndex)
        {
            if (const auto v = m_scriptEngine.drainInstrumentGainCommand(tagIndex))
            {
                m_instrumentGainReq[tagIndex].store(*v, std::memory_order_relaxed);
            }
            if (const auto v = m_scriptEngine.drainInstrumentReverbSendCommand(tagIndex))
            {
                m_instrumentReverbSendReq[tagIndex].store(*v, std::memory_order_relaxed);
            }
        }
        if (const auto v = m_scriptEngine.drainReverbSizeCommand())
        {
            m_reverbSizeReq.store(*v, std::memory_order_relaxed);
        }
        if (const auto v = m_scriptEngine.drainReverbDecayCommand())
        {
            m_reverbDecayReq.store(*v, std::memory_order_relaxed);
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
        // The loop just (re)started at its own beginning - keep the groove from drifting
        // out of sync with it rather than free-running against the old definition.
        m_grooveSequencer.resetPosition();
        m_cleanLoopPositionFrames = 0.0;
    }
#pragma GCC diagnostic pop

    struct LoopClock
    {
        size_t loopFrames{0};
        size_t samplesPerBar{0};
        size_t loopPositionFrames{0};
        size_t barPositionFrames{0};
    };

    // Advances by tapeSpeed alone, deliberately excluding wow/flutter - the same
    // rate the groove's samplesPerBeat() runs at, so "beat 1" here always matches
    // the groove's true beat 1, not the tape's wow/flutter-wobbled position.
    void advanceCleanLoopClock() noexcept
    {
        const auto loopFrames = TapeLooperDetail::framesForLoop(m_appliedBars, m_appliedBpmForLoopLength);
        if (loopFrames == 0)
        {
            m_cleanLoopPositionFrames = 0.0;
            return;
        }
        m_cleanLoopPositionFrames += static_cast<double>(BlockSize) * static_cast<double>(m_tapeSpeed);
        m_cleanLoopPositionFrames = std::fmod(m_cleanLoopPositionFrames, static_cast<double>(loopFrames));
    }

    [[nodiscard]] LoopClock computeLoopClock() const noexcept
    {
        const auto loopFrames = TapeLooperDetail::framesForLoop(m_appliedBars, m_appliedBpmForLoopLength);
        if (loopFrames == 0)
        {
            return {};
        }
        const auto bars = std::max<size_t>(1, static_cast<size_t>(m_appliedBars));
        const auto samplesPerBar = std::max<size_t>(1, loopFrames / bars);
        const auto loopPositionFrames = static_cast<size_t>(m_cleanLoopPositionFrames);
        return {loopFrames, samplesPerBar, loopPositionFrames, loopPositionFrames % samplesPerBar};
    }

    [[nodiscard]] int trackClockState(const size_t track) const noexcept
    {
        if (m_recording[track])
        {
            return 2;
        }
        return m_playing[track] ? 1 : 0;
    }

    // Feeds the continuous iris/waveform displays from the final mixed output - never
    // gated by record/play state, since the clock display always shows what's audible.
    // Debug-only, temporary: prints on every bar.beat change so it can be
    // correlated against GrooveDrumPlayer's own trigger log, both timestamped
    // from the same shared AbacDsp::debugElapsedMicroseconds() epoch.
    void logBarBeatIfChanged(const LoopClock& clock) noexcept
    {
        if (clock.samplesPerBar == 0)
        {
            return;
        }
        const auto bar = 1 + clock.loopPositionFrames / clock.samplesPerBar;
        const auto beatLen =
            std::max<size_t>(1, clock.samplesPerBar / static_cast<size_t>(TapeLooperDetail::kBeatsPerBar));
        const auto beat = 1 + clock.barPositionFrames / beatLen;
        if (bar == m_lastPrintedBar && beat == m_lastPrintedBeat)
        {
            return;
        }
        m_lastPrintedBar = bar;
        m_lastPrintedBeat = beat;
        std::cout << std::format("{:10} us  bar.beat {}.{}\n", AbacDsp::debugElapsedMicroseconds(), bar, beat);
    }

    void feedClockDisplays(const AbacDsp::AudioBuffer<2, BlockSize>& out) noexcept
    {
        const auto clock = computeLoopClock();
        logBarBeatIfChanged(clock);
        std::array<float, BlockSize> monoDecimated{};
        size_t decimatedCount = 0;
        float blockPeak = 0.f;
        float blockLast = 0.f;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float mono = 0.5f * (out(i, 0) + out(i, 1));
            blockPeak = std::max(blockPeak, std::abs(mono));
            blockLast = mono;
            if (m_tapeSpectrogramDecimatePhase == 0)
            {
                monoDecimated[decimatedCount++] = mono;
            }
            m_tapeSpectrogramDecimatePhase =
                (m_tapeSpectrogramDecimatePhase + 1) % TapeLooperDetail::kTapeSpectrogramDecimation;
        }
        tagUpcomingSpectrogramSlices(decimatedCount, clock);
        m_tapeSpectrogram.processBlock(std::span<const float>{monoDecimated.data(), decimatedCount});

        constexpr auto kBuckets = TapeLooperDetail::kClockWaveformBuckets;
        if (clock.loopFrames > 0)
        {
            const auto bucket = std::min(kBuckets - 1, clock.loopPositionFrames * kBuckets / clock.loopFrames);
            m_loopWaveformPeaks[bucket] = blockPeak;
        }
        if (clock.samplesPerBar > 0)
        {
            const auto bucket = std::min(kBuckets - 1, clock.barPositionFrames * kBuckets / clock.samplesPerBar);
            m_barWaveform[bucket] = blockLast;
        }
    }

    // Backdates by half the analysis window's real-sample span (a Hann window's
    // energy centroid), converted to loop-position units via tapeSpeed, so a slice
    // is tagged where its content actually peaks, not where its window finished.
    [[nodiscard]] size_t spectrogramTagBucket(const size_t fftLength, const LoopClock& clock) const noexcept
    {
        const auto halfWindowRealSamples = (fftLength * TapeLooperDetail::kTapeSpectrogramDecimation) / 2;
        const auto loopFramesD = static_cast<double>(clock.loopFrames);
        const auto backdate = static_cast<size_t>(
            std::fmod(static_cast<double>(halfWindowRealSamples) * static_cast<double>(m_tapeSpeed), loopFramesD));
        const auto taggedPosition = (clock.loopPositionFrames + clock.loopFrames - backdate) % clock.loopFrames;
        return std::min(TapeLooperDetail::kIrisAngularBuckets - 1,
                        taggedPosition * TapeLooperDetail::kIrisAngularBuckets / clock.loopFrames);
    }

    // Mirrors m_tapeSpectrogram's own fftLength/forwardLength accumulation to predict
    // exactly when a new FFT window completes, and stamps it with the position it was
    // actually fed at - not wherever the playhead is once it's later consumed.
    void tagUpcomingSpectrogramSlices(const size_t decimatedCount, const LoopClock& clock) noexcept
    {
        if (m_tapeSpectrogramSliceBucket.empty())
        {
            return;
        }
        const auto fftLength = static_cast<size_t>(m_tapeSpectrogram.fftLength());
        const auto forwardLen = static_cast<size_t>(m_tapeSpectrogram.forwardLength());
        if (fftLength == 0 || forwardLen == 0)
        {
            return;
        }
        const auto bucket = clock.loopFrames > 0 ? spectrogramTagBucket(fftLength, clock) : size_t{0};
        m_tapeSpectrogramWindowFill += decimatedCount;
        while (m_tapeSpectrogramWindowFill >= fftLength)
        {
            m_tapeSpectrogramSliceBucket[m_tapeSpectrogramNextSliceIndex] = bucket;
            m_tapeSpectrogramNextSliceIndex =
                (m_tapeSpectrogramNextSliceIndex + 1) % m_tapeSpectrogramSliceBucket.size();
            m_tapeSpectrogramWindowFill -= forwardLen;
        }
    }

    // setMinSize()/setMaxSize() re-randomize every delay line's length (computeDelaySizes()),
    // cheap only because it's skipped when nothing changed - unlike setDecay(), which is a
    // pure gain recompute and safe to call every block regardless.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    void applyReverbSizeIfChanged() noexcept
    {
        const auto size = m_reverbSizeReq.load(std::memory_order_relaxed);
        if (size == m_appliedReverbSize)
        {
            return;
        }
        m_appliedReverbSize = size;
        for (auto& reverb : m_reverb)
        {
            reverb.setMinSize(size * 0.5f);
            reverb.setMaxSize(size);
        }
        m_grooveReverb.setMinSize(size * 0.5f);
        m_grooveReverb.setMaxSize(size);
    }
#pragma GCC diagnostic pop

    // Click is just the Metronome style loaded through the same one conductor
    // (GrooveDrumPlayer) every other groove uses - not a second playback mechanism.
    void applyGrooveSourceIfChanged()
    {
        const bool useClick = m_grooveSourceReq.load(std::memory_order_relaxed);
        if (useClick == m_useClick)
        {
            return;
        }
        m_useClick = useClick;
        if (m_useClick)
        {
            m_grooveKit.requestLoadStyle(
                kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, std::string(TapeLooperDetail::kMetronomeGrooveStyle),
                TapeLooperDetail::kMetronomeGrooveVariation, AbacDsp::BurstConfig{sampleRate(), m_bpm});
            return;
        }
        std::string style;
        {
            std::lock_guard lock(m_grooveStyleMutex);
            style = m_currentGrooveStyle;
        }
        if (!style.empty())
        {
            m_grooveKit.requestLoadStyle(kAbacDspDrumSamplesDir, kAbacDspMidiDrumsDir, style,
                                         static_cast<unsigned>(m_appliedGrooveVariation),
                                         AbacDsp::BurstConfig{sampleRate(), m_bpm});
        }
    }

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

    // repositionGrooveSequencer() undoes setGroove()'s reset-to-0 so a
    // style/variation swap keeps beat position instead of restarting the pattern.
    void installGrooveProgramIfChanged()
    {
        const auto* program = m_grooveKit.program();
        const bool programChanged = program != m_lastGrooveProgram;
        const auto* previousProgram = m_lastGrooveProgram;
        const double previousTickPos = m_grooveSequencer.tickPosition();
        if (programChanged)
        {
            m_lastGrooveProgram = program;
        }
        m_grooveSequencer.setLibrary(m_grooveKit.library());
        m_grooveSequencer.setTrackNames(m_grooveKit.installedTrackNames());
        m_grooveSequencer.setGroove(program);
        if (programChanged && previousProgram != nullptr && program != nullptr)
        {
            repositionGrooveSequencer(*program, previousTickPos, *previousProgram);
        }
    }

    // Wraps via fmod, so a shorter new groove just loops sooner rather than
    // reading past its own loopLengthTicks.
    void repositionGrooveSequencer(const AbacDsp::GrooveProgram& program, const double previousTickPos,
                                   const AbacDsp::GrooveProgram& previousProgram) noexcept
    {
        const double previousTicksPerBeat =
            static_cast<double>(std::max<uint16_t>(1, previousProgram.ticksPerQuarterNote));
        const double beatPosition = previousTickPos / previousTicksPerBeat;
        const double loopLengthTicks = static_cast<double>(std::max<uint32_t>(1, program.loopLengthTicks));
        double newTickPos = std::fmod(beatPosition * static_cast<double>(program.ticksPerQuarterNote), loopLengthTicks);
        if (newTickPos < 0.0)
        {
            newTickPos += loopLengthTicks;
        }
        const auto& triggers = program.triggers;
        const auto nextTriggerIndex = static_cast<size_t>(std::distance(
            triggers.begin(), std::upper_bound(triggers.begin(), triggers.end(), newTickPos,
                                               [](const double tick, const AbacDsp::GrooveTrigger& trigger)
                                               { return tick < static_cast<double>(trigger.tick); })));
        m_grooveSequencer.primeTickState(newTickPos, nextTriggerIndex);
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

    // One function per node type, all sharing the (track, inL, inR) -> (outL, outR) shape so
    // runTrackEffectChain() can dispatch through them uniformly; a node's own mix (chorus,
    // ring-mod) always runs regardless of its blend, so raising it later finds warm state.
    [[nodiscard]] std::pair<float, float> stepFilterNode(const size_t track, const float inL, const float inR) noexcept
    {
        return {m_filter.left(track).step(inL), m_filter.right(track).step(inR)};
    }

    [[nodiscard]] std::pair<float, float> stepDistortionNode(const size_t track, const float inL,
                                                             const float inR) noexcept
    {
        return {m_distortion.left(track).step(inL), m_distortion.right(track).step(inR)};
    }

    [[nodiscard]] std::pair<float, float> stepChorusNode(const size_t track, const float inL, const float inR) noexcept
    {
        const auto mix = m_chorusMix[track];
        return {inL + mix * (m_chorus.left(track).step(inL) - inL),
                inR + mix * (m_chorus.right(track).step(inR) - inR)};
    }

    // Feedback also doubles as the send level - see TapeLooperEchoCommand's own doc comment.
    [[nodiscard]] std::pair<float, float> stepEchoNode(const size_t track, const float inL, const float inR) noexcept
    {
        const auto feedback = m_echoFeedback[track];
        const auto contribL = feedback * m_echo.left(track).readTap(0);
        const auto contribR = feedback * m_echo.right(track).readTap(0);
        m_echo.left(track).write(inL + contribL);
        m_echo.right(track).write(inR + contribR);
        return {inL + contribL, inR + contribR};
    }

    [[nodiscard]] std::pair<float, float> stepCompressorNode(const size_t track, const float inL,
                                                             const float inR) noexcept
    {
        return {m_compressor.left(track).step(inL), m_compressor.right(track).step(inR)};
    }

    [[nodiscard]] std::pair<float, float> stepRingModNode(const size_t track, const float inL, const float inR) noexcept
    {
        const auto mix = m_ringModMix[track];
        return {inL + mix * (m_ringMod.left(track).step(inL) - inL),
                inR + mix * (m_ringMod.right(track).step(inR) - inR)};
    }

    [[nodiscard]] std::pair<float, float> stepTremoloNode(const size_t track, const float inL, const float inR) noexcept
    {
        return {m_tremolo.left(track).step(inL), m_tremolo.right(track).step(inR)};
    }

    [[nodiscard]] std::pair<float, float> stepNode(const EffectNodeType type, const size_t track, const float inL,
                                                   const float inR) noexcept
    {
        switch (type)
        {
            case EffectNodeType::Filter:
                return stepFilterNode(track, inL, inR);
            case EffectNodeType::Distortion:
                return stepDistortionNode(track, inL, inR);
            case EffectNodeType::Chorus:
                return stepChorusNode(track, inL, inR);
            case EffectNodeType::Echo:
                return stepEchoNode(track, inL, inR);
            case EffectNodeType::Compressor:
                return stepCompressorNode(track, inL, inR);
            case EffectNodeType::RingMod:
                return stepRingModNode(track, inL, inR);
            case EffectNodeType::Tremolo:
                return stepTremoloNode(track, inL, inR);
        }
        return {inL, inR};
    }

    // Reverb send is not a chain node (see plan's scope cut) - it always taps whatever this
    // returns, i.e. the chain's last node's output.
    [[nodiscard]] std::pair<float, float> runTrackEffectChain(const size_t track, const float inL,
                                                              const float inR) noexcept
    {
        auto curL = inL;
        auto curR = inR;
        const auto& chain = m_chain[track];
        for (size_t i = 0; i < chain.length; ++i)
        {
            std::tie(curL, curR) = stepNode(chain.nodes[i], track, curL, curR);
        }
        return {curL, curR};
    }

    // Audio thread: starts a chunked extraction once the message thread's request lands -
    // writeHead() must be read here, not from requestSaveLoopAs(), since the tape's ring
    // buffer is only safe to touch from the audio thread.
    void checkLoopSaveStart() noexcept
    {
        if (!m_extractionStartRequested.exchange(false, std::memory_order_acquire) || m_extractionInProgress ||
            m_installInProgress)
        {
            return;
        }
        for (size_t t = 0; t < TapeLooperDetail::kFreeTracks; ++t)
        {
            m_extractionBaseline[t] = m_tapeTrack[t].writeHead();
        }
        m_extractionTotalFrames = m_extractionTotalFramesReq.load(std::memory_order_relaxed);
        m_extractionCursor = 0;
        m_extractionInProgress = true;
    }

    // Audio thread: copies one bounded chunk per track per block - a full loop can be ~7M
    // frames, far too large to copy in one block - until the whole loop has been extracted,
    // then hands the assembled buffers to the background save worker.
    void processLoopExtraction() noexcept
    {
        if (!m_extractionInProgress)
        {
            return;
        }
        const auto chunkFrames =
            std::min(TapeLooperDetail::kLoopIoChunkFrames, m_extractionTotalFrames - m_extractionCursor);
        for (size_t t = 0; t < TapeLooperDetail::kFreeTracks; ++t)
        {
            const auto& raw = m_tapeTrack[t].getBuffer();
            for (size_t i = 0; i < chunkFrames; ++i)
            {
                const auto srcFrame = (m_extractionBaseline[t] + TapeLooperDetail::kBufferSize -
                                       m_extractionTotalFrames + m_extractionCursor + i) %
                                      TapeLooperDetail::kBufferSize;
                m_extractionChunkLeftScratch[i] = raw[srcFrame * 2];
                m_extractionChunkRightScratch[i] = raw[srcFrame * 2 + 1];
            }
            m_loopStorage.writeSaveChunk(t, m_extractionCursor,
                                         std::span(m_extractionChunkLeftScratch).first(chunkFrames),
                                         std::span(m_extractionChunkRightScratch).first(chunkFrames));
        }
        m_extractionCursor += chunkFrames;
        if (m_extractionCursor >= m_extractionTotalFrames)
        {
            m_loopStorage.finishSave();
            m_extractionInProgress = false;
        }
    }

    // Audio thread: picks up a completed background load and starts its chunked install.
    void checkLoopLoadCompletion() noexcept
    {
        if (m_installInProgress || m_extractionInProgress)
        {
            return;
        }
        auto result = m_loopStorage.pollLoadCompletion();
        if (!result)
        {
            return;
        }
        m_installResult = std::move(*result);
        m_installTotalFrames = m_installResult.trackLeft[0].size();
        m_installCursor = 0;
        m_installInProgress = m_installTotalFrames > 0;
    }

    // Audio thread: writes one bounded chunk per track per block, then repositions every
    // track's read head to the newly loaded length. Tracks stay silent meanwhile (see
    // processBlock()) since the old loop's read/write heads could land inside this range.
    void processLoopInstall() noexcept
    {
        if (!m_installInProgress)
        {
            return;
        }
        const auto chunkFrames = std::min(TapeLooperDetail::kLoopIoChunkFrames, m_installTotalFrames - m_installCursor);
        for (size_t t = 0; t < TapeLooperDetail::kFreeTracks; ++t)
        {
            for (size_t i = 0; i < chunkFrames; ++i)
            {
                m_installChunkScratch[i * 2] = m_installResult.trackLeft[t][m_installCursor + i];
                m_installChunkScratch[i * 2 + 1] = m_installResult.trackRight[t][m_installCursor + i];
            }
            m_tapeTrack[t].installLoopChunk(m_installCursor, std::span(m_installChunkScratch).first(chunkFrames * 2));
        }
        m_installCursor += chunkFrames;
        if (m_installCursor >= m_installTotalFrames)
        {
            for (auto& tape : m_tapeTrack)
            {
                tape.finishLoopLoad(m_installTotalFrames);
                // setReadHead() anchors to the write position readBlock() last reported,
                // stale here since readBlock() was skipped while installing - prime it first.
                std::array<float, 2 * BlockSize> primer{};
                tape.readBlock(0, primer);
                tape.setReadHead(0, static_cast<float>(m_installTotalFrames), true);
            }
            m_barsReq.store(m_installResult.bars, std::memory_order_relaxed);
            m_bpmReq.store(m_installResult.bpm, std::memory_order_relaxed);
            m_installInProgress = false;
        }
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

            // Fed only while playing, but ticked every block regardless - its own tail
            // keeps ringing after Play (or the send) drops, the way a real room does.
            std::array<float, BlockSize> reverbSendIn{};
            if (m_playing[track])
            {
                const auto send = m_reverbSend[track];
                for (size_t i = 0; i < BlockSize; ++i)
                {
                    const auto gain = m_trackGainSmoother[track].getValue(i);
                    const auto [wetChainL, wetChainR] =
                        runTrackEffectChain(track, tapeOut[i * 2] * gain, tapeOut[i * 2 + 1] * gain);
                    mix[i * 2] += wetChainL;
                    mix[i * 2 + 1] += wetChainR;
                    reverbSendIn[i] = (wetChainL + wetChainR) * 0.5f * send;
                }
            }

            std::array<float, BlockSize> wetL{};
            std::array<float, BlockSize> wetR{};
            m_reverb[track].processBlockSplitAdd(reverbSendIn.data(), wetL.data(), wetR.data());
            for (size_t i = 0; i < BlockSize; ++i)
            {
                mix[i * 2] += wetL[i];
                mix[i * 2 + 1] += wetR[i];
            }
        }
    }

    // Tape speed multiplies the effective tempo, same as slowing/speeding a physical tape
    // changes the pitch and rate of everything already on it - 2x speed means 2x BPM.
    [[nodiscard]] size_t samplesPerBeat() const noexcept
    {
        return static_cast<size_t>(sampleRate() * 60.f / std::max(1.f, m_bpm * m_tapeSpeed));
    }

    // The per-instrument reverb send is generated alongside the main groove signal, weighted
    // by m_instrumentSendByTrack, and carried through its own varispeed tape so it stays in
    // sync with the main signal's own tape speed/wow/flutter.
    void renderGrooveTrack(std::array<float, 2 * BlockSize>& mix) noexcept
    {
        std::array<float, 2 * BlockSize> grooveOut{};
        m_grooveTape.readBlock(0, grooveOut);
        std::array<float, 2 * BlockSize> sendOut{};
        m_grooveSendTape.readBlock(0, sendOut);

        std::array<float, 2 * BlockSize> grooveIn{};
        std::array<float, 2 * BlockSize> sendIn{};
        if (m_groovePlaying)
        {
            const auto spb = samplesPerBeat();
            for (size_t i = 0; i < BlockSize; ++i)
            {
                std::array<std::array<float, AbacDsp::GrooveDrumPlayer::kChannels>,
                           AbacDsp::GrooveDrumPlayer::kMaxTracks>
                    perTrack{};
                const auto frame = m_grooveSequencer.advanceSample(spb, &perTrack);
                grooveIn[i * 2] = frame[0];
                grooveIn[i * 2 + 1] = frame[1];

                float sendL = 0.f;
                float sendR = 0.f;
                for (size_t track = 0; track < AbacDsp::GrooveDrumPlayer::kMaxTracks; ++track)
                {
                    const auto send = m_instrumentSendByTrack[track];
                    sendL += perTrack[track][0] * send;
                    sendR += perTrack[track][1] * send;
                }
                sendIn[i * 2] = sendL;
                sendIn[i * 2 + 1] = sendR;
            }
        }
        m_grooveTape.feed(grooveIn);
        m_grooveSendTape.feed(sendIn);

        std::array<float, BlockSize> reverbSendMono{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            reverbSendMono[i] = (sendOut[i * 2] + sendOut[i * 2 + 1]) * 0.5f;
        }
        std::array<float, BlockSize> wetL{};
        std::array<float, BlockSize> wetR{};
        m_grooveReverb.processBlockSplitAdd(reverbSendMono.data(), wetL.data(), wetR.data());

        if (m_groovePlaying)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                mix[i * 2] += grooveOut[i * 2] * m_grooveLevel;
                mix[i * 2 + 1] += grooveOut[i * 2 + 1] * m_grooveLevel;
            }
        }
        for (size_t i = 0; i < BlockSize; ++i)
        {
            mix[i * 2] += wetL[i];
            mix[i * 2 + 1] += wetR[i];
        }
    }

    std::shared_ptr<AbacDsp::SincFilter> m_sincFilter;
    std::array<TapeTrack, TapeLooperDetail::kFreeTracks> m_tapeTrack;
    TapeTrack m_grooveTape;
    TapeTrack
        m_grooveSendTape; // carries the per-instrument reverb send through the same varispeed path as m_grooveTape
    GrooveKit m_grooveKit;
    AbacDsp::GrooveDrumPlayer m_grooveSequencer;
    const AbacDsp::GrooveProgram* m_lastGrooveProgram{nullptr};
    std::array<AbacDsp::LinearSmoothingParameter<BlockSize>, TapeLooperDetail::kFreeTracks> m_trackGainSmoother;
    AbacDsp::StereoTrackBank<AbacDsp::Filter1Pole4StageSmooth, TapeLooperDetail::kFreeTracks> m_filter;
    std::array<ReverbBus, TapeLooperDetail::kFreeTracks> m_reverb;
    ReverbBus m_grooveReverb;
    AbacDsp::StereoTrackBank<AbacDsp::SimpleHysteresis, TapeLooperDetail::kFreeTracks> m_distortion;
    AbacDsp::StereoTrackBank<ChorusDelay, TapeLooperDetail::kFreeTracks> m_chorus;
    AbacDsp::StereoTrackBank<EchoDelay, TapeLooperDetail::kFreeTracks> m_echo;
    AbacDsp::StereoTrackBank<AbacDsp::Compressor, TapeLooperDetail::kFreeTracks> m_compressor;
    AbacDsp::StereoTrackBank<AbacDsp::RingModulator, TapeLooperDetail::kFreeTracks> m_ringMod;
    AbacDsp::StereoTrackBank<AbacDsp::Tremolo, TapeLooperDetail::kFreeTracks> m_tremolo;
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
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_filterCutoffReq{
        TapeLooperDetail::kDefaultFilterCutoff, TapeLooperDetail::kDefaultFilterCutoff,
        TapeLooperDetail::kDefaultFilterCutoff};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_filterResonanceReq{
        TapeLooperDetail::kDefaultFilterResonance, TapeLooperDetail::kDefaultFilterResonance,
        TapeLooperDetail::kDefaultFilterResonance};
    std::array<std::atomic<size_t>, TapeLooperDetail::kFreeTracks> m_filterModeReq{
        TapeLooperDetail::kDefaultFilterModeIndex, TapeLooperDetail::kDefaultFilterModeIndex,
        TapeLooperDetail::kDefaultFilterModeIndex};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_reverbSendReq{TapeLooperDetail::kDefaultReverbSend,
                                                                                  TapeLooperDetail::kDefaultReverbSend,
                                                                                  TapeLooperDetail::kDefaultReverbSend};
    std::atomic<float> m_reverbSizeReq{TapeLooperDetail::kDefaultReverbSize};
    std::atomic<float> m_reverbDecayReq{TapeLooperDetail::kDefaultReverbDecay};
    std::array<std::atomic<float>, TapeLooperScriptEngine::kInstrumentTags> m_instrumentGainReq;
    std::array<std::atomic<float>, TapeLooperScriptEngine::kInstrumentTags> m_instrumentReverbSendReq;

    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_driveReq{0.f, 0.f, 0.f};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_chorusDepthReq{0.f, 0.f, 0.f};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_chorusRateReq{TapeLooperDetail::kDefaultChorusRate,
                                                                                  TapeLooperDetail::kDefaultChorusRate,
                                                                                  TapeLooperDetail::kDefaultChorusRate};
    std::array<std::atomic<size_t>, TapeLooperDetail::kFreeTracks> m_echoDivisionReq{
        TapeLooperDetail::kDefaultEchoDivision, TapeLooperDetail::kDefaultEchoDivision,
        TapeLooperDetail::kDefaultEchoDivision};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_echoFeedbackReq{0.f, 0.f, 0.f};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_compThresholdReq{
        TapeLooperDetail::kDefaultCompThreshold, TapeLooperDetail::kDefaultCompThreshold,
        TapeLooperDetail::kDefaultCompThreshold};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_compRatioReq{
        TapeLooperDetail::kDefaultCompRatio, TapeLooperDetail::kDefaultCompRatio, TapeLooperDetail::kDefaultCompRatio};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_compAttackReq{TapeLooperDetail::kDefaultCompAttack,
                                                                                  TapeLooperDetail::kDefaultCompAttack,
                                                                                  TapeLooperDetail::kDefaultCompAttack};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_compReleaseReq{
        TapeLooperDetail::kDefaultCompRelease, TapeLooperDetail::kDefaultCompRelease,
        TapeLooperDetail::kDefaultCompRelease};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_ringModFreqReq{
        TapeLooperDetail::kDefaultRingModFreq, TapeLooperDetail::kDefaultRingModFreq,
        TapeLooperDetail::kDefaultRingModFreq};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_ringModMixReq{0.f, 0.f, 0.f};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_tremoloRateReq{
        TapeLooperDetail::kDefaultTremoloRate, TapeLooperDetail::kDefaultTremoloRate,
        TapeLooperDetail::kDefaultTremoloRate};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_tremoloDepthReq{0.f, 0.f, 0.f};
    std::array<std::atomic<float>, TapeLooperDetail::kFreeTracks> m_tremoloDriveReq{0.f, 0.f, 0.f};

    float m_tapeSpeed{1.f};
    float m_appliedBars{0.f};
    float m_appliedBpmForLoopLength{0.f};
    double m_cleanLoopPositionFrames{0.0};
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
    bool m_useClick{false}; // applied state; edge-triggers applyGrooveSourceIfChanged()'s style swap
    std::array<bool, TapeLooperDetail::kFreeTracks> m_lastNotifiedRecording{};
    std::array<float, TapeLooperScriptEngine::kMaxLuaParams> m_luaParamValues{};
    std::array<float, TapeLooperScriptEngine::kMaxLuaParams> m_lastNotifiedLuaParamValues{-1.f, -1.f, -1.f, -1.f,
                                                                                          -1.f, -1.f, -1.f, -1.f};
    float m_bpm{120.f};
    int m_appliedGrooveVariation{0};
    std::array<float, TapeLooperDetail::kFreeTracks> m_reverbSend{TapeLooperDetail::kDefaultReverbSend,
                                                                  TapeLooperDetail::kDefaultReverbSend,
                                                                  TapeLooperDetail::kDefaultReverbSend};
    float m_appliedReverbSize{0.f}; // 0 forces the first applyReverbSizeIfChanged() to apply
    // Rebuilt every block by applyGrooveInstrumentParameters() from the tag-indexed
    // send atomics, resolved through the currently installed kit's tag -> track map.
    std::array<float, AbacDsp::GrooveDrumPlayer::kMaxTracks> m_instrumentSendByTrack{};

    std::array<float, TapeLooperDetail::kFreeTracks> m_chorusMix{0.f, 0.f, 0.f};
    std::array<float, TapeLooperDetail::kFreeTracks> m_ringModMix{0.f, 0.f, 0.f};
    std::array<float, TapeLooperDetail::kFreeTracks> m_echoFeedback{0.f, 0.f, 0.f};
    std::array<size_t, TapeLooperDetail::kFreeTracks> m_appliedEchoDelaySamples{};
    std::array<TrackEffectChain, TapeLooperDetail::kFreeTracks> m_chain{TapeLooperDetail::kDefaultTrackEffectChain,
                                                                        TapeLooperDetail::kDefaultTrackEffectChain,
                                                                        TapeLooperDetail::kDefaultTrackEffectChain};

    std::mutex m_grooveStyleMutex;
    std::string m_currentGrooveStyle; // "<Genre>/<style>", empty until a menu pick

    std::string m_lastGrooveName; // audio thread only
    mutable std::mutex m_infoTextMutex;
    mutable std::string m_pendingInfoText;

    TapeLooperLoopStorageService<TapeLooperDetail::kFreeTracks> m_loopStorage;
    std::atomic<bool> m_extractionStartRequested{false};
    std::atomic<size_t> m_extractionTotalFramesReq{0};
    bool m_extractionInProgress{false};
    std::array<size_t, TapeLooperDetail::kFreeTracks> m_extractionBaseline{};
    size_t m_extractionTotalFrames{0};
    size_t m_extractionCursor{0};
    std::vector<float> m_extractionChunkLeftScratch;
    std::vector<float> m_extractionChunkRightScratch;

    bool m_installInProgress{false};
    size_t m_installCursor{0};
    size_t m_installTotalFrames{0};
    TapeLooperLoopStorageService<TapeLooperDetail::kFreeTracks>::LoopLoadResult m_installResult;
    std::vector<float> m_installChunkScratch;

    // Clock display (10): fed unconditionally every block from the final mixed
    // output, never gated by record/play state and never reset.
    AbacDsp::SimpleSpectrogram m_tapeSpectrogram;
    size_t m_tapeSpectrogramDecimatePhase{0};
    size_t m_lastPrintedBar{0};  // debug-only, temporary
    size_t m_lastPrintedBeat{0}; // debug-only, temporary
    std::vector<size_t> m_tapeSpectrogramSliceBucket;
    size_t m_tapeSpectrogramNextSliceIndex{0};
    size_t m_tapeSpectrogramWindowFill{0};
    std::vector<float> m_barWaveform;
    std::vector<float> m_loopWaveformPeaks;
};
