#pragma once

#include "EffectBase.h"

#include "Audio/AudioBuffer.h"
#include "Delays/OuModDelay.h"
#include "Filters/OnePoleFilter.h"
#include "Helpers/ConstructArray.h"

template <size_t BlockSize>
class DelayImpl final : public EffectBase
{
  public:
    using Delay = AbacDsp::OuModDelay<48000 * 4>;
    using LowPass = AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass>;
    using HighPass = AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>;
    using AllPass = AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::AllPass>;

    explicit DelayImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_delay{AbacDsp::constructArray<Delay, 2>(sampleRate)}
        , m_lowPass{AbacDsp::constructArray<LowPass, 2>(sampleRate)}
        , m_highPass{AbacDsp::constructArray<HighPass, 2>(sampleRate)}
        , m_allPass{AbacDsp::constructArray<AllPass, 2>(sampleRate)}
    {
        for (auto& f : m_lowPass)
        {
            f.setCutoff(16000);
        }
        for (auto& f : m_highPass)
        {
            f.setCutoff(50.f);
        }
        for (auto& f : m_allPass)
        {
            f.setCutoff(1500.f);
        }
        m_visualWavedata.resize(6000);
    }

    void setGain(const float value)
    {
        m_gain = std::pow(10.f, value / 20.f);
    }

    void setDry(const float value)
    {
        m_dryGain = std::pow(10.f, value / 20.f);
    }

    void setFeedback(const float valueInPercentage)
    {
        m_feedBack = valueInPercentage * 0.01f;
    }

    void setLowPass(const float cutoff)
    {
        for (auto& f : m_lowPass)
        {
            f.setCutoff(cutoff);
        }
    }

    void setHighPass(const float cutoff)
    {
        for (auto& f : m_highPass)
        {
            f.setCutoff(cutoff);
        }
    }

    void setAllPass(const float cutoff)
    {
        for (auto& f : m_allPass)
        {
            f.setCutoff(cutoff);
        }
    }

    void setModDepth(const float depthPercentage)
    {
        for (auto& f : m_delay)
        {
            f.setModDepth(depthPercentage * 0.01f);
        }
    }

    void setModSpeed(const float speed)
    {
        float factor = 1.f;
        for (auto& d : m_delay)
        {
            d.setModSpeed(speed * factor);
            factor *= 1.01f;
        }
    }

    void setModVariance(const float variance)
    {
        for (auto& f : m_delay)
        {
            f.setModVariance(variance * 0.01f);
        }
    }
    void setModDrift(const float drift)
    {
        for (auto& f : m_delay)
        {
            f.setModDrift(drift * 0.01f);
        }
    }
    void setTimeInMs(const float value)
    {
        for (auto& d : m_delay)
        {
            d.setSize(static_cast<size_t>(this->sampleRate() * value * 0.001f));
        }
    }

    void setWet(const float value)
    {
        m_wetGain = std::pow(10.f, value / 20.f);
    }

    void setLowPassCutoff(const float cutoff)
    {
        for (auto& f : m_lowPass)
        {
            f.setCutoff(cutoff);
        }
    }
    void setHighPassCutoff(const float cutoff)
    {
        for (auto& f : m_highPass)
        {
            f.setCutoff(cutoff);
        }
    }
    void setAllPassCutoff(const float cutoff)
    {
        for (auto& f : m_allPass)
        {
            f.setCutoff(cutoff);
        }
    }
    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            for (size_t c = 0; c < 2; ++c)
            {
                const float dry = in(i, c);
                const float filteredValue = m_allPass[c].step(
                    m_highPass[c].step(m_lowPass[c].step(dry * m_gain + m_feedBack * m_lastValue[c])));
                const auto value = m_delay[c].step(filteredValue);
                m_lastValue[c] = value;
                out(i, c) = value * m_wetGain + m_dryGain * dry;
            }
        }
        m_visualWavedata[m_currentSample] = (out(0, 0) + out(0, 1)) * 0.5f;
        m_currentSample++;
        if (m_currentSample >= m_visualWavedata.size())
        {
            m_currentSample = 0;
        }
    }
    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata.resize(m_visualWavedata.size());
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    float m_gain{};
    std::array<float, 2> m_lastValue{};
    std::array<Delay, 2> m_delay;
    std::array<LowPass, 2> m_lowPass;
    std::array<HighPass, 2> m_highPass;
    std::array<AllPass, 2> m_allPass;
    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
    float m_wetGain{0.2f};
    float m_dryGain{1.f};
    float m_feedBack{0.f};
};