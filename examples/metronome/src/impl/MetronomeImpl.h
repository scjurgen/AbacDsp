#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "BeatAnalysisReport.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
#include "MetronomeDrumPaths.h"
#include "MetronomePattern.h"
#include "MetronomeScriptEngine.h"
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
class MetronomeImpl final : public EffectBase
{
  public:
    // Display window — shows one full beat: 1/4 before beat, 3/4 after
    static constexpr size_t kVisualBufferSize = 200000; // ~4s at 48kHz, covers 40 BPM
    static constexpr float kBeatPositionRatio = 0.25f;

    static constexpr int kDefaultPresetIndex = 6; // 4/4 8th

    explicit MetronomeImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_click(sampleRate)
        , m_seq(sampleRate)
        , m_onsetDetector(sampleRate)
        , m_drumVoices(makeDrumVoicePool(sampleRate))
    {
        const auto& preset = kPresets[static_cast<size_t>(m_presetIndex)];
        m_seq.setBeatsPerBar(preset.barBeats);
        m_seq.setSubdivType(preset.subdivType);
        m_seq.setSwingRatio(kDefaultSwingRatio);
        m_seq.setBpm(m_bpm);
        m_visualWavedata.resize(kVisualBufferSize, 0.f);
        m_inputSpectrogram.setSampleRate(sampleRate);
        updateWindowSizes();
        setDrumKit(m_drumKitIndex);
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

    void setImportResolver(MetronomeScriptEngine::ImportResolver resolver)
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
        return std::string{MetronomeScriptEngine::kSkeletonScript};
    }

    [[nodiscard]] const MetronomeScriptEngine::UiParamSlots& uiParamSlots() const noexcept
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

    // Only ever read when the report is built (see buildAnalysisReportHtml()), never during
    // capture - so it may be left set to anything, or changed mid-take, without effect until then.
    void setAnalysisGrid(const int index)
    {
        m_analysisGridIndex = std::clamp(index, 0, static_cast<int>(kAnalysisGrids.size()) - 1);
    }

