#pragma once

#include "EffectBase.h"
#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "SmoothedGain.h"
#include "LatencyDelay.h"

#include "Filters/Biquad.h"

#include <vector>

template <size_t BlockSize>
class GainImpl final : public EffectBase
{
  public:
    constexpr static float lowShelfFrequency{120.f};
    constexpr static float highShelfFrequency{6000.f};
    constexpr static float qFactor{0.707f};

    explicit GainImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_smoothedGain(sampleRate)
        , m_latencyDelay(sampleRate)
    {
        for (auto& flt : m_lowShelving)
        {
            flt.computeCoefficients(sampleRate, lowShelfFrequency, qFactor, 0);
        }
        for (auto& flt : m_highShelving)
        {
            flt.computeCoefficients(sampleRate, lowShelfFrequency, qFactor, 0);
        }
        m_visualWavedata.resize(6000);
    }

    void setGain(const float value)
    {
        m_smoothedGain.setGain(value);
    }
    void setLatency(const float value)
    {
        m_latencyDelay.setLatency(value);
    }
    void setLowShelving(const float valueDb)
    {
        for (auto& flt : m_lowShelving)
        {
            flt.computeCoefficients(sampleRate(), lowShelfFrequency, qFactor, valueDb);
        }
    }
    void setHighShelving(const float valueDb)
    {
        for (auto& flt : m_highShelving)
        {
            flt.computeCoefficients(sampleRate(), highShelfFrequency, qFactor, valueDb);
        }
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float gain = m_smoothedGain.process();
            const auto sample = m_latencyDelay.process({in(i, 0), in(i, 1)});
            for (size_t c = 0; c < 2; ++c)
            {
                out(i, c) = m_highShelving[c].singleStepGeneric(m_lowShelving[c].singleStepGeneric(sample[c])) * gain;
            }
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            if (++m_currentSample >= m_visualWavedata.size())
            {
                m_currentSample = 0;
            }
        }
    }

    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    SmoothedGain m_smoothedGain;
    LatencyDelay m_latencyDelay;
    std::array<AbacDsp::Biquad<AbacDsp::BiquadFilterType::LoShelf>, 2> m_lowShelving;
    std::array<AbacDsp::Biquad<AbacDsp::BiquadFilterType::HiShelf>, 2> m_highShelving;

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample{};
};