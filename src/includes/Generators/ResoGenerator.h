#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

#include "Filters/SvfResoBP.h"
#include "HarmonicGenerator.h"
#include "Numbers/Convert.h"

namespace AbacDsp
{
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
    {
        for (auto& f : m_bq)
        {
            f.setSampleRate(sampleRate);
        }
    }

    void setExcitationNoise(const float value) noexcept {}

    void setSoftExcitation(const float value) noexcept
    {
        m_softExcitation = value;
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
            m_trigger[i] = 1;
            m_frequencies[i] = frequencyList[i].f;

            m_triggerGain[i] = frequencyList[i].gain *
                               AbacDsp::ResonanceCompensation::compensate(
                                   Convert::frequencyToNote<float>(frequencyList[i].f), frequencyList[i].decay);
            m_triggerWait[i] = frequencyList[i].delay;
            m_activeState[i] = m_triggerWait[i] == 0 ? 1 : 2;
            m_bq[i].setByDecay(0, frequencyList[i].f, frequencyList[i].decay);
            m_bq[i].setByDecay(1, frequencyList[i].f, frequencyList[i].decay);
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
        std::ranges::fill(out, 0.f);

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
                        m_bq[j].reset(0, m_triggerGain[j]);
                        m_trigger[j] = 0;
                    }
                    else
                    {
                        if (m_softExcitation)
                        {
                            std::uniform_real_distribution uniform(-m_softExcitation, m_softExcitation);

                            out[i] += m_bq[j].step(uniform(m_rng));
                        }
                        else
                        {
                            out[i] += m_bq[j].step0();
                        }
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
    mutable std::mt19937 m_rng{std::random_device{}()};
    std::array<float, NumElements> m_frequencies{};
    alignas(64) std::array<AbacDsp::SvfResoBP, NumElements> m_bq{};
    alignas(64) std::array<int32_t, NumElements> m_triggerWait{};
    alignas(64) std::array<float, NumElements> m_trigger{};
    alignas(64) std::array<float, NumElements> m_triggerGain{};
    alignas(64) std::array<float, NumElements> m_phaseAdvance{};
    alignas(64) std::array<int32_t, NumElements> m_activeState{};
};
}