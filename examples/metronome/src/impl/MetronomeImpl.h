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
    static constexpr float tickFrequencyHz = 800.f;
    static constexpr float tickDecaySeconds = 0.04f;
    static constexpr float tickBoostDb = 24.f;

    // Max display window: ±250ms each side at 48 kHz = 24 000 samples total
    static constexpr size_t kVisualBufferSize = 24000;
    static constexpr float kDesiredHalfWindowMs = 250.f;

    explicit MetronomeImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_tickFilter(sampleRate)
    {
        m_tickFilter.setByDecay(0, tickFrequencyHz, tickDecaySeconds);
        m_samplesPerBeat = beatsToSamples(m_bpm);
        m_visualWavedata.resize(kVisualBufferSize, 0.f);
        updateHalfWindow();
    }

    void setBpm(const float value)
    {
        m_bpm = std::clamp(value, 40.f, 250.f);
        m_samplesPerBeat = beatsToSamples(m_bpm);
        updateHalfWindow();
    }

    void setMetroVolume(const float valueDb)
    {
        m_metroGain = std::pow(10.f, (valueDb + tickBoostDb) / 20.f);
    }

    void setInputVolume(const float valueDb)
    {
        m_inputGain = std::pow(10.f, valueDb / 20.f);
    }

    void setOnOff(const bool value)
    {
        m_running = value;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        const size_t halfWindow = m_halfWindow;

        auto writeInputPlusDoubletToVisualWindow = [&](const float visSignal)
        {
            if (m_beatSamplePos < halfWindow)
            {
                m_visualWavedata[halfWindow + m_beatSamplePos] = visSignal;
            }
            else if (m_beatSamplePos >= m_samplesPerBeat - halfWindow)
            {
                m_visualWavedata[m_beatSamplePos - (m_samplesPerBeat - halfWindow)] = visSignal;
            }
        };

        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (m_beatSamplePos == 0)
            {
                m_tickFilter.reset(0.f, m_metroGain);
            }

            const float tick = m_tickFilter.step0();
            out(i, 0) = in(i, 0) * m_inputGain + (m_running ? tick : 0.f);
            out(i, 1) = in(i, 1) * m_inputGain + (m_running ? tick : 0.f);

            const float doublet = !m_running ? 0.f : m_beatSamplePos == 0 ? 1.f : m_beatSamplePos == 1 ? -1.f : 0.f;
            writeInputPlusDoubletToVisualWindow(in(i, 0) + in(i, 1) + doublet);

            if (++m_beatSamplePos >= m_samplesPerBeat)
            {
                m_beatSamplePos = 0;
            }
        }
    }

    const std::vector<float>& visualizeWaveData()
    {
        const size_t windowSize = m_halfWindow * 2;
        m_preparedWavedata.assign(m_visualWavedata.begin(), m_visualWavedata.begin() + windowSize);
        return m_preparedWavedata;
    }

  private:
    void updateHalfWindow() noexcept
    {
        const size_t desired = static_cast<size_t>(sampleRate() * kDesiredHalfWindowMs / 1000.f);
        m_halfWindow = std::min(desired, m_samplesPerBeat / 2);
    }

    [[nodiscard]] size_t beatsToSamples(const float bpm) const noexcept
    {
        return static_cast<size_t>(sampleRate() * 60.f / bpm);
    }

    float m_bpm{120.f};
    float m_metroGain{std::pow(10.f, (-6.f + tickBoostDb) / 20.f)};
    float m_inputGain{1.f};
    bool m_running{false};
    size_t m_samplesPerBeat{0};
    size_t m_beatSamplePos{0};
    size_t m_halfWindow{3000};

    AbacDsp::SvfResoBP m_tickFilter;

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
};
