#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
#include "DrumAnalysisReport.h"
#include "DrumHitMatcher.h"
#include "DrumMap.h"
#include "DrumMidiDecoder.h"
#include "DrumPattern.h"
#include "DrumTrainerDrumPaths.h"
#include "DrumTrainerScriptEngine.h"
#include "Sampler/DrumVoiceKit.h"
#include "Sampler/SamplePlayerBasic.h"

using AbacDsp::ClickAccent;
using AbacDsp::SubdivType;

struct RhythmPreset
{
    std::string_view name;
    uint8_t barBeats;
    std::array<ClickAccent, 16> pattern; // only first barBeats entries are used
    SubdivType subdivType;
    bool hasSwing;
};

template <size_t BlockSize>
class DrumTrainerImpl final : public EffectBase
{
  public:
    static constexpr int kDefaultPresetIndex = 6; // 4/4 8th

    explicit DrumTrainerImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_click(sampleRate)
        , m_seq(sampleRate)
        , m_drumVoices(makeDrumVoicePool(sampleRate))
    {
        const auto& preset = kPresets[static_cast<size_t>(m_presetIndex)];
        m_seq.setBeatsPerBar(preset.barBeats);
        m_seq.setSubdivType(preset.subdivType);
        m_seq.setSwingRatio(kDefaultSwingRatio);
        m_seq.setBpm(m_bpm);
        setDrumKit(m_drumKitIndex);
        m_midiDecoder.setMap(m_drumMapFile.map, sampleRate);
    }

    void setBpm(const float value)
    {
        applyBpm(std::clamp(value, 40.f, 250.f));
    }

