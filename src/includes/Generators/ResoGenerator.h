#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <numeric>

#include "Filters/SvfResoBP.h"
#include "PingExcitation.h"
#include "HarmonicGenerator.h"

/*
 * Resonance generator processor:
 * - generates a list of frequencies when triggered.
 * - list depends on various variables that control the distribution in frequency
 * and amplitude
 */

template <size_t BlockSize, size_t NumElements>
class ResoGenerator
{
  public:
    explicit ResoGenerator(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_excitation(1024)
    {
        for (auto& f : m_bq)
        {
            f.setSampleRate(sampleRate);
        }
    }

    void setExcitationNoise(const float value) noexcept
    {
        m_excitation.setNoise(value);
    }

    void setSoftExcitation(const float value) noexcept
    {
        m_softExcitation = value;
        // decide:
        // - how to excitate? add noise? multiply residual?
        // implement:
        // - range of excitation (m_minRangeExcitation, m_maxRangeExcitation) could be a curve of strength?
        // - set excitation level for processBlock to apply (best would be continued excitation?)
    }

    void setAttack(const float attackMs) noexcept
    {
        m_attack = attackMs;
        m_attackSamples = static_cast<int32_t>(m_attack * m_sampleRate * 0.001f);
    }

    void runHarmonicList(Harmonic* frequencyList, const size_t lastElement) noexcept
    {
        m_attackCounter = 0;

        for (size_t i = 0; i < lastElement; ++i)
        {
            m_trigger[i] = static_cast<float>(m_excitation.getPatternLength() - 1);
            m_frequencies[i] = frequencyList[i].f;
            m_triggerGain[i] = frequencyList[i].gain * logisticCompensation(frequencyList[i].f);
            m_triggerWait[i] = frequencyList[i].delay;
            m_activeState[i] = m_triggerWait[i] == 0 ? 1 : 2;
            m_bq[i].setByDecay(0, frequencyList[i].f, frequencyList[i].decay);
            m_bq[i].setByDecay(1, frequencyList[i].f, frequencyList[i].decay);

            const auto patternLength = static_cast<float>(m_excitation.getPatternLength());
            constexpr float periodsInPattern = 2.0f;
            const float samplesForTwoPeriods = (periodsInPattern / frequencyList[i].f) * m_sampleRate;
            m_phaseAdvance[i] = patternLength / samplesForTwoPeriods;
        }
        cntActive = lastElement;
    }

    void checkActivity() noexcept
    {
        cntActive = 0;
        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            if (m_activeState[j])
            {
                cntActive++;
            }
            if (m_activeState[j] == 1)
            {
                m_activeState[j] = m_bq[j].isActive() ? 1 : 0;
            }
        }
    }

    static float logisticCompensation(const float frequency) noexcept
    {
        const auto power = std::pow(frequency / 95.18412f, 1.189401f);
        const auto numerator = 1.f + power;
        const auto denominator = 0.8258689f + 0.006020447f * power;
        return numerator / denominator;
    }

    [[nodiscard]] std::array<float, NumElements> getFrequencies() const noexcept
    {
        return m_frequencies;
    }

    void setDampMode(const bool mode) noexcept
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

    void pitchBend(const size_t minNote, const size_t maxNote, const float normalized) noexcept
    {
        // const int minIndex = (static_cast<int>(minNote) - m_minMidiNote) * m_stepsPerSemitone;
        // const int maxIndex = (static_cast<int>(maxNote) - m_minMidiNote) * m_stepsPerSemitone;
        // const int startIdx = std::max(0, minIndex);
        // const int endIdx = std::min(static_cast<int>(NumElements) - 1, maxIndex);
        for (int index = 0; index < cntActive; ++index)
        {
            m_bq[index].pitchBend(normalized);
        }
    }

    void processBlock(std::array<float, BlockSize>& out) noexcept
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out[i] = 0.f;
        }

        if (!cntActive)
        {
            return;
        }

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
                        out[i] += m_bq[j].step(0.f);
                    }
                }
            }
        }
        if (m_attackCounter < m_attackSamples)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                const float attackGain = static_cast<float>(m_attackCounter) / m_attackSamples;
                out[i] *= attackGain;
                m_attackCounter++;
            }
        }
        checkActivity();
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return cntActive != 0;
    }

  private:
    float m_sampleRate;
    float m_attack = 0.0f;
    int32_t m_attackSamples = 0;
    int32_t m_attackCounter = 0;
    size_t m_countVoices{0};
    size_t lastCnt = 0;
    size_t cntActive = 0;
    int m_minMidiNote{0};
    int m_stepsPerSemitone{12};
    float m_softExcitation{0.f};
    std::array<float, NumElements> m_frequencies{};
    Excitation m_excitation;
    alignas(64) std::array<AbacDsp::SvfResoBP, NumElements> m_bq{};
    alignas(64) std::array<int32_t, NumElements> m_triggerWait{};
    alignas(64) std::array<float, NumElements> m_trigger{};
    alignas(64) std::array<float, NumElements> m_triggerGain{};
    alignas(64) std::array<float, NumElements> m_phaseAdvance{};
    alignas(64) std::array<int32_t, NumElements> m_activeState{};
};
