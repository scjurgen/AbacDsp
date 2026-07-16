#pragma once

#include <array>
#include <cstddef>

#include "Audio/AudioBuffer.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "Delays/NaiveDelay.h"
#include "Diffuser/DiffusorDelayChain.h"
#include "EffectBase.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/Convert.h"

template <size_t BlockSize>
class MaxDiffuserImpl final : public EffectBase
{
  public:
    static constexpr size_t MaxDelaySamples{24000};
    static constexpr size_t MaxElements{50};
    static constexpr size_t MaxPreDelaySamples{96000};

    using Chain = AbacDsp::DiffuserDelayChain<MaxDelaySamples, MaxElements, AbacDsp::AllpassFeedbackStyle::Schroeder>;
    using PreDelay = AbacDsp::NaiveDelay<MaxPreDelaySamples>;
    using Pitcher = AbacDsp::BlockProc::Pitch<BlockSize>;

    explicit MaxDiffuserImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_diffuser{AbacDsp::constructArray<Chain, 2>(sampleRate, BlockSize)}
        , m_pitcher{AbacDsp::constructArray<Pitcher, 2>(sampleRate)}
    {
        for (auto& chain : m_diffuser)
        {
            chain.resetDiffuser(m_elements, 0.5f, m_bulge, 100.f, 1000.f, AbacDsp::skipSmoothing);
            chain.setModulationDepth(0.f);
        }
        for (auto& delay : m_preDelay)
        {
            delay.setSize(0);
        }
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setReverse(false);
        }

    }

    void setDry(const float value)
    {
        m_dry = Convert::dbToGain(value);
    }

    void setWet(const float value)
    {
        m_wet = Convert::dbToGain(value);
    }

    void setPreDelay(const float msecs)
    {
        const auto samples = static_cast<size_t>(msecs * 0.001f * sampleRate());
        for (auto& delay : m_preDelay)
        {
            delay.setSize(samples);
        }
    }

    void setElements(const float value)
    {
        m_elements = static_cast<size_t>(value);
        for (auto& chain : m_diffuser)
        {
            chain.setElements(m_elements);
            chain.setBulge(m_elements, m_bulge);
        }
    }

    void setFeedback(const float valueInPercentage)
    {
        const auto feedback = valueInPercentage * 0.00999f;
        for (auto& chain : m_diffuser)
        {
            chain.setFeedback(feedback);
        }
    }

    void setBulge(const float value)
    {
        m_bulge = value;
        m_diffuser[0].setBulge(m_elements, m_bulge);
        m_diffuser[1].setBulge(m_elements, m_bulge);
    }

    void setBottomSize(const float value)
    {
        m_diffuser[0].setBottomSize(value);
        m_diffuser[1].setBottomSize(value*1.1f);
    }

    void setTopSize(const float value)
    {
        m_diffuser[0].setTopSize(value*1.1f);
        m_diffuser[1].setTopSize(value);
    }

    void setModulationDepth(const float value)
    {
        for (auto& chain : m_diffuser)
        {
            chain.setModulationDepth(value);
        }
    }

    void setModulationSpeed(const float value)
    {
        for (auto& chain : m_diffuser)
        {
            chain.setModulationSpeed(value);
        }
    }

    void setLowPass(const float cutoff)
    {
        for (auto& chain : m_diffuser)
        {
            chain.setDamper(cutoff);
        }
    }

    void setMix(const float valueInPercentage)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPitchMix(valueInPercentage * 0.01f);
        }
    }

    void setPitch(const float semitones)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPitch(semitones);
        }
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t c = 0; c < 2; ++c)
        {
            std::array<float, BlockSize> wetData{};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                wetData[i] = in(i, c);
            }
            m_preDelay[c].processBlock(wetData, wetData);
            m_pitcher[c].process(wetData);
            m_diffuser[c].processBlock(wetData.data(), wetData.data(), BlockSize);
            for (size_t i = 0; i < BlockSize; ++i)
            {
                out(i, c) = m_dry * in(i, c) + m_wet * wetData[i];
            }
        }
    }

  private:
    size_t m_elements{6};
    float m_bulge{0.46f};
    float m_dry{1.f};
    float m_wet{0.5f};
    std::array<Chain, 2> m_diffuser;
    std::array<PreDelay, 2> m_preDelay{};
    std::array<Pitcher, 2> m_pitcher;
};