    void setHostSync(const bool value) noexcept
    {
        m_hostSync = value;
    }

    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return m_hostSync;
    }

    [[nodiscard]] float currentClickBpm() const noexcept
    {
        return m_bpm;
    }

    void setDropBars(const size_t value)
    {
        m_dropModeIndex = static_cast<int>(std::min(value, kDropBarModes.size() - 1));
        m_barCount = 0;
    }

    void setMetroVolume(const float valueDb) noexcept
    {
        m_click.setVolumeDb(valueDb);
        m_metroVolumeGain = dbToLinearGain(valueDb);
    }

    void setSubVolume(const float valueDb) noexcept
    {
        m_click.setSubVolumeDb(valueDb);
        m_subVolumeGain = dbToLinearGain(valueDb);
    }

    // How much later than the engine's clock a hit arrives because of audio output and MIDI
    // latency; subtracted from every captured hit.
    void setLatencyComp(const float valueMs) noexcept
    {
        m_latencySamples = static_cast<int64_t>(std::lround(valueMs * 0.001f * sampleRate()));
    }

    // Consumes the "a report is waiting" flag set on the audio thread when a take that captured
    // something ends; the message thread then builds and exports the report.
    [[nodiscard]] bool consumeAnalysisReportReady() noexcept
    {
        return m_reportReady.exchange(false, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::string buildAnalysisReportHtml() const
    {
        const std::string_view name = scriptSetsBeatsPerBar() ? "Script" : kPresets[presetSlot()].name;
        return DrumAnalysis::buildReportHtml(m_takeSetup, m_capturedHits.hits(), name, m_scriptName);
    }

    [[nodiscard]] std::string analysisFileStem() const
    {
        return DrumAnalysis::fileNameStem(m_scriptName);
    }

    void setInputVolume(const float valueDb) noexcept
    {
        m_inputGain = std::pow(10.f, valueDb / 20.f);
    }

    void setOnOff(const bool value) noexcept
    {
        m_running = value;
    }

    void setAnalysisMode(const bool value) noexcept
    {
        m_analysisMode.store(value, std::memory_order_relaxed);
    }

    void setAnalysisBars(const size_t index) noexcept
    {
        m_analysisBarsIndex.store(std::min(index, kAnalysisBarCounts.size() - 1), std::memory_order_relaxed);
    }

    // True once per take that stopped itself at its bar count; the message thread then flips the
    // Analysis switch off to match.
    [[nodiscard]] bool consumeAnalysisAutoStopped() noexcept
    {
        return m_analysisAutoStopped.exchange(false, std::memory_order_acq_rel);
    }

    // Empty while no take is active.
    [[nodiscard]] std::string analysisStatusText() const
    {
        const uint64_t packed = m_analysisProgress.load(std::memory_order_relaxed);
        const auto phase = static_cast<AnalysisPhase>(packed & 0xFFu);
        const size_t bar = static_cast<size_t>((packed >> 8) & 0xFFFFu) + 1;
        const size_t total = static_cast<size_t>((packed >> 24) & 0xFFFFu);
        switch (phase)
        {
            case AnalysisPhase::Idle:
                return {};
            case AnalysisPhase::Waiting:
                return "Analysis: waiting for the next bar";
            case AnalysisPhase::CountIn:
                return std::format("Analysis: count-in bar {}/{}", bar, kAnalysisCountInBars);
            case AnalysisPhase::Capturing:
                return total == 0 ? std::format("Analysis: bar {}", bar)
                                  : std::format("Analysis: bar {}/{}", bar, total);
        }
        return {};
    }

    // Runs the pattern script for the current Preset's bar length. A failed script leaves
    // the previously published pattern playing and the error readable via scriptError().
    bool setScript(const std::string_view source)
    {
        m_scriptSource = std::string{source};
        return applyScript();
    }

    void setImportResolver(DrumTrainerScriptEngine::ImportResolver resolver)
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

    [[nodiscard]] static std::string scriptSkeleton()
    {
        return std::string{DrumTrainerScriptEngine::kSkeletonScript};
    }

    [[nodiscard]] const DrumTrainerScriptEngine::UiParamSlots& uiParamSlots() const noexcept
    {
        return m_scriptEngine.uiParamSlots();
    }

    // setPreset() may run on the audio thread (host automation) where loading Lua is not
    // allowed, so it only raises a flag; the message thread calls this to do the reload.
    void reloadScriptIfPending()
    {
        if (m_scriptReloadPending.exchange(false, std::memory_order_acq_rel) && !m_scriptSource.empty())
        {
            static_cast<void>(applyScript());
        }
    }

    // Message thread only, like applyScript() which writes it.
    [[nodiscard]] const std::string& scriptName() const noexcept
    {
        return m_scriptName;
    }

    // The script's own name, else the Preset's ("Script" when a script sets the bar length).
    [[nodiscard]] std::string displayName() const
    {
        if (!m_scriptName.empty())
        {
            return m_scriptName;
        }
        return std::string{scriptSetsBeatsPerBar() ? "Script" : kPresets[presetSlot()].name};
    }

    void setPreset(const int index)
    {
        m_presetIndex = std::clamp(index, 0, static_cast<int>(kPresets.size()) - 1);
        const auto& preset = kPresets[presetSlot()];
        if (!scriptSetsBeatsPerBar())
        {
            m_seq.setBeatsPerBar(preset.barBeats);
        }
        m_seq.setSubdivType(preset.subdivType);
        m_seq.resetBarPosition();
        m_barCount = 0;
        m_beatOccurrenceInBar = 0;
        m_scriptReloadPending.store(true, std::memory_order_release);
    }

    void setVoicing(const int index)
    {
        m_voicingIndex = std::clamp(index, 0, static_cast<int>(kVoicings.size()) - 1);
    }

    // Same code vocabulary across all three kits (see samples/README.md), so
    // switching just reloads DrumVoiceKit from the new directory.
    void setDrumKit(const int index)
    {
        m_drumKitIndex = std::clamp(index, 0, static_cast<int>(kDrumKitDirNames.size()) - 1);
        const std::string path = std::string(kAbacDspDrumKitsRootDir) + "/" +
                                 std::string(kDrumKitDirNames[static_cast<size_t>(m_drumKitIndex)]);
        m_drumVoiceKit.requestLoad(path);
    }

    void setSwingRatio(const float ratio)
    {
        m_seq.setSwingRatio(std::clamp(ratio, 1.f, 2.f));
    }

    [[nodiscard]] bool presetHasSwing() const noexcept
    {
        return kPresets[static_cast<size_t>(m_presetIndex)].hasSwing;
    }

    [[nodiscard]] static bool isPresetSwing(const int index) noexcept
    {
        if (index < 0 || index >= static_cast<int>(kPresets.size()))
        {
            return false;
        }
        return kPresets[static_cast<size_t>(index)].hasSwing;
    }

    // Message thread. A map that fails to parse leaves the current one active; returns the error.
    [[nodiscard]] std::optional<std::string> setDrumMapJson(const std::string_view json)
    {
        auto load = DrumInput::parseDrumMap(json);
        if (!load.file)
        {
            return std::move(load.error);
        }
        installDrumMap(*load.file);
        return std::nullopt;
    }

    void useDefaultDrumMap()
    {
        installDrumMap(DrumInput::defaultDrumMapFile());
    }

    // Message thread only, like setDrumMapJson() which writes it.
    [[nodiscard]] const std::string& drumMapName() const noexcept
    {
        return m_drumMapFile.name;
    }

    // Message thread: the last note-on or CC received (before channel filtering) and the last
    // drum hit decoded, for checking a drum map against the module.
    [[nodiscard]] std::string midiMonitorText() const
    {
        const uint32_t event = m_lastMidiEvent.load(std::memory_order_relaxed);
        if ((event & kMonitorValidBit) == 0)
        {
            return std::format("Drum map: {} | waiting for MIDI input", m_drumMapFile.name);
        }
        std::string text = std::format("Drum map: {} | MIDI in: {}", m_drumMapFile.name, describeMidiEvent(event));
        const uint32_t hit = m_lastDrumHit.load(std::memory_order_relaxed);
        if ((hit & kMonitorValidBit) != 0)
        {
            text += " | Last hit: " + describeDrumHit(hit);
        }
        return text;
    }

    void processMidiEvent(const std::span<const uint8_t> bytes, const int sampleOffset) noexcept
    {
        adoptPendingDrumMap();
        recordMonitorEvent(bytes);
        m_midiDecoder.process(bytes, m_sampleClock + sampleOffset, [this](const DrumInput::DrumHit& hit) noexcept
                              { onDrumHit(hit); });
    }

    template <typename Emit>
    void drainMidiOutput([[maybe_unused]] Emit&& emit) noexcept
    {
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        if (m_hostSync)
        {
            syncToHostTransport();
        }
        updateAnalysisMode();
        m_drumVoiceKit.pollAndInstall();
        adoptPendingDrumMap();
        m_midiDecoder.advance(m_sampleClock, [this](const DrumInput::DrumHit& hit) noexcept { onDrumHit(hit); });
        if (!effectiveRunning())
        {
            adoptPendingPattern();
        }

        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto event = m_seq.advance();
            const auto& mode = kDropBarModes[static_cast<size_t>(m_dropModeIndex)];
            const bool countingIn = isAnalysisCountingIn();
            const bool isMuted = !countingIn && mode.playBars > 0 && m_barCount >= mode.playBars;

            if (event.beatStart && !isMuted)
            {
                triggerBeatAccent(beatAccent(event.beatIndexInBar));
            }
            if (event.subdivision && !isMuted)
            {
                triggerSubdivision();
            }

            const float click = m_click.step0();
            float drumLeft = 0.f;
            float drumRight = 0.f;
            for (auto& slot : m_drumVoices)
            {
                float voiceLeft = 0.f;
                float voiceRight = 0.f;
                slot.player.processBlock(&voiceLeft, &voiceRight, 1);
                drumLeft += voiceLeft * slot.gain;
                drumRight += voiceRight * slot.gain;
            }

            const bool audible = (effectiveRunning() || countingIn) && !isMuted;
            const bool useClick = isVoicingClick();
            const float leftOutput = audible ? (useClick ? click : drumLeft) : 0.f;
            const float rightOutput = audible ? (useClick ? click : drumRight) : 0.f;

            out(i, 0) = in(i, 0) * m_inputGain + leftOutput;
            out(i, 1) = in(i, 1) * m_inputGain + rightOutput;

            if (event.barWrapped)
            {
                advanceDropBar(mode);
                m_beatOccurrenceInBar = 0;
                advancePatternBar();
                adoptPendingPattern();
                advanceAnalysisBar();
            }
        }
        m_sampleClock += static_cast<int64_t>(BlockSize);
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return static_cast<int>(effectiveBeatsPerBar());
    }

    [[nodiscard]] size_t patternBars() const noexcept
    {
        return m_patternBars.load(std::memory_order_relaxed);
    }

    // The bar of the target pattern being played, 0-based; restarts with a new pattern and
    // when an analysis take starts capturing.
    [[nodiscard]] size_t patternBarIndex() const noexcept
    {
        return m_patternBar.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool scriptSetsBeatsPerBar() const noexcept
    {
        return m_scriptBeatsPerBar.load(std::memory_order_relaxed) != 0;
    }

    [[nodiscard]] float getBarPhase() const noexcept
    {
        return m_seq.barPhase();
    }

  private:
    // Drop-bar mode table: {playBars, dropBars}. playBars=0 means disabled.
    struct DropBarMode
    {
        int playBars;
        int dropBars;
    };
    // clang-format off
    static constexpr auto kDropBarModes = std::to_array<DropBarMode>({
        {0, 0}, // Drop none
        {1, 1}, // Play 1 Drop 1
        {3, 1}, // Play 3 Drop 1
        {2, 2}, // Play 2 Drop 2
        {1, 3}, // Play 1 Drop 3
    });
    // clang-format on

    // Short aliases keep the preset table readable
    // clang-format off
    static constexpr ClickAccent D = ClickAccent::Downbeat;
    static constexpr ClickAccent B = ClickAccent::Beat;
    static constexpr ClickAccent S = ClickAccent::Sub;
    static constexpr ClickAccent N = ClickAccent::None;

    static constexpr SubdivType kEi = SubdivType::Eighth;
    static constexpr SubdivType kSi = SubdivType::Sixteenth;
    static constexpr SubdivType kTr = SubdivType::Triplet;
    static constexpr SubdivType kSh = SubdivType::Shuffle;
    static constexpr SubdivType kC3 = SubdivType::Compound3;
    static constexpr SubdivType kNo = SubdivType::None;

    // clang-format off
    static constexpr auto kPresets = std::to_array<RhythmPreset>({
        //  name                      beats  pattern (padded to 16)                              subdiv  swing
        // Simple meters: vanilla (quarter beats only) then subdivided variants
        {"3/4",                   3, {D,B,B},                                                   kNo, false},
        {"3/4 8th",               3, {D,B,B},                                                   kEi, false},
        {"3/4 16th",              3, {D,B,B},                                                   kSi, false},
        {"3/4 shuffle",           3, {D,B,B},                                                   kSh, true},
        {"3/4 triplet",           3, {D,B,B},                                                   kTr, false},
        {"4/4",                   4, {D,B,B,B},                                                 kNo, false},
        {"4/4 8th",               4, {D,B,B,B},                                                 kEi, false},
        {"4/4 16th",              4, {D,B,B,B},                                                 kSi, false},
        {"4/4 shuffle",           4, {D,B,B,B},                                                 kSh, true},
        {"4/4 triplet",           4, {D,B,B,B},                                                 kTr, false},
        {"4/4 swing",             4, {D,B,B,B},                                                 kSh, true},
        // 5/4: grouping feel in name; vanilla then 8th subdivision
        {"5/4 (3+2)",             5, {D,B,B,B,B},                                               kNo, false},
        {"5/4 8th (3+2)",         5, {D,B,B,B,B},                                               kEi, false},
        {"5/4 (2+3)",             5, {D,B,B,B,B},                                               kNo, false},
        {"5/4 8th (2+3)",         5, {D,B,B,B,B},                                               kEi, false},
        // Compound meters (felt beat = dotted quarter, subdivides into 3 eighth notes)
        {"6/8 in-2",              2, {D,B},                                                      kC3, false},
        // 6/8 in-6: felt beat = eighth note; accents every 3 eighths
        {"6/8 in-6",              6, {D,S,S,B,S,S},                                             kNo, false},
        // Odd meters: felt beat = eighth note; S marks within-group subdivisions
        {"7/8 (2+2+3)",           7, {D,S,B,S,B,S,S},                                          kNo, false},
        {"7/8 (2+3+2)",           7, {D,S,B,S,S,B,S},                                          kNo, false},
        {"7/8 (3+2+2)",           7, {D,S,S,B,S,B,S},                                          kNo, false},
        {"9/8 in-3",              3, {D,B,B},                                                   kC3, false},
        {"9/8 in-9",              9, {D,S,S,B,S,S,B,S,S},                                      kNo, false},
        {"11/8 (3+3+3+2)",       11, {D,S,S,B,S,S,B,S,S,B,S},                                  kNo, false},
        {"11/8 (3+3+2+3)",       11, {D,S,S,B,S,S,B,S,B,S,S},                                  kNo, false},
        {"13/8 (3+3+3+2+2)",     13, {D,S,S,B,S,S,B,S,S,B,S,B,S},                              kNo, false},
        {"13/8 (3+4+3+3)",       13, {D,S,S,B,S,S,S,B,S,S,B,S,S},                              kNo, false},
    });
    // clang-format on

    // Bars captured per take, in blueprint dropdown order; 0 is open end.
    static constexpr auto kAnalysisBarCounts = std::to_array<size_t>({0, 4, 8, 12, 16, 24, 32, 64});
    static constexpr size_t kAnalysisCountInBars = 2;

    enum class AnalysisPhase : uint8_t
    {
        Idle,
        Waiting,
        CountIn,
        Capturing
    };

    // samples/drums subfolder names, in blueprint dropdown order.
    static constexpr auto kDrumKitDirNames = std::to_array<std::string_view>({"808", "reggae", "pocket"});

    // One drum-kit voicing: which sample code plays on the alternating strong-beat
    // slots (A = downbeat + even beat count, B = odd beat count - see
    // m_beatOccurrenceInBar) and on the subdivision grid. An empty code means
    // silence for that slot; index 0 ("Click") is the original damped-sine click
    // and is never resolved through the kit at all (see isVoicingClick()).
    struct VoicingDef
    {
        std::string_view name;
        std::string_view codeA;
        std::string_view codeB;
        std::string_view codeSub;
    };
    static constexpr int kClickVoicingIndex = 0;
    // clang-format off
    static constexpr auto kVoicings = std::to_array<VoicingDef>({
        //  name                beat A     beat B     subdivision
        {"Click",               "",        "",        ""},
        {"Kick",                "bd",      "bd",      ""},
        {"Kick + HH",           "bd",      "bd",      "hh"},
        {"HH only",             "hh",      "hh",      "hhghost"},
        {"Kick Snare HH",       "bd",      "sd",      "hh"},
        {"Timbal",              "timb1",   "timb2",   "timbdmp"},
        {"Tom",                 "tomlo",   "tom1",    "tom2"},
        {"Wood",                "wood",    "wood",    "wood"},
        {"Sticks",              "sstick",  "sstick",  "sstick"},
        {"Shaker offbeat",      "",        "",        "shaker"},
    });
    // clang-format on

    static constexpr float kDownbeatAccentGain = 1.0f;
    static constexpr float kBeatAccentGain = 0.85f;
    static constexpr float kSubAccentGain = 0.55f;

    // Unlike ClickGenerator::dbToGain(), no kBoostDb: the click is a tiny damped
    // sine that needs a large boost to be audible at all, while the drum-kit
    // samples are already normalized to their own sensible peak levels.
    [[nodiscard]] static float dbToLinearGain(const float valueDb) noexcept
    {
        return std::pow(10.f, valueDb / 20.f);
    }

    [[nodiscard]] static constexpr float accentGain(const ClickAccent level) noexcept
    {
        switch (level)
        {
            case ClickAccent::Downbeat:
                return kDownbeatAccentGain;
            case ClickAccent::Beat:
                return kBeatAccentGain;
            case ClickAccent::Sub:
                return kSubAccentGain;
            case ClickAccent::None:
                return 0.f;
        }
        return 0.f;
    }

    [[nodiscard]] bool isVoicingClick() const noexcept
    {
        return m_voicingIndex == kClickVoicingIndex;
    }

    // One drum-voicing sample slot: a SamplePlayerBasic plus the trigger-time
    // gain (from accentGain()) and allocation order for allocateDrumVoice()'s
    // steal-oldest policy.
    struct DrumVoiceSlot
    {
        explicit DrumVoiceSlot(const float sampleRate)
            : player(sampleRate)
        {
        }
        AbacDsp::SamplePlayerBasic player;
        float gain{1.f};
        uint64_t startOrder{0};
    };
    static constexpr size_t kNumDrumVoices = 16;
    using DrumVoicePool = std::array<DrumVoiceSlot, kNumDrumVoices>;

    template <size_t... I>
    [[nodiscard]] static DrumVoicePool makeDrumVoicePoolImpl(const float sampleRate, std::index_sequence<I...>)
    {
        return DrumVoicePool{{(static_cast<void>(I), DrumVoiceSlot(sampleRate))...}};
    }

    [[nodiscard]] static DrumVoicePool makeDrumVoicePool(const float sampleRate)
    {
        return makeDrumVoicePoolImpl(sampleRate, std::make_index_sequence<kNumDrumVoices>{});
    }

    // Prefers an idle voice; if every voice is still playing, steals the one
    // triggered longest ago (mirrors SlicePlayer::allocateVoice()).
    [[nodiscard]] DrumVoiceSlot& allocateDrumVoice() noexcept
    {
        for (auto& slot : m_drumVoices)
        {
            if (slot.player.isDone())
            {
                return slot;
            }
        }
        auto* oldest = &m_drumVoices.front();
        for (auto& slot : m_drumVoices)
        {
            if (slot.startOrder < oldest->startOrder)
            {
                oldest = &slot;
            }
        }
        return *oldest;
    }

    void triggerDrumVoice(const std::string_view code, const float gain) noexcept
    {
        if (code.empty())
        {
            return;
        }
        const auto sample = m_drumVoiceKit.nextTake(code);
        if (!sample)
        {
            return;
        }
        auto& slot = allocateDrumVoice();
        slot.player.runStereo(sample);
        slot.player.setLoop(false);
        slot.player.restart();
        slot.gain = gain;
        slot.startOrder = ++m_drumVoiceTriggerCounter;
    }

    void triggerBeatAccent(const ClickAccent level) noexcept
    {
        if (isVoicingClick())
        {
            m_click.trigger(level);
            return;
        }
        const auto& voicing = kVoicings[static_cast<size_t>(m_voicingIndex)];
        const std::string_view code = (m_beatOccurrenceInBar % 2 == 0) ? voicing.codeA : voicing.codeB;
        ++m_beatOccurrenceInBar;
        triggerDrumVoice(code, accentGain(level) * m_metroVolumeGain);
    }

    void triggerSubdivision() noexcept
    {
        if (isVoicingClick())
        {
            m_click.triggerSub();
            return;
        }
        triggerDrumVoice(kVoicings[static_cast<size_t>(m_voicingIndex)].codeSub, kSubAccentGain * m_subVolumeGain);
    }

    [[nodiscard]] size_t presetSlot() const noexcept
    {
        return static_cast<size_t>(m_presetIndex);
    }

    [[nodiscard]] size_t effectiveBeatsPerBar() const noexcept
    {
        const size_t scripted = m_scriptBeatsPerBar.load(std::memory_order_relaxed);
        return scripted != 0 ? scripted : kPresets[presetSlot()].barBeats;
    }

    // A script bar length can exceed the Preset's accent table, so it gets plain accents.
    [[nodiscard]] ClickAccent beatAccent(const size_t beatIndexInBar) const noexcept
    {
        if (m_pattern != nullptr && m_pattern->beatsPerBar != 0)
        {
            return beatIndexInBar == 0 ? ClickAccent::Downbeat : ClickAccent::Beat;
        }
        return kPresets[presetSlot()].pattern[beatIndexInBar];
    }

    void advancePatternBar() noexcept
    {
        const size_t next = m_patternBar.load(std::memory_order_relaxed) + 1;
        m_patternBar.store(next < patternBars() ? next : 0, std::memory_order_relaxed);
    }

    void adoptPendingPattern() noexcept
    {
        const auto* pending = m_patternMailbox.acquireNewest();
        if (pending == nullptr)
        {
            return;
        }
        m_pattern = pending;
        m_patternBars.store(pending->active ? pending->patternBars : 1, std::memory_order_relaxed);
        m_patternBar.store(0, std::memory_order_relaxed);
        const size_t beatsPerBar = pending->beatsPerBar != 0 ? pending->beatsPerBar : kPresets[presetSlot()].barBeats;
        if (beatsPerBar != m_seq.beatsPerBar())
        {
            m_seq.setBeatsPerBar(beatsPerBar);
            m_seq.resetBarPosition();
        }
    }

    bool applyScript()
    {
        const size_t presetBeats = kPresets[presetSlot()].barBeats;
        if (!m_scriptEngine.loadPattern(m_scriptSource, presetBeats))
        {
            return false;
        }
        const auto& result = m_scriptEngine.result();
        m_patternMailbox.backSlot() = result.pattern;
        m_patternMailbox.publish();
        m_scriptBeatsPerBar.store(result.pattern.beatsPerBar, std::memory_order_relaxed);
        m_scriptName = result.name;
        return true;
    }

    void advanceDropBar(const DropBarMode& mode) noexcept
    {
        if (mode.playBars > 0 && ++m_barCount >= mode.playBars + mode.dropBars)
        {
            m_barCount = 0;
        }
    }

    void applyBpm(const float value)
    {
        m_bpm = value;
        m_seq.setBpm(value);
    }

    [[nodiscard]] bool effectiveRunning() const noexcept
    {
        return m_hostSync ? hostTransport().isPlaying : m_running;
    }

    // Bar length uses the effective barBeats (via the sequencer), not the host time
    // signature. Only resync on a fresh transport sample; advance() carries phase between.
    void syncToHostTransport()
    {
        const auto& transport = hostTransport();
        if (!transport.isPlaying)
        {
            return;
        }
        if (transport.updateCount == m_lastSyncedUpdateCount)
        {
            return;
        }
        m_lastSyncedUpdateCount = transport.updateCount;
        applyBpm(std::clamp(static_cast<float>(transport.bpm), 20.f, 999.f));
        m_seq.syncToPpq(transport.ppqPosition);
    }

    [[nodiscard]] bool isAnalysisCountingIn() const noexcept
    {
        return m_analysisPhase == AnalysisPhase::Waiting || m_analysisPhase == AnalysisPhase::CountIn;
    }

    void setAnalysisPhase(const AnalysisPhase phase) noexcept
    {
        m_analysisPhase = phase;
        publishAnalysisProgress();
    }

    void publishAnalysisProgress() noexcept
    {
        const uint64_t packed =
            static_cast<uint64_t>(m_analysisPhase) | (uint64_t{m_analysisBar} << 8) | (uint64_t{m_takeBars} << 24);
        m_analysisProgress.store(packed, std::memory_order_relaxed);
    }

    // Free-running takes restart the bar so the count-in begins on a downbeat; under host sync the
    // bar position belongs to the host, so the count-in waits for the next bar wrap instead.
    void beginTake() noexcept
    {
        m_takeBars = static_cast<uint32_t>(kAnalysisBarCounts[m_analysisBarsIndex.load(std::memory_order_relaxed)]);
        m_analysisBar = 0;
        m_capturedHits.reset();
        if (m_hostSync)
        {
            setAnalysisPhase(AnalysisPhase::Waiting);
            return;
        }
        m_seq.reset();
        m_beatOccurrenceInBar = 0;
        setAnalysisPhase(AnalysisPhase::CountIn);
    }

    // A take stopped during the count-in has captured nothing, so it gets no report.
    void endTake() noexcept
    {
        if (m_analysisPhase == AnalysisPhase::Capturing)
        {
            m_takeSetup.capturedBars = m_analysisBar;
            m_reportReady.store(true, std::memory_order_release);
        }
        setAnalysisPhase(AnalysisPhase::Idle);
    }

    void beginCapture() noexcept
    {
        m_takeSetup.pattern = m_pattern != nullptr ? *m_pattern : DrumPattern::HitPattern{};
        m_takeSetup.beatsPerBar = m_seq.beatsPerBar();
        m_takeSetup.bpm = m_bpm;
        m_takeSetup.capturedBars = 0;
        m_barCount = 0;
        m_patternBar.store(0, std::memory_order_relaxed);
        m_analysisBar = 0;
        setAnalysisPhase(AnalysisPhase::Capturing);
    }

    void finishTake() noexcept
    {
        m_analysisMode.store(false, std::memory_order_relaxed);
        m_prevAnalysisMode = false;
        endTake();
        m_analysisAutoStopped.store(true, std::memory_order_release);
    }

    void advanceAnalysisBar() noexcept
    {
        switch (m_analysisPhase)
        {
            case AnalysisPhase::Idle:
                return;
            case AnalysisPhase::Waiting:
                m_analysisBar = 0;
                setAnalysisPhase(AnalysisPhase::CountIn);
                return;
            case AnalysisPhase::CountIn:
                if (++m_analysisBar >= kAnalysisCountInBars)
                {
                    beginCapture();
                    return;
                }
                break;
            case AnalysisPhase::Capturing:
                if (++m_analysisBar >= m_takeBars && m_takeBars > 0)
                {
                    finishTake();
                    return;
                }
                break;
        }
        publishAnalysisProgress();
    }

    // Detects the analysis on/off edge made by the switch.
    void updateAnalysisMode() noexcept
    {
        const bool requested = m_analysisMode.load(std::memory_order_relaxed);
        if (requested == m_prevAnalysisMode)
        {
            return;
        }
        m_prevAnalysisMode = requested;
        if (requested)
        {
            beginTake();
        }
        else
        {
            endTake();
        }
    }

    void installDrumMap(const DrumInput::DrumMapFile& file)
    {
        m_drumMapFile = file;
        m_drumMapMailbox.backSlot() = file.map;
        m_drumMapMailbox.publish();
    }

    void adoptPendingDrumMap() noexcept
    {
        if (const auto* map = m_drumMapMailbox.acquireNewest())
        {
            m_midiDecoder.setMap(*map, sampleRate());
        }
    }

    static constexpr uint32_t kMonitorValidBit{1u << 24};

    void recordMonitorEvent(const std::span<const uint8_t> bytes) noexcept
    {
        if (bytes.size() < 3)
        {
            return;
        }
        const auto type = static_cast<uint8_t>(bytes[0] & 0xF0);
        if ((type == 0x90 && bytes[2] > 0) || type == 0xB0)
        {
            m_lastMidiEvent.store(bytes[0] | (uint32_t{bytes[1]} << 8) | (uint32_t{bytes[2]} << 16) | kMonitorValidBit,
                                  std::memory_order_relaxed);
        }
    }

    // Hits of the last count-in bar are kept (with negative beats) so that an early first hit
    // of the take still counts.
    [[nodiscard]] std::optional<float> takeBeatsAt(const int64_t time) const noexcept
    {
        const bool lastCountInBar =
            m_analysisPhase == AnalysisPhase::CountIn && m_analysisBar + 1 == kAnalysisCountInBars;
        if (m_analysisPhase != AnalysisPhase::Capturing && !lastCountInBar)
        {
            return std::nullopt;
        }
        const auto samplesPerBeat = static_cast<double>(std::max<size_t>(m_seq.samplesPerBeat(), 1));
        const double barsSinceCapture = lastCountInBar ? -1.0 : static_cast<double>(m_analysisBar);
        const double beatInBar = static_cast<double>(m_seq.beatIndexInBar()) +
                                 static_cast<double>(m_seq.beatSamplePos()) / samplesPerBeat;
        const double offsetBeats = static_cast<double>(time - m_latencySamples - m_sampleClock) / samplesPerBeat;
        return static_cast<float>(barsSinceCapture * static_cast<double>(m_seq.beatsPerBar()) + beatInBar +
                                  offsetBeats);
    }

    void onDrumHit(const DrumInput::DrumHit& hit) noexcept
    {
        if (const auto takeBeats = takeBeatsAt(hit.time))
        {
            m_capturedHits.push({*takeBeats, hit.voice, hit.velocity, hit.pedal});
        }
        m_lastDrumHit.store(static_cast<uint32_t>(hit.voice) | (uint32_t{hit.velocity} << 8) |
                                (uint32_t{hit.pedal} << 16) | kMonitorValidBit,
                            std::memory_order_relaxed);
    }

    [[nodiscard]] std::string describeMidiEvent(const uint32_t event) const
    {
        const auto status = static_cast<uint8_t>(event & 0xFF);
        const auto data1 = static_cast<uint8_t>((event >> 8) & 0xFF);
        const auto data2 = static_cast<uint8_t>((event >> 16) & 0xFF);
        const auto channel = static_cast<uint8_t>((status & 0x0F) + 1);
        const auto& map = m_drumMapFile.map;
        const bool accepted = map.acceptsChannel(channel);
        if ((status & 0xF0) == 0xB0)
        {
            const bool isPedal = accepted && data1 == map.hihat.pedalCc;
            return std::format("CC {} = {} ch {}{}", data1, data2, channel,
                               !accepted ? " (channel ignored)" : (isPedal ? " (hihat pedal)" : ""));
        }
        std::string text = std::format("note {} ({}) vel {} ch {}", data1, DrumInput::noteName(data1, map.middleCOctave),
                                       data2, channel);
        if (!accepted)
        {
            return text + " (channel ignored)";
        }
        const auto pad = map.padForNote(data1);
        return text + (pad ? std::format(" = {}", DrumInput::kPads[static_cast<size_t>(*pad)].label) : " (not mapped)");
    }

    [[nodiscard]] static std::string describeDrumHit(const uint32_t packed)
    {
        const auto voice = static_cast<DrumInput::PlayedVoice>(packed & 0xFF);
        const auto velocity = (packed >> 8) & 0xFF;
        const auto pedal = (packed >> 16) & 0xFF;
        if (DrumInput::isHihat(voice))
        {
            return std::format("{} vel {}, pedal {}", DrumInput::label(voice), velocity, pedal);
        }
        return std::format("{} vel {}", DrumInput::label(voice), velocity);
    }

    static constexpr float kDefaultSwingRatio = 1.5f;

    float m_bpm{120.f};
    float m_inputGain{1.f};
    bool m_running{false};
    bool m_hostSync{false};
    std::atomic<bool> m_analysisMode{false};
    bool m_prevAnalysisMode{false};
    std::atomic<size_t> m_analysisBarsIndex{0};
    std::atomic<bool> m_analysisAutoStopped{false};
    std::atomic<uint64_t> m_analysisProgress{0};
    AnalysisPhase m_analysisPhase{AnalysisPhase::Idle};
    uint32_t m_analysisBar{0};
    uint32_t m_takeBars{0};
    uint64_t m_lastSyncedUpdateCount{0};
    int m_dropModeIndex{0};
    int m_barCount{0};

    int m_presetIndex{kDefaultPresetIndex};

    int m_voicingIndex{kClickVoicingIndex};
    int m_drumKitIndex{0};
    size_t m_beatOccurrenceInBar{0};
    uint64_t m_drumVoiceTriggerCounter{0};
    // Mirrors ClickGenerator's own defaults (see setMetroVolume()/setSubVolume()) so a
    // freshly constructed instance matches the blueprint's default dial values before
    // the host attaches and applies its own parameter state.
    float m_metroVolumeGain{dbToLinearGain(-6.f)};
    float m_subVolumeGain{dbToLinearGain(-15.f)};

    AbacDsp::ClickGenerator m_click;
    AbacDsp::BeatSequencer m_seq;

    AbacDsp::DrumVoiceKit m_drumVoiceKit;
    DrumVoicePool m_drumVoices;

    DrumInput::DrumMapFile m_drumMapFile{DrumInput::defaultDrumMapFile()};
    DrumPattern::TripleBuffer<DrumInput::DrumMap> m_drumMapMailbox;
    DrumInput::DrumMidiDecoder m_midiDecoder;
    int64_t m_sampleClock{0};
    std::atomic<uint32_t> m_lastMidiEvent{0};
    std::atomic<uint32_t> m_lastDrumHit{0};
    int64_t m_latencySamples{0};
    DrumAnalysis::CapturedHitCollector m_capturedHits;
    DrumAnalysis::TakeSetup m_takeSetup;
    std::atomic<bool> m_reportReady{false};

    DrumTrainerScriptEngine m_scriptEngine;
    std::string m_scriptSource;
    std::string m_scriptName;
    std::atomic<bool> m_scriptReloadPending{false};
    std::atomic<size_t> m_scriptBeatsPerBar{0};
    DrumPattern::TripleBuffer<DrumPattern::HitPattern> m_patternMailbox;
    const DrumPattern::HitPattern* m_pattern{nullptr};
    std::atomic<size_t> m_patternBars{1};
    std::atomic<size_t> m_patternBar{0};
};
