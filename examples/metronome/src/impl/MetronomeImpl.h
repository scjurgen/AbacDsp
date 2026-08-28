#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "BeatAnalysisReport.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"

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
    {
        const auto& preset = kPresets[static_cast<size_t>(m_presetIndex)];
        m_seq.setBeatsPerBar(preset.barBeats);
        m_seq.setSubdivType(preset.subdivType);
        m_seq.setSwingRatio(kDefaultSwingRatio);
        m_seq.setBpm(m_bpm);
        m_visualWavedata.resize(kVisualBufferSize, 0.f);
        m_inputSpectrogram.setSampleRate(sampleRate);
        updateWindowSizes();
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
    }

    void setSubVolume(const float valueDb) noexcept
    {
        m_click.setSubVolumeDb(valueDb);
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
        m_analysisMode = value;
    }

    // Consumes the "a report is waiting" flag set from the audio thread when analysis mode
    // is switched off; the caller (message thread) is expected to build and export the report.
    [[nodiscard]] bool consumeAnalysisReportReady() noexcept
    {
        return m_reportReady.exchange(false, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::string buildAnalysisReportHtml() const
    {
        const auto& preset = kPresets[static_cast<size_t>(m_presetIndex)];
        return MetronomeAnalysis::buildReportHtml(m_deviationCollector.hits(), preset.barBeats,
                                                  m_seq.subPositions().size(), m_bpm, preset.name);
    }

    void setPreset(const int index)
    {
        m_presetIndex = std::clamp(index, 0, static_cast<int>(kPresets.size()) - 1);
        const auto& preset = kPresets[static_cast<size_t>(m_presetIndex)];
        m_seq.setBeatsPerBar(preset.barBeats);
        m_seq.setSubdivType(preset.subdivType);
        m_seq.resetBarPosition();
        m_barCount = 0;
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

        std::array<float, BlockSize> inMono{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            inMono[i] = in(i, 0);
        }
        m_inputSpectrogram.processBlock(std::span<const float>{inMono});

        const size_t preWindow = m_preWindow;
        const size_t postWindow = m_postWindow;
        const size_t samplesPerBeat = m_seq.samplesPerBeat();

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
            if (m_analysisMode && m_onsetDetector.step(inMono[i]))
            {
                const auto gridPoint = m_seq.nearestGridPoint();
                const float deviationMs = static_cast<float>(gridPoint.distanceSamples) / sampleRate() * 1000.f;
                const uint8_t role =
                    gridPoint.isBeat
                        ? MetronomeAnalysis::roleForBeat(gridPoint.beatIndexInBar)
                        : MetronomeAnalysis::roleForSubdivision(kPresets[static_cast<size_t>(m_presetIndex)].barBeats,
                                                                gridPoint.subdivisionIndex);
                m_deviationCollector.push(deviationMs, role);
            }

            const auto event = m_seq.advance();
            const auto& mode = kDropBarModes[static_cast<size_t>(m_dropModeIndex)];
            const bool isMuted = mode.playBars > 0 && m_barCount >= mode.playBars;

            if (event.beatStart && !isMuted)
            {
                triggerBeatAccent(kPresets[static_cast<size_t>(m_presetIndex)].pattern[event.beatIndexInBar]);
            }

            if (event.subdivision && !isMuted)
            {
                m_click.triggerSub();
            }

            const float click = m_click.step0();
            const float output = effectiveRunning() && !isMuted ? click : 0.f;

            out(i, 0) = in(i, 0) * m_inputGain + output;
            out(i, 1) = in(i, 1) * m_inputGain + output;

            writeToVisualWindow(event.beatSamplePos, in(i, 0) + in(i, 1));

            if (event.barWrapped)
            {
                advanceDropBar(mode);
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
        return static_cast<int>(kPresets[static_cast<size_t>(m_presetIndex)].barBeats);
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

    void triggerBeatAccent(const ClickAccent level) noexcept
    {
        m_click.trigger(level);
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

    // Bar length uses the preset's own barBeats (via the sequencer), not the host time
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

    // Detects the analysis on/off edge: activating resets the collector fresh for this take,
    // deactivating raises the flag the message thread polls to build and export the report.
    void updateAnalysisMode() noexcept
    {
        if (m_analysisMode == m_prevAnalysisMode)
        {
            return;
        }
        if (m_analysisMode)
        {
            m_deviationCollector.reset();
            m_onsetDetector.reset();
        }
        else
        {
            m_reportReady.store(true, std::memory_order_release);
        }
        m_prevAnalysisMode = m_analysisMode;
    }

    static constexpr float kDefaultSwingRatio = 1.5f;

    float m_bpm{120.f};
    float m_inputGain{1.f};
    bool m_running{false};
    bool m_hostSync{false};
    bool m_analysisMode{false};
    bool m_prevAnalysisMode{false};
    uint64_t m_lastSyncedUpdateCount{0};
    int m_dropModeIndex{0};
    int m_barCount{0};

    int m_presetIndex{kDefaultPresetIndex};
    size_t m_preWindow{0};
    size_t m_postWindow{0};

    AbacDsp::ClickGenerator m_click;
    AbacDsp::BeatSequencer m_seq;
    MetronomeAnalysis::OnsetDetector m_onsetDetector;
    MetronomeAnalysis::DeviationCollector m_deviationCollector;
    std::atomic<bool> m_reportReady{false};

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    AbacDsp::SimpleSpectrogram m_inputSpectrogram;
};
