#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/SvfResoBP.h"

enum class AccentLevel : uint8_t
{
    None,
    Subdiv,
    Beat,
    Downbeat
};
enum class SubdivType : uint8_t
{
    None,
    Eighth,
    Sixteenth,
    Triplet,
    Shuffle,
    Compound3
};

struct RhythmPreset
{
    std::string_view name;
    uint8_t barBeats;
    std::array<AccentLevel, 16> pattern; // only first barBeats entries are used
    SubdivType subdivType;
    bool hasSwing;
};

template <size_t BlockSize>
class MetronomeImpl final : public EffectBase
{
  public:
    static constexpr float tickFrequencyHz = 800.f;
    static constexpr float beatOneFrequencyHz = 400.f;
    static constexpr float subFrequencyHz = 1600.f;
    static constexpr float tickDecaySeconds = 0.04f;
    static constexpr float tickBoostDb = 24.f;
    static constexpr float subDefaultOffsetDb = -9.f;

    // Display window — shows one full beat: 1/4 before beat, 3/4 after
    static constexpr size_t kVisualBufferSize = 200000; // ~4s at 48kHz, covers 40 BPM
    static constexpr float kBeatPositionRatio = 0.25f;

    static constexpr int kDefaultPresetIndex = 6; // 4/4 8th

    explicit MetronomeImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_tickFilter(sampleRate)
        , m_beatOneFilter(sampleRate)
        , m_subFilter(sampleRate)
    {
        m_tickFilter.setByDecay(0, tickFrequencyHz, tickDecaySeconds);
        m_beatOneFilter.setByDecay(0, beatOneFrequencyHz, tickDecaySeconds);
        m_subFilter.setByDecay(0, subFrequencyHz, tickDecaySeconds);

        m_samplesPerBeat = beatsToSamples(m_bpm);
        m_visualWavedata.resize(kVisualBufferSize, 0.f);
        m_inputSpectrogram.setSampleRate(sampleRate);
        updateWindowSizes();
        updateSubPositions();
    }

    void setBpm(const float value)
    {
        m_bpm = std::clamp(value, 40.f, 250.f);
        m_samplesPerBeat = beatsToSamples(m_bpm);
        updateWindowSizes();
        updateSubPositions();
    }

    void setDropBars(const size_t value)
    {
        m_dropModeIndex = static_cast<int>(std::min(value, kDropBarModes.size() - 1));
        m_barCount = 0;
    }

    void setMetroVolume(const float valueDb) noexcept
    {
        m_metroGain = std::pow(10.f, (valueDb + tickBoostDb) / 20.f);
    }

    void setSubVolume(const float valueDb) noexcept
    {
        m_subGain = std::pow(10.f, (valueDb + tickBoostDb) / 20.f);
    }

    void setInputVolume(const float valueDb) noexcept
    {
        m_inputGain = std::pow(10.f, valueDb / 20.f);
    }

    void setOnOff(const bool value) noexcept
    {
        m_running = value;
    }

    void setPreset(const int index)
    {
        m_presetIndex = std::clamp(index, 0, static_cast<int>(kPresets.size()) - 1);
        m_barBeatCount = 0;
        m_barCount = 0;
        updateSubPositions();
    }

