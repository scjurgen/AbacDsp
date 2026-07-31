#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>

#include "Audio/AudioBuffer.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "Delays/NaiveDelay.h"
#include "Diffuser/DiffusorDelayChain.h"
#include "EffectBase.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/Convert.h"
#include "Reverbs/FdnTankGlide.h"

template <size_t BlockSize>
class MaxDiffuserImpl final : public EffectBase
{
  public:
    static constexpr size_t MaxDelaySamples{24000};
    static constexpr size_t MaxElements{50};
    static constexpr size_t MaxPreDelaySamples{96000};
    static constexpr size_t FdnOrder{32};
    static constexpr size_t FdnMaxSizePerElement{100000};
    static constexpr float FdnSizeSpread{2.3f};
    static constexpr float FdnInScale{1.f / static_cast<float>(FdnOrder)};
    static constexpr float FdnPresetBulge{-0.4f};
    static constexpr float BandLowHz{20.f};
    static constexpr float BandHighHz{300.f};

    using Chain = AbacDsp::DiffuserDelayChain<MaxDelaySamples, MaxElements, AbacDsp::AllpassFeedbackStyle::Schroeder>;
    using PreDelay = AbacDsp::NaiveDelay<MaxPreDelaySamples>;
    using Pitcher = AbacDsp::BlockProc::Pitch<BlockSize>;
    using Fdn = AbacDsp::FdnTankGlide<FdnMaxSizePerElement, FdnOrder, BlockSize>;

    explicit MaxDiffuserImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_diffuser{AbacDsp::constructArray<Chain, 2>(sampleRate, BlockSize)}
        , m_pitcher{AbacDsp::constructArray<Pitcher, 2>(sampleRate)}
        , m_pitcher2{sampleRate}
        , m_fdn{sampleRate}
    {
        for (auto& chain : m_diffuser)
        {
            chain.resetDiffuser(m_elements, 0.5f, m_bulge, 0.7f, 7.f, AbacDsp::skipSmoothing);
            chain.setModulationDepth(0.f);
        }
        // Only the left channel is metered for the bin-level display: both channels share
        // the same size/feedback/bulge and differ only in modulation phase.
        m_diffuser[0].setLevelMeterSink(&m_binLevels);
        m_diffuser[0].configureBandFilters(sampleRate, BandLowHz, BandHighHz);
        m_diffuser[0].setBandLevelMeterSink(&m_bandLevels);
        for (auto& delay : m_preDelay)
        {
            delay.setSize(0);
        }
        for (auto& delay : m_pitchDelay)
        {
            delay.setSize(0);
        }
        m_pitch2Delay.setSize(0);
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setReverse(false);
        }
        m_pitcher2.setReverse(false);

