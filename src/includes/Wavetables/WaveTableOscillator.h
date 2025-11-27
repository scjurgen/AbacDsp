#pragma once

#include "WaveTableStorage.h"
#include "Parameters/SmoothingParameter.h"
#include <cmath>
#include <vector>

namespace AbacDsp
{

/*
 * This wave table oscillator contains following options:
 * - 3 wavetables between we can morph, potentially it could be extended to a classic
 *       wavetable morph with N wavetables (e.g. serum)
 * - every wavetable consists of bandlimited wave tables and when select a start frequency
 *   we select the best table for the playout frequency (setFrequency)
 * - with changeFrequency you can change the pitch
 * - we have a pwm mode which can also be used (subtract wave from itself with a phase offset)
 * - morphmode has no smoothing (seems to be resilient to it)
 * - pwm needs smoothing, changes are quite drastic
 */
class WaveTableOscillator
{
  public:
    enum class PwmMode
    {
        Off,
        Soft,
        Strong
    };
    explicit WaveTableOscillator(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setWaveset(0, BasicWave::Sine);
        setWaveset(1, BasicWave::Square);
        setWaveset(2, BasicWave::Saw);
    }

    void setWaveset(const size_t targetIndex, const BasicWave waveIndex)
    {
        m_set[targetIndex] = WaveTableStore::getTableSet(waveIndex);
        applyMorph();
    }

    // don't change table when doing slight changes to pitch
    void changeFrequency(const float frequency) noexcept
    {
        if (m_frequency == frequency)
        {
            return;
        }
        m_frequency = frequency;
        m_phaseInc = frequency / m_sampleRate;
        m_invPhaseInc = m_sampleRate / frequency;
    }

    // only when setting a frequency for a new trigger we choose the tableindex
    void setFrequency(const float frequency) noexcept
    {
        if (m_frequency == frequency)
        {
            return;
        }
        m_frequency = frequency;
        m_phaseInc = frequency / m_sampleRate;
        m_invPhaseInc = m_sampleRate / frequency;
        updateWaveTableIndices();
    }

    void setPwmMode(const PwmMode mode)
    {
        m_pwmMode = mode;
    }

    void setPwm(const float pwm)
    {
        m_pwm.newTransition(std::clamp(pwm, 0.f, 1.f), 0.01f, m_sampleRate);
    }

    void setMorph(const float value)
    {
        m_morphValue = value;
        applyMorph();
    }

    float process() noexcept
    {
        updatePhase();
        return getOutput();
    }

    // block processing handles wrapping at end of table by calculating first the number of
    // samples we can process without wrapping
    void processBlock(float* target, const size_t numSamples)
    {
        if (!m_pwm.hasStoppedSmoothing())
        {
            if (const auto pwm = m_pwm.getValue(); m_pwmMode != PwmMode::Off)
            {
                m_phasorWithOffset = m_phasor + pwm;
                if (m_phasorWithOffset >= 1.f)
                {
                    m_phasorWithOffset -= 1.f;
                }
            }
        }

        if (const size_t samplesUntilWrap = (1.f - m_phasor) * m_invPhaseInc; samplesUntilWrap >= numSamples)
        {
            std::generate_n(target, numSamples,
                            [this]()
                            {
                                const float v = getOutput();
                                m_phasor += m_phaseInc;
                                return v;
                            });
            if (m_phasor >= 1.0f)
            {
                m_phasor -= 1.0f;
            }
        }
        else
        {
            if (samplesUntilWrap > 1)
            {
                std::generate_n(target, samplesUntilWrap,
                                [this]()
                                {
                                    const float v = getOutput();
                                    m_phasor += m_phaseInc;
                                    return v;
                                });
                if (m_phasor >= 1.0f)
                {
                    m_phasor -= 1.0f;
                }
                for (size_t i = samplesUntilWrap; i < numSamples; ++i)
                {
                    target[i] = getOutput();
                    updatePhase();
                }
            }
            else
            {
                for (size_t i = 0; i < numSamples; ++i)
                {
                    target[i] = getOutput();
                    updatePhase();
                }
            }
        }
        switch (m_pwmMode)
        {
            case PwmMode::Soft:
                subtractPwmBlock(target, numSamples, 0.25f);
                break;
            case PwmMode::Strong:
                subtractPwmBlock(target, numSamples, 1.f);
                break;
            case PwmMode::Off:
                break;
        }
        if (m_hasNoise)
        {
            addNoiseBlock(target, numSamples, m_noiseRatio);
        }
    }

  private:
    void updateWaveTableIndices() noexcept
    {
        // keep save against aliasing if we pitch up (we loose very high frequencies only>12k)
        // factor 2 seems to be a good guess (hearing estimated)
        m_curTableIdx[0] = m_set[m_tblSubIdx].getIndexByFrequency(m_phaseInc * 2);
        m_curTableIdx[1] = m_set[m_tblSubIdx + 1].getIndexByFrequency(m_phaseInc * 2);
    }

    void applyMorph()
    {
        const auto morph = std::clamp((m_morphValue + 1) * 0.5f, 0.f, 1.f) * (m_set.size() - 1);
        m_tblSubIdx = std::floor(morph);
        m_morph = morph - m_tblSubIdx;
        if (m_tblSubIdx >= m_set.size() - 1)
        {
            m_tblSubIdx = m_set.size() - 2;
            m_morph = 0.999999f;
        }
        handleNoise();
    }