    void setSwingRatio(const float ratio)
    {
        m_swingRatio = std::clamp(ratio, 1.f, 2.f);
        updateSubPositions();
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
        return m_subPositions;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<float, BlockSize> inMono{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            inMono[i] = in(i, 0);
        }
        m_inputSpectrogram.processBlock(std::span<const float>{inMono});

        const size_t preWindow = m_preWindow;
        const size_t postWindow = m_postWindow;

        auto writeToVisualWindow = [&](const float visSignal) noexcept
        {
            if (m_beatSamplePos < postWindow)
            {
                m_visualWavedata[preWindow + m_beatSamplePos] = visSignal;
            }
            else if (m_beatSamplePos >= m_samplesPerBeat - preWindow)
            {
                m_visualWavedata[m_beatSamplePos - (m_samplesPerBeat - preWindow)] = visSignal;
            }
        };

        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto& mode = kDropBarModes[static_cast<size_t>(m_dropModeIndex)];
            const bool isMuted = mode.playBars > 0 && m_barCount >= mode.playBars;

            if (m_beatSamplePos == 0 && !isMuted)
            {
                triggerBeatAccent(kPresets[static_cast<size_t>(m_presetIndex)].pattern[m_barBeatCount]);
            }

            if (!isMuted)
            {
                triggerSubdivisions();
            }

            const float tick = m_tickFilter.step0();
            const float beatOne = m_beatOneFilter.step0();
            const float sub = m_subFilter.step0();
            const float output = m_running && !isMuted ? (tick + beatOne + sub) : 0.f;

            out(i, 0) = in(i, 0) * m_inputGain + output;
            out(i, 1) = in(i, 1) * m_inputGain + output;

            writeToVisualWindow(in(i, 0) + in(i, 1));

            if (++m_beatSamplePos >= m_samplesPerBeat)
            {
                m_beatSamplePos = 0;
                advanceBeatAndBar(mode);
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
        if (m_samplesPerBeat == 0)
        {
            return 0.f;
        }
        const int barBeats = getBarBeats();
        if (barBeats == 0)
        {
            return 0.f;
        }
        const float beatPhase = static_cast<float>(m_beatSamplePos) / static_cast<float>(m_samplesPerBeat);
        return (static_cast<float>(m_barBeatCount) + beatPhase) / static_cast<float>(barBeats);
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
    static constexpr AccentLevel D = AccentLevel::Downbeat;
    static constexpr AccentLevel B = AccentLevel::Beat;
    static constexpr AccentLevel S = AccentLevel::Subdiv;
    static constexpr AccentLevel N = AccentLevel::None;

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

    void triggerBeatAccent(const AccentLevel level) noexcept
    {
        switch (level)
        {
            case AccentLevel::Downbeat:
                m_beatOneFilter.reset(0.f, m_metroGain);
                break;
            case AccentLevel::Beat:
                m_tickFilter.reset(0.f, m_metroGain);
                break;
            case AccentLevel::Subdiv:
                m_subFilter.reset(0.f, m_subGain);
                break;
            case AccentLevel::None:
                break;
        }
    }

    void triggerSubdivisions() noexcept
    {
        for (const size_t subPos : m_subPositions)
        {
            if (m_beatSamplePos == subPos)
            {
                m_subFilter.reset(0.f, m_subGain);
            }
        }
    }

    void advanceBeatAndBar(const DropBarMode& mode) noexcept
    {
        if (++m_barBeatCount >= static_cast<size_t>(kPresets[static_cast<size_t>(m_presetIndex)].barBeats))
        {
            m_barBeatCount = 0;
            if (mode.playBars > 0 && ++m_barCount >= mode.playBars + mode.dropBars)
            {
                m_barCount = 0;
            }
        }
    }

    void updateWindowSizes() noexcept
    {
        m_preWindow = m_samplesPerBeat / 4;
        m_postWindow = m_samplesPerBeat - m_preWindow;
    }

    void updateSubPositions()
    {
        m_subPositions.clear();
        if (m_samplesPerBeat == 0)
        {
            return;
        }
        const size_t spb = m_samplesPerBeat;
        switch (kPresets[static_cast<size_t>(m_presetIndex)].subdivType)
        {
            case SubdivType::None:
                break;
            case SubdivType::Eighth:
                m_subPositions = {spb / 2};
                break;
            case SubdivType::Sixteenth:
                m_subPositions = {spb / 4, spb / 2, 3 * spb / 4};
                break;
            case SubdivType::Triplet:
                [[fallthrough]];
            case SubdivType::Compound3:
                m_subPositions = {spb / 3, 2 * spb / 3};
                break;
            case SubdivType::Shuffle:
            {
                const auto longPart =
                    static_cast<size_t>(static_cast<float>(spb) * m_swingRatio / (1.f + m_swingRatio));
                m_subPositions = {longPart};
                break;
            }
        }
    }

    [[nodiscard]] size_t beatsToSamples(const float bpm) const noexcept
    {
        return static_cast<size_t>(sampleRate() * 60.f / bpm);
    }

    float m_bpm{120.f};
    float m_metroGain{std::pow(10.f, (-6.f + tickBoostDb) / 20.f)};
    float m_subGain{std::pow(10.f, (-6.f + subDefaultOffsetDb + tickBoostDb) / 20.f)};
    float m_inputGain{1.f};
    bool m_running{false};
    int m_dropModeIndex{0};
    int m_barCount{0};

    int m_presetIndex{kDefaultPresetIndex};
    float m_swingRatio{1.5f};

    size_t m_samplesPerBeat{0};
    size_t m_beatSamplePos{0};
    size_t m_barBeatCount{0};
    size_t m_preWindow{0};
    size_t m_postWindow{0};

    std::vector<size_t> m_subPositions;

    AbacDsp::SvfResoBP m_tickFilter;
    AbacDsp::SvfResoBP m_beatOneFilter;
    AbacDsp::SvfResoBP m_subFilter;

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    AbacDsp::SimpleSpectrogram m_inputSpectrogram;
};