    // Consumes the "a report is waiting" flag set from the audio thread when analysis mode
    // is switched off; the caller (message thread) is expected to build and export the report.
    [[nodiscard]] bool consumeAnalysisReportReady() noexcept
    {
        return m_reportReady.exchange(false, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::string buildAnalysisReportHtml() const
    {
        const std::string_view name = scriptSetsBeatsPerBar() ? "Script" : kPresets[presetSlot()].name;
        return MetronomeAnalysis::buildReportHtml(m_rawOnsetCollector.hits(), effectiveBeatsPerBar(),
                                                  currentAnalysisGrid(), m_bpm, sampleRate(), name, m_scriptName);
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

    [[nodiscard]] std::string analysisFileStem() const
    {
        return MetronomeAnalysis::fileNameStem(m_scriptName);
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

    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        return m_seq.subPositions();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        if (m_hostSync)
        {
            syncToHostTransport();
        }
        updateAnalysisMode();
        m_drumVoiceKit.pollAndInstall();
        if (!effectiveRunning())
        {
            adoptPendingPattern();
        }

        std::array<float, BlockSize> inMono{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            inMono[i] = in(i, 0);
        }
        m_inputSpectrogram.processBlock(std::span<const float>{inMono});

        const size_t preWindow = m_preWindow;
        const size_t postWindow = m_postWindow;
        const size_t samplesPerBeat = m_seq.samplesPerBeat();

        const auto triggerScriptedHit = [this](const MetronomePattern::Hit& hit) noexcept
        { triggerDrumVoice(MetronomePattern::sampleCode(hit.instrument), hit.gain * m_metroVolumeGain); };

        auto writeToVisualWindow = [&](const size_t beatSamplePos, const float visSignal) noexcept
        {
            if (beatSamplePos < postWindow)
            {
                m_visualWavedata[preWindow + beatSamplePos] = visSignal;
            }
            else if (beatSamplePos >= samplesPerBeat - preWindow)
            {
                m_visualWavedata[beatSamplePos - (samplesPerBeat - preWindow)] = visSignal;
            }
        };

        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (m_analysisPhase == AnalysisPhase::Capturing && m_onsetDetector.step(inMono[i]))
            {
                m_rawOnsetCollector.push({m_seq.beatIndexInBar(), m_seq.beatSamplePos(), m_seq.samplesPerBeat()});
            }

            const auto event = m_seq.advance();
            const auto& mode = kDropBarModes[static_cast<size_t>(m_dropModeIndex)];
            const bool countingIn = isAnalysisCountingIn();
            const bool isMuted = !countingIn && mode.playBars > 0 && m_barCount >= mode.playBars;

            const bool scripted = m_patternScheduler.active();
            if (scripted && !isMuted)
            {
                m_patternScheduler.step(event.beatIndexInBar, event.beatSamplePos, samplesPerBeat, triggerScriptedHit);
            }
            if (!scripted && event.beatStart && !isMuted)
            {
                triggerBeatAccent(kPresets[static_cast<size_t>(m_presetIndex)].pattern[event.beatIndexInBar]);
            }
            if (!scripted && event.subdivision && !isMuted)
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
            const bool useClick = isVoicingClick() && !scripted;
            const float leftOutput = audible ? (useClick ? click : drumLeft) : 0.f;
            const float rightOutput = audible ? (useClick ? click : drumRight) : 0.f;

            out(i, 0) = in(i, 0) * m_inputGain + leftOutput;
            out(i, 1) = in(i, 1) * m_inputGain + rightOutput;

            writeToVisualWindow(event.beatSamplePos, in(i, 0) + in(i, 1));

            if (event.barWrapped)
            {
                advanceDropBar(mode);
                m_beatOccurrenceInBar = 0;
                adoptPendingPattern();
                advanceAnalysisBar();
            }
        }
    }

    [[nodiscard]] const std::vector<float>& visualizeWaveData()
    {
        const size_t windowSize = m_preWindow + m_postWindow;
        m_preparedWavedata.assign(m_visualWavedata.begin(),
                                  std::next(m_visualWavedata.begin(), static_cast<std::ptrdiff_t>(windowSize)));
        return m_preparedWavedata;
    }

    [[nodiscard]] size_t getBeatIndex() const noexcept
    {
        return m_preWindow;
    }

    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return m_inputSpectrogram.getImageSet();
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return static_cast<int>(effectiveBeatsPerBar());
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
        // Simple meters — vanilla (quarter beats only) then subdivided variants
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
        // 5/4 — grouping feel in name; vanilla then 8th subdivision
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

    // Grid the timing analysis measures onset hits against - independent of the preset above,
    // which only controls what the metronome itself plays.
    struct AnalysisGrid
    {
        std::string_view name;
        SubdivType type;
    };
    static constexpr auto kAnalysisGrids = std::to_array<AnalysisGrid>({
        {"Quarter", kNo},
        {"8th", kEi},
        {"Triplet", kTr},
        {"Shuffle", kSh},
        {"16th", kSi},
    });

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

    void adoptPendingPattern() noexcept
    {
        const auto* pending = m_patternMailbox.acquireNewest();
        if (pending == nullptr)
        {
            return;
        }
        m_patternScheduler.setPattern(pending);
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
        m_scriptAnalysis = result.analysis;
        m_scriptName = result.name;
        return true;
    }

    // The script's positions when it defined any, else the Analysis Grid dropdown's grid.
    [[nodiscard]] MetronomeAnalysis::AnalysisGridSpec currentAnalysisGrid() const
    {
        if (m_scriptAnalysis.active && m_scriptAnalysis.count > 0)
        {
            return MetronomeAnalysis::scriptGrid(
                std::span<const float>{m_scriptAnalysis.positions.data(), m_scriptAnalysis.count});
        }
        const auto& grid = kAnalysisGrids[static_cast<size_t>(m_analysisGridIndex)];
        return MetronomeAnalysis::builtInGrid(grid.type, grid.name, effectiveBeatsPerBar(), m_seq.swingRatio());
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
        updateWindowSizes();
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

    void updateWindowSizes() noexcept
    {
        m_preWindow = m_seq.samplesPerBeat() / 4;
        m_postWindow = m_seq.samplesPerBeat() - m_preWindow;
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
            m_reportReady.store(true, std::memory_order_release);
        }
        setAnalysisPhase(AnalysisPhase::Idle);
    }

    void beginCapture() noexcept
    {
        m_rawOnsetCollector.reset();
        m_onsetDetector.reset();
        m_barCount = 0;
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
    int m_analysisGridIndex{0};
    size_t m_preWindow{0};
    size_t m_postWindow{0};

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
    MetronomeAnalysis::OnsetDetector m_onsetDetector;
    MetronomeAnalysis::RawOnsetCollector m_rawOnsetCollector;
    std::atomic<bool> m_reportReady{false};

    AbacDsp::DrumVoiceKit m_drumVoiceKit;
    DrumVoicePool m_drumVoices;

    MetronomeScriptEngine m_scriptEngine;
    std::string m_scriptSource;
    std::string m_scriptName;
    std::atomic<bool> m_scriptReloadPending{false};
    std::atomic<size_t> m_scriptBeatsPerBar{0};
    MetronomePattern::AnalysisPositions m_scriptAnalysis;
    MetronomePattern::TripleBuffer<MetronomePattern::HitPattern> m_patternMailbox;
    MetronomePattern::PatternScheduler m_patternScheduler;

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    AbacDsp::SimpleSpectrogram m_inputSpectrogram;
};
