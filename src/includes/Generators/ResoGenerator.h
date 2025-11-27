#pragma once

#include <array>
#include <iostream>
#include <memory>
#include <random>
#include "Filters/BiquadResoBP.h"
#include "Excitation.h"

template <size_t BlockSize, size_t NumElements>
class ResoGenerator
{
  public:
    explicit ResoGenerator(const float sampleRate, const int minMidiNote = 24, const int stepsPerSemitone = 1)
        : m_sampleRate(sampleRate)
        , m_excitation(1024)
    {
        fillFrequencyTable(minMidiNote, stepsPerSemitone);
        calculatePhaseAdvances();

        for (auto& f : m_bq)
        {
            f.setSampleRate(sampleRate);
        }

        assignFrequencyAndDecay();
    }

    void setDecay(const float decay) noexcept
    {
        m_decay = decay;
        newDecay();
    }

    void setDecaySkew(const float value) noexcept
    {
        m_decaySkew = value;
        newDecay();
    }

    void setExcitationNoise(const float value) noexcept
    {
        m_excitation.setNoise(value);
    }

    void triggerNew(size_t index, float power, size_t triggerWaitBlocks)
    {
        m_trigger[index] = static_cast<float>(m_excitation.getPatternLength() - 1);
        m_triggerGain[index] = power;
        m_triggerWait[index] = triggerWaitBlocks;
        m_activeState[index] = triggerWaitBlocks == 0 ? 1 : 2;
        cntActive++;
    }

    int checkActivity()
    {
        cntActive = 0;
        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            if (m_activeState[j])
            {
                cntActive++;
                if (m_activeState[j] == 1)
                {
                    m_activeState[j] = m_bq[j].isActive() ? 1 : 0;
                }
            }
        }
        return cntActive;
    }

    void processBlock(std::array<float, BlockSize>& out) noexcept
    {
        std::ranges::fill(out, 0.f);
        // if (!cntActive)
        // {
        //     return;
        // }

        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            if (m_activeState[j] == 2)
            {
                --m_triggerWait[j];
                if (m_triggerWait[j] == 0)
                {
                    m_activeState[j] = 1;
                }
            }
            if (m_activeState[j] == 1)
            {
                if (m_trigger[j] <= 0.0f)
                {
                    for (size_t i = 0; i < BlockSize; ++i)
                    {
                        out[i] += m_bq[j].step0();
                    }
                }
                for (size_t i = 0; i < BlockSize; ++i)
                {
                    if (m_trigger[j] > 0.0f)
                    {
                        const auto excitationValue = m_excitation.getInterpolatedValue(m_trigger[j]);
                        const auto v = m_triggerGain[j] * excitationValue;
                        out[i] += m_bq[j].step(v);
                        m_trigger[j] -= m_phaseAdvance[j];
                        if (m_trigger[j] <= 0.0f)
                        {
                            m_triggerGain[j] = 0.f;
                        }
                    }
                    else
                    {
                        out[i] += m_bq[j].step0();
                    }
                }
            }
        }
    }

    void fillFrequencyTable(const int minMidiNote, const int stepsPerSemitone) noexcept
    {
        const auto baseFrequency = 440.f * std::pow(2.f, static_cast<float>(minMidiNote - 69) / 12.f);
        const auto slotsPerOctave = static_cast<float>(stepsPerSemitone) * 12;

        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            const auto f = baseFrequency * std::pow(2.f, static_cast<float>(j) / slotsPerOctave);
            m_frequencies[j] = f;
        }
        calculatePhaseAdvances();
        assignFrequencyAndDecay();
    }

    std::array<float, NumElements> getFrequencies() const
    {
        return m_frequencies;
    }

    void setDampMode(const bool mode)
    {
        for (auto& b : m_bq)
        {
            b.damp(mode);
        }
        if (mode)
        {
            std::ranges::fill(m_triggerWait, 0);
        }
    }

  private:
    void calculatePhaseAdvances() noexcept
    {
        const auto patternLength = static_cast<float>(m_excitation.getPatternLength());

        for (size_t j = 0; j < m_frequencies.size(); ++j)
        {
            const float samplesForTwoPeriods = (2.f / m_frequencies[j]) * m_sampleRate;
            m_phaseAdvance[j] = patternLength / samplesForTwoPeriods;
        }
    }


    void assignFrequencyAndDecay() noexcept
    {
        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            m_bq[j].setByDecay(0, m_frequencies[j], 0.01f + m_decay * 10.f);
            m_bq[j].setByDecay(1, m_frequencies[j], 0.1f);
        }
    }

    float m_decaySkew = 0.3f;

    void newDecay() noexcept
    {
        const float centerDecay = 0.02f + m_decay * 30.f;
        // Middle C as reference
        const float centerFreq = 440.f * std::pow(2.f, -9.f / 12.f); // ~261.63 Hz (middle C)

        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            float adjDecay = centerDecay;
            if (m_decaySkew != 0.0f)
            {
                const float octaveDistanceFromCenter = std::log2(m_frequencies[j] / centerFreq);
                const float skewMultiplier = std::pow(2.0f, -m_decaySkew * octaveDistanceFromCenter);
                adjDecay = centerDecay * skewMultiplier;
            }
            m_bq[j].setDecay(0, adjDecay);
        }
    }

    float m_sampleRate;
    float m_decay{0.1f};
    size_t m_countVoices{0};
    size_t lastCnt = 0;
    size_t cntActive = 0;

    std::array<float, NumElements> m_frequencies{};
    std::array<AbacDsp::BiquadResoBP, NumElements> m_bq{};
    Excitation m_excitation;
    std::array<size_t, NumElements> m_triggerWait{};
    std::array<float, NumElements> m_trigger{};
    std::array<float, NumElements> m_triggerGain{};
    std::array<float, NumElements> m_phaseAdvance{};
    std::array<int, NumElements> m_activeState{};
};
