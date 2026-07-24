#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Audio/AudioBuffer.h"
#include "Delays/NaiveDelay.h"
#include "EffectBase.h"
#include "Filters/BiquadResoBP.h"
#include "Filters/SvfResoBP.h"

template <size_t BlockSize>
class ResonikImpl final : public EffectBase
{
  public:
    static constexpr size_t kMaxChains{100};
    // NaiveDelay allocates its full buffer per instance regardless of the delay currently in
    // use, and we hold kMaxChains of them, so the max delay time is sized against the (now
    // smaller) chain count to keep total buffer memory reasonable. Assumes the engine's fixed
    // 48 kHz internal rate (RateNormalizer::kInternalSampleRate).
    static constexpr float kMaxDelayMs{2000.f};
    static constexpr float kAssumedSampleRate{48000.f};
    static constexpr size_t kMaxDelaySamples{static_cast<size_t>(kAssumedSampleRate * kMaxDelayMs / 1000.f) + 4};

    explicit ResonikImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
        for (auto& biquad : m_biquad)
        {
            biquad.setSampleRate(sampleRate);
        }
        for (auto& svf : m_svf)
        {
            svf.setSampleRate(sampleRate);
        }
        recomputeFrequencies();
        recomputeDecay();
        recomputeGain();
        recomputeDelay();
    }

    void setNumChains(const float value)
    {
        m_activeChains = static_cast<size_t>(std::clamp(value, 1.f, static_cast<float>(kMaxChains)) + 0.5f);
        recomputeFrequencies();
        recomputeDecay();
        recomputeGain();
        recomputeDelay();
    }

    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }

    void setWet(const float value)
    {
        constexpr float kWetBoost = 10.f;
        m_wet = std::pow(10.f, value / 20.f) * kWetBoost;
    }

    void setLowFreq(const float value)
    {
        m_lowFreq = value;
        recomputeFrequencies();
    }

    void setHighFreq(const float value)
    {
        m_highFreq = value;
        recomputeFrequencies();
    }

    void setDistribution(const size_t value)
    {
        m_distribution = value;
        recomputeFrequencies();
    }

    void setDecayMin(const float value)
    {
        m_decayMin = value;
        recomputeDecay();
    }

    void setDecayMax(const float value)
    {
        m_decayMax = value;
        recomputeDecay();
    }

    void setGainMin(const float value)
    {
        m_gainMinDb = value;
        recomputeGain();
    }

    void setGainMax(const float value)
    {
        m_gainMaxDb = value;
        recomputeGain();
    }

    void setQ(const float value)
    {
        m_q = value;
        recomputeBiquad();
    }

    void setDelayMin(const float value)
    {
        m_delayMinMs = value;
        recomputeDelay();
    }

    void setDelayMax(const float value)
    {
        m_delayMaxMs = value;
        recomputeDelay();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<float, BlockSize> monoIn{};
        for (size_t s = 0; s < BlockSize; ++s)
        {
            monoIn[s] = 0.5f * (in(s, 0) + in(s, 1));
        }

        std::array<float, BlockSize> wetSum{};
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            auto& delay = m_delay[c];
            auto& biquad = m_biquad[c];
            auto& svf = m_svf[c];
            const auto gain = m_gainLin[c];
            for (size_t s = 0; s < BlockSize; ++s)
            {
                wetSum[s] += svf.step(biquad.step(delay.step(monoIn[s]))) * gain;
            }
        }

        for (size_t s = 0; s < BlockSize; ++s)
        {
            for (size_t c = 0; c < 2; ++c)
            {
                out(s, c) = m_dry * in(s, c) + m_wet * wetSum[s];
            }
        }
    }

  private:
    [[nodiscard]] float chainFraction(const size_t index) const noexcept
    {
        return m_activeChains > 1 ? static_cast<float>(index) / static_cast<float>(m_activeChains - 1) : 0.f;
    }

    void recomputeFrequencies()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            m_chainFreq[c] = m_distribution == 0 ? m_lowFreq + fraction * (m_highFreq - m_lowFreq)
                                                 : m_lowFreq * std::pow(m_highFreq / m_lowFreq, fraction);
        }
        recomputeBiquad();
        recomputeSvf();
    }

    void recomputeDecay()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            m_chainDecay[c] = m_decayMin + fraction * (m_decayMax - m_decayMin);
        }
        recomputeSvf();
    }

    void recomputeGain()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            const auto gainDb = m_gainMinDb + fraction * (m_gainMaxDb - m_gainMinDb);
            m_gainLin[c] = std::pow(10.f, gainDb / 20.f);
        }
    }

    void recomputeDelay()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            const auto fraction = chainFraction(c);
            const auto delayMs = m_delayMinMs + fraction * (m_delayMaxMs - m_delayMinMs);
            const auto delaySamples =
                static_cast<size_t>(std::clamp(delayMs, 0.f, kMaxDelayMs) * 0.001f * kAssumedSampleRate + 0.5f);
            m_delay[c].setSize(delaySamples);
        }
    }

    void recomputeBiquad()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            m_biquad[c].computeCoefficients(0, m_chainFreq[c], m_q);
        }
    }

    void recomputeSvf()
    {
        for (size_t c = 0; c < m_activeChains; ++c)
        {
            m_svf[c].setByDecay(0, m_chainFreq[c], m_chainDecay[c]);
        }
    }

    size_t m_activeChains{48};
    float m_dry{1.f};
    float m_wet{1.f};
    float m_lowFreq{80.f};
    float m_highFreq{6000.f};
    size_t m_distribution{1};
    float m_decayMin{0.3f};
    float m_decayMax{4.f};
    float m_gainMinDb{-18.f};
    float m_gainMaxDb{0.f};
    float m_q{6.f};
    float m_delayMinMs{0.f};
    float m_delayMaxMs{300.f};

    std::array<AbacDsp::BiquadResoBP, kMaxChains> m_biquad{};
    std::array<AbacDsp::SvfResoBP, kMaxChains> m_svf{};
    std::array<AbacDsp::NaiveDelay<kMaxDelaySamples>, kMaxChains> m_delay{};
    std::array<float, kMaxChains> m_chainFreq{};
    std::array<float, kMaxChains> m_chainDecay{};
    std::array<float, kMaxChains> m_gainLin{};
};