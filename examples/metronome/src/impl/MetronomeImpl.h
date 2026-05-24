#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Filters/SvfResoBP.h"

template <size_t BlockSize>
class MetronomeImpl final : public EffectBase
{
  public:
    // -----------------------------------------------------------------------
    // Sound parameters
    static constexpr float tickFrequencyHz = 800.f;    // main beat
    static constexpr float beatOneFrequencyHz = 400.f; // bar start (octave below)
    static constexpr float subFrequencyHz = 1600.f;    // subdivision (octave above)
    static constexpr float tickDecaySeconds = 0.04f;
    static constexpr float tickBoostDb = 24.f;
    static constexpr float subDefaultOffsetDb = -9.f;

    // Display window — shows one full beat: 1/4 before beat, 3/4 after
    static constexpr size_t kVisualBufferSize = 200000; // ~4s at 48kHz, covers 40 BPM
    static constexpr float kBeatPositionRatio = 0.25f;

    // -----------------------------------------------------------------------
    enum class Subdivision
    {
        Off,
        Eighths,
        Shuffle,
        Triplets,
        Sixteenths
    };

    static constexpr int kBeatsPerBar[] = {4, 3, 6, 5, 7, 9, 11, 13};

    // -----------------------------------------------------------------------
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
        updateWindowSizes();
        updateSubPositions();
    }

    // -----------------------------------------------------------------------
    void setBpm(const float value)
    {
        m_bpm = std::clamp(value, 40.f, 250.f);
        m_samplesPerBeat = beatsToSamples(m_bpm);
        updateWindowSizes();
        updateSubPositions();
    }

    void setMetroVolume(const float valueDb)
    {
        m_metroGain = std::pow(10.f, (valueDb + tickBoostDb) / 20.f);
    }

    void setSubVolume(const float valueDb)
    {
        m_subGain = std::pow(10.f, (valueDb + tickBoostDb) / 20.f);
    }

    void setInputVolume(const float valueDb)
    {
        m_inputGain = std::pow(10.f, valueDb / 20.f);
    }

    void setOnOff(const bool value)
    {
        m_running = value;
    }

    void setSubdivision(const int index)
    {
        m_subdivision = static_cast<Subdivision>(std::clamp(index, 0, 4));
        updateSubPositions();
    }

    // index matches kBeatsPerBar[]
    void setTimeSig(const int index)
    {
        m_beatsPerBar = static_cast<size_t>(kBeatsPerBar[std::clamp(index, 0, 7)]);
        m_barBeatCount = 0;
    }

    // -----------------------------------------------------------------------
    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        const size_t preWindow = m_preWindow;
        const size_t postWindow = m_postWindow;

        auto writeToVisualWindow = [&](const float visSignal)
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
            if (m_beatSamplePos == 0)
            {
                if (m_barBeatCount == 0)
                {
                    m_beatOneFilter.reset(0.f, m_metroGain);
                }
                else
                {
                    m_tickFilter.reset(0.f, m_metroGain);
                }
            }

            for (const size_t subPos : m_subPositions)
            {
                if (m_beatSamplePos == subPos)
                {
                    m_subFilter.reset(0.f, m_subGain);
                }
            }

            const float tick = m_tickFilter.step0();
            const float beatOne = m_beatOneFilter.step0();
            const float sub = m_subFilter.step0();
            const float output = m_running ? (tick + beatOne + sub) : 0.f;

            out(i, 0) = in(i, 0) * m_inputGain + output;
            out(i, 1) = in(i, 1) * m_inputGain + output;

            writeToVisualWindow(in(i, 0) + in(i, 1));

            if (++m_beatSamplePos >= m_samplesPerBeat)
            {
                m_beatSamplePos = 0;
                if (++m_barBeatCount >= m_beatsPerBar)
                {
                    m_barBeatCount = 0;
                }
            }
        }
    }

    const std::vector<float>& visualizeWaveData()
    {
        const size_t windowSize = m_preWindow + m_postWindow;
        m_preparedWavedata.assign(m_visualWavedata.begin(), m_visualWavedata.begin() + windowSize);
        return m_preparedWavedata;
    }

    [[nodiscard]] size_t getBeatIndex() const noexcept
    {
        return m_preWindow;
    }

  private:
    // -----------------------------------------------------------------------
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
        switch (m_subdivision)
        {
            case Subdivision::Off:
                break;
            case Subdivision::Eighths:
                m_subPositions = {spb / 2};
                break;
            case Subdivision::Shuffle:
                m_subPositions = {spb * 2 / 3};
                break;
            case Subdivision::Triplets:
                m_subPositions = {spb / 3, spb * 2 / 3};
                break;
            case Subdivision::Sixteenths:
                m_subPositions = {spb / 4, spb / 2, spb * 3 / 4};
                break;
        }
    }

    [[nodiscard]] size_t beatsToSamples(const float bpm) const noexcept
    {
        return static_cast<size_t>(sampleRate() * 60.f / bpm);
    }

    // -----------------------------------------------------------------------
    float m_bpm{120.f};
    float m_metroGain{std::pow(10.f, (-6.f + tickBoostDb) / 20.f)};
    float m_subGain{std::pow(10.f, (-6.f + subDefaultOffsetDb + tickBoostDb) / 20.f)};
    float m_inputGain{1.f};
    bool m_running{false};

    size_t m_samplesPerBeat{0};
    size_t m_beatSamplePos{0};
    size_t m_barBeatCount{0};
    size_t m_beatsPerBar{4};
    size_t m_preWindow{0};
    size_t m_postWindow{0};

    Subdivision m_subdivision{Subdivision::Off};
    std::vector<size_t> m_subPositions;

    AbacDsp::SvfResoBP m_tickFilter;
    AbacDsp::SvfResoBP m_beatOneFilter;
    AbacDsp::SvfResoBP m_subFilter;

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
};