    void handleNoise()
    {
        // if any of the active tables is a noise wave table
        const bool isWhite0 = m_set[m_tblSubIdx].wave == BasicWave::White;
        const bool isWhite1 = m_set[m_tblSubIdx + 1].wave == BasicWave::White;

        m_hasNoise = isWhite0 || isWhite1;

        if (isWhite0 && isWhite1)
        {
            m_noiseRatio = 1.f;
        }
        else if (isWhite0)
        {
            m_noiseRatio = std::abs(1 - m_morph);
        }
        else if (isWhite1)
        {
            m_noiseRatio = std::abs(m_morph);
        }
        else
        {
            m_noiseRatio = 0.f;
        }
    }

    [[nodiscard]] float getOutput() const noexcept
    {
        const auto& wt1 = m_set[m_tblSubIdx].tables[m_curTableIdx[0]];
        const auto& wt2 = m_set[m_tblSubIdx + 1].tables[m_curTableIdx[1]];
        const float pos = m_phasor * TableSize;
        const auto idx = static_cast<size_t>(pos);
        const float frac = pos - idx;
        const auto v1 = std::lerp(wt1.data[idx], wt1.data[idx + 1], frac);
        const auto v2 = std::lerp(wt2.data[idx], wt2.data[idx + 1], frac);
        return m_morph * v2 + (1 - m_morph) * v1;
    }

    [[nodiscard]] float getOutputOffset() const noexcept
    {
        const auto& wt1 = m_set[m_tblSubIdx].tables[m_curTableIdx[0]];
        const auto& wt2 = m_set[m_tblSubIdx + 1].tables[m_curTableIdx[1]];
        const float pos = m_phasorWithOffset * TableSize;
        const auto idx = static_cast<size_t>(pos);
        const float frac = pos - idx;
        const auto v1 = std::lerp(wt1.data[idx], wt1.data[idx + 1], frac);
        const auto v2 = std::lerp(wt2.data[idx], wt2.data[idx + 1], frac);
        return m_morph * v2 + (1 - m_morph) * v1;
    }

    [[nodiscard]] float getOutputMinusOffset() const noexcept
    {
        return getOutput() - getOutputOffset();
    }

    void updatePhase() noexcept
    {
        m_phasor += m_phaseInc;
        if (m_phasor >= 1.0f)
        {
            m_phasor -= 1.0f;
        }
    }

    void updatePhaseWithOffset() noexcept
    {
        m_phasorWithOffset += m_phaseInc;
        if (m_phasorWithOffset >= 1.0f)
        {
            m_phasorWithOffset -= 1.0f;
        }
    }

    void addNoiseBlock(float* target, const size_t numSamples, const float noiseRatio)
    {
        std::transform(target, target + numSamples, target,
                       [this, noiseRatio](const float in)
                       {
                           const float v = m_dist(m_rng) * noiseRatio;
                           return in + v;
                       });
    }

    void subtractPwmBlock(float* target, const size_t numSamples, const float factor)
    {
        if (const size_t samplesUntilWrap = (1.f - m_phasorWithOffset) * m_invPhaseInc; samplesUntilWrap >= numSamples)
        {
            std::transform(target, target + numSamples, target,
                           [&](const float in)
                           {
                               const float v = getOutputOffset();
                               m_phasorWithOffset += m_phaseInc;
                               return in - factor * v;
                           });
            if (m_phasorWithOffset >= 1.0f)
            {
                m_phasorWithOffset -= 1.0f;
            }
        }
        else
        {
            if (samplesUntilWrap > 1)
            {
                std::transform(target, target + samplesUntilWrap, target,
                               [&](const float in)
                               {
                                   const float v = getOutputOffset();
                                   m_phasorWithOffset += m_phaseInc;
                                   return in - factor * v;
                               });
                if (m_phasorWithOffset >= 1.0f)
                {
                    m_phasorWithOffset -= 1.0f;
                }
                for (size_t i = samplesUntilWrap; i < numSamples; ++i)
                {
                    target[i] -= factor * getOutputOffset();
                    updatePhaseWithOffset();
                }
            }
            else
            {
                for (size_t i = 0; i < numSamples; ++i)
                {
                    target[i] -= factor * getOutputOffset();
                    updatePhaseWithOffset();
                }
            }
        }
    }

    static constexpr size_t TableSize = WaveTableSize;
    float m_sampleRate;
    float m_phasor = 0.0f;
    float m_phasorWithOffset = 0.0f;
    float m_phaseInc = 0.0f;
    float m_invPhaseInc = 1.f;
    float m_frequency{1.f};
    float m_morphValue{0.f};

    size_t m_tblSubIdx{0};                     // Index into WaveTableCollection
    std::array<size_t, 2> m_curTableIdx{0, 0}; // Index within WaveTableSet
    std::array<WaveTableSet, 3> m_set{};
    bool m_hasNoise{false};
    float m_noiseRatio{0.f};
    PwmMode m_pwmMode{PwmMode::Off};
    LinearSmoothing m_pwm{0.5f};

    float m_morph{0.f};

    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_dist{-2.0f, 2.0f};
};
}