        m_fdn.setSpreadBulge(FdnPresetBulge);
        setFdnSize(10.f);
        setFdnDecay(1000.f);
        m_fdn.setModulation(m_modulationDepth, m_modulationSpeed);
    }

    void setDry(const float value)
    {
        m_dry = Convert::dbToGain(value);
    }

    void setWet(const float value)
    {
        m_wet = Convert::dbToGain(value);
    }

    // Delays the unpitched signal fed into the diffuser chain (independent of the final
    // dry-output blend); setPitchDelay/setPitch2Delay delay the other two, pitched, taps.
    void setPreDelay(const float msecs)
    {
        const auto samples = msecsToSamples(msecs);
        for (auto& delay : m_preDelay)
        {
            delay.setSize(samples);
        }
    }

    void setPitchDelay(const float msecs)
    {
        const auto samples = msecsToSamples(msecs);
        for (auto& delay : m_pitchDelay)
        {
            delay.setSize(samples);
        }
    }

    void setPitch2Delay(const float msecs)
    {
        m_pitch2Delay.setSize(msecsToSamples(msecs));
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
        m_diffuser[1].setBottomSize(value);
    }

    void setTopSize(const float value)
    {
        m_diffuser[0].setTopSize(value);
        m_diffuser[1].setTopSize(value);
    }

    void setSizeSpread(const float valueInMeters)
    {
        m_diffuser[0].setSizeSpread(valueInMeters, false);
        m_diffuser[1].setSizeSpread(valueInMeters, true);
    }

    void setModulationDepth(const float value)
    {
        m_modulationDepth = value;
        for (auto& chain : m_diffuser)
        {
            chain.setModulationDepth(value);
        }
        m_fdn.setModulation(m_modulationDepth, m_modulationSpeed);
    }

    void setModulationSpeed(const float value)
    {
        m_modulationSpeed = value;

        for (size_t o = 0; o < m_diffuser.size(); ++o)
        {
            const float speedFactor = 1 + 0.2f * static_cast<float>(o) / static_cast<float>(m_elements);
            m_diffuser[o].setModulationSpeed(value * speedFactor);
        }
        m_fdn.setModulation(m_modulationDepth, m_modulationSpeed);
    }

    void setLowPass(const float cutoff)
    {
        for (auto& chain : m_diffuser)
        {
            chain.setDamper(cutoff);
        }
        m_fdn.setDamping(cutoff);
    }

    void setMix(const float valueInPercentage)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPitchMix(valueInPercentage * 0.01f);
        }
        m_pitcher2.setPitchMix(valueInPercentage * 0.01f);
    }

    void setPitch(const float semitones)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPitch(semitones);
        }
    }

    void setPitch2(const float semitones)
    {
        m_pitcher2.setPitch(semitones);
    }

    void setPitchMode(const int mode)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPsolaEnabled(mode == 1);
            pitcher.setPhaseVocoderEnabled(mode == 2);
        }
        m_pitcher2.setPsolaEnabled(mode == 1);
        m_pitcher2.setPhaseVocoderEnabled(mode == 2);
    }

    void setFdnMix(const float value)
    {
        m_fdnMix = Convert::dbToGain(value);
    }

    void setFdnSize(const float meters)
    {
        m_fdn.setMinSize(meters / FdnSizeSpread);
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
            levels[i] = m_binLevels[i].load(std::memory_order_relaxed);
        }
        return levels;
    }

    [[nodiscard]] std::array<std::array<float, 3>, MaxElements + 1> getProcessingBinBandLevels() const noexcept
    {
        std::array<std::array<float, 3>, MaxElements + 1> levels{};
        for (size_t i = 0; i < levels.size(); ++i)
        {
            levels[i][0] = m_bandLevels[i][0].load(std::memory_order_relaxed);
            levels[i][1] = m_bandLevels[i][1].load(std::memory_order_relaxed);
            levels[i][2] = m_bandLevels[i][2].load(std::memory_order_relaxed);
        }
        return levels;
    }

    [[nodiscard]] std::array<float, MaxElements> getElementSizesInMeters() const noexcept
    {
        return m_diffuser[0].getElementSizesInMeters();
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        // Three independent taps feed the diffuser: dry (Pre Delay, unpitched), pitch1 (Pitch
        // Delay, per channel) and pitch2 (Pitch 2 Delay, mono). Each is delayed straight from the
        // raw input so their delay times don't compound, letting the three arrive with
        // independent time offsets relative to each other.
        std::array<std::array<float, BlockSize>, 2> dryToDiffuser{};
        std::array<std::array<float, BlockSize>, 2> pitch1Data{};
        for (size_t c = 0; c < 2; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                dryToDiffuser[c][i] = in(i, c);
                pitch1Data[c][i] = in(i, c);
            }
            m_preDelay[c].processBlock(dryToDiffuser[c], dryToDiffuser[c]);
            m_pitchDelay[c].processBlock(pitch1Data[c], pitch1Data[c]);
            m_pitcher[c].process(pitch1Data[c]);
        }

        // Second pitch voice is mono, in parallel to the per-channel pitcher above: it taps the
        // raw input (its own delay applied independently), then its result is averaged into both
        // channels equally, rather than a straight per-channel pitch shift.
        std::array<float, BlockSize> monoDry{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            monoDry[i] = 0.5f * (in(i, 0) + in(i, 1));
        }
        m_pitch2Delay.processBlock(monoDry, monoDry);
        m_pitcher2.process(monoDry);

        std::array<std::array<float, BlockSize>, 2> wetData{};
        for (size_t c = 0; c < 2; ++c)
        {
            constexpr float third{1.f / 3.f};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                wetData[c][i] = third * (dryToDiffuser[c][i] + pitch1Data[c][i] + monoDry[i]);
            }
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
    [[nodiscard]] size_t msecsToSamples(const float msecs) const noexcept
    {
        return static_cast<size_t>(msecs * 0.001f * sampleRate());
    }

    void logElementSizes() const
    {
        std::cout << " size: ";
        std::array<size_t, 2> absVal{};
        for (size_t c = 0; c < m_diffuser.size(); ++c)
        {
            const auto sizes = m_diffuser[c].getElementSizesInSamples();
            size_t total = 0;
            for (size_t i = 0; i < m_elements; ++i)
            {
                total += sizes[i];
            }
            std::cout << total;
            absVal[c] = total;
            if (c == 0)
            {
                std::cout << ", ";
            }
        }
        std::cout << " -> " << static_cast<long long>(absVal[0]) - static_cast<long long>(absVal[1]) << "\n";
    }

    size_t m_elements{6};
    float m_bulge{0.46f};
    float m_dry{1.f};
    float m_wet{0.5f};
    float m_fdnMix{0.f};
    float m_modulationDepth{0.f};
    float m_modulationSpeed{0.5f};
    std::array<Chain, 2> m_diffuser;
    std::array<std::atomic<float>, MaxElements + 1> m_binLevels{};
    Chain::BandLevelSink m_bandLevels{};
    std::array<PreDelay, 2> m_preDelay{};
    std::array<PreDelay, 2> m_pitchDelay{};
    PreDelay m_pitch2Delay{};
    std::array<Pitcher, 2> m_pitcher;
    Pitcher m_pitcher2;
    Fdn m_fdn;
};