#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "Audio/AudioBuffer.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "Delays/NaiveDelay.h"
#include "Diffuser/DiffusorDelayChain.h"
#include "EffectBase.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/Convert.h"
#include "Reverbs/FdnTankSpiced.h"

template <size_t BlockSize>
class MaxDiffuserImpl final : public EffectBase
{
  public:
    static constexpr size_t MaxDelaySamples{24000};
    static constexpr size_t MaxElements{50};
    static constexpr size_t MaxPreDelaySamples{96000};
    static constexpr size_t FdnOrder{32};
    static constexpr size_t FdnMaxSizePerElement{100000};
    static constexpr float FdnSizeSpread{4.3f};
    static constexpr float FdnInScale{1.f / static_cast<float>(FdnOrder)};
    static constexpr float FdnPresetBulge{-0.4f};

    using Chain = AbacDsp::DiffuserDelayChain<MaxDelaySamples, MaxElements, AbacDsp::AllpassFeedbackStyle::Schroeder>;
    using PreDelay = AbacDsp::NaiveDelay<MaxPreDelaySamples>;
    using Pitcher = AbacDsp::BlockProc::Pitch<BlockSize>;
    using Fdn = AbacDsp::FdnTankSpiced<FdnMaxSizePerElement, FdnOrder, BlockSize>;

    explicit MaxDiffuserImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_diffuser{AbacDsp::constructArray<Chain, 2>(sampleRate, BlockSize)}
        , m_pitcher{AbacDsp::constructArray<Pitcher, 2>(sampleRate)}
        , m_fdn{sampleRate}
    {
        for (auto& chain : m_diffuser)
        {
            chain.resetDiffuser(m_elements, 0.5f, m_bulge, 100.f, 1000.f, AbacDsp::skipSmoothing);
            chain.setModulationDepth(0.f);
        }
        // Only the left channel is metered for the bin-level display: both channels share
        // the same size/feedback/bulge and differ only in modulation phase.
        m_diffuser[0].setLevelMeterSink(&m_binLevelsDb);
        for (auto& delay : m_preDelay)
        {
            delay.setSize(0);
        }
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setReverse(false);
        }

        m_fdn.setSpreadBulge(FdnPresetBulge);
        setFdnSize(10.f);
        setFdnDecay(1000.f);
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
        m_diffuser[1].setBottomSize(value * 1.1f);
    }

    void setTopSize(const float value)
    {
        m_diffuser[0].setTopSize(value * 1.1f);
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

    void setPsola(const bool enabled)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPsolaEnabled(enabled);
        }
    }

    void setFdnMix(const float value)
    {
        m_fdnMix = Convert::dbToGain(value);
    }

    void setFdnSize(const float meters)
    {
        m_fdn.setMinSize(meters);
        m_fdn.setMaxSize(meters * FdnSizeSpread);
    }

    void setFdnDecay(const float msecs)
    {
        m_fdn.setDecay(msecs);
    }

    [[nodiscard]] std::array<float, MaxElements + 1> getProcessingBinLevels() const noexcept
    {
        std::array<float, MaxElements + 1> levels{};
        for (size_t i = 0; i < levels.size(); ++i)
        {
            levels[i] = m_binLevelsDb[i].load(std::memory_order_relaxed);
        }
        return levels;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<std::array<float, BlockSize>, 2> wetData{};
        for (size_t c = 0; c < 2; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                wetData[c][i] = in(i, c);
            }
            m_preDelay[c].processBlock(wetData[c], wetData[c]);
            m_pitcher[c].process(wetData[c]);
            m_diffuser[c].processBlock(wetData[c].data(), wetData[c].data(), BlockSize);
        }

        std::array<float, BlockSize> fdnIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            fdnIn[i] = FdnInScale * (wetData[0][i] + wetData[1][i]);
        }
        std::array<std::array<float, BlockSize>, 2> fdnOut{};
        m_fdn.processBlockSplit(fdnIn.data(), fdnOut[0].data(), fdnOut[1].data());

        for (size_t c = 0; c < 2; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                out(i, c) = m_dry * in(i, c) + m_wet * wetData[c][i] + m_fdnMix * fdnOut[c][i];
            }
        }
    }

  private:
    size_t m_elements{6};
    float m_bulge{0.46f};
    float m_dry{1.f};
    float m_wet{0.5f};
    float m_fdnMix{0.f};
    std::array<Chain, 2> m_diffuser;
    std::array<std::atomic<float>, MaxElements + 1> m_binLevelsDb{};
    std::array<PreDelay, 2> m_preDelay{};
    std::array<Pitcher, 2> m_pitcher;
    Fdn m_fdn;
};