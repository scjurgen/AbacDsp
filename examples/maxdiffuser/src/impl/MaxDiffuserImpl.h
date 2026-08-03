#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <iomanip>
#include <iostream>

#include "Audio/AudioBuffer.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "Delays/NaiveDelay.h"
#include "Diffuser/DiffusorDelayChain.h"
#include "EffectBase.h"
#include "Filters/Biquad.h"
#include "Filters/Distortion.h"
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

    // Fixed corner frequencies for the gain-only shaping bands; only the gains are exposed as
    // knobs. First-pass values, expected to be ear-tuned once the effect is running.
    static constexpr float EqLowHz{150.f};
    static constexpr float EqMidHz{1000.f};
    static constexpr float EqHighHz{4000.f};
    static constexpr float EqShelfQ{0.707f};
    static constexpr float EqPeakQ{0.7f};
    static constexpr float PitcherShelfLowHz{200.f};
    static constexpr float PitcherShelfHighHz{5000.f};
    static constexpr float ReverbShelfLowHz{150.f};
    static constexpr float ReverbShelfHighHz{6000.f};
    // Additional linear pre-gain applied to the distortion stage at 100% Drive.
    static constexpr float DriveMaxPreGain{30.f};

    using Chain = AbacDsp::DiffuserDelayChain<MaxDelaySamples, MaxElements, AbacDsp::AllpassFeedbackStyle::Schroeder>;
    using PreDelay = AbacDsp::NaiveDelay<MaxPreDelaySamples>;
    using Pitcher = AbacDsp::BlockProc::Pitch<BlockSize>;
    using Fdn = AbacDsp::FdnTankGlide<FdnMaxSizePerElement, FdnOrder, BlockSize>;
    using LoShelfFilter = AbacDsp::Biquad<AbacDsp::BiquadFilterType::LoShelf>;
    using HiShelfFilter = AbacDsp::Biquad<AbacDsp::BiquadFilterType::HiShelf>;
    using PeakFilter = AbacDsp::Biquad<AbacDsp::BiquadFilterType::Peak>;
    using Drive = AbacDsp::AtanhDrive;

    explicit MaxDiffuserImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_diffuser{AbacDsp::constructArray<Chain, 2>(sampleRate, BlockSize)}
        , m_pitcher{AbacDsp::constructArray<Pitcher, 2>(sampleRate)}
        , m_pitcher2{AbacDsp::constructArray<Pitcher, 2>(sampleRate)}
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
        for (auto& delay : m_pitch2Delay)
        {
            delay.setSize(0);
        }
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setReverse(false);
        }
        for (auto& pitcher : m_pitcher2)
        {
            pitcher.setReverse(false);
        }

        m_fdn.setSpreadBulge(FdnPresetBulge);
        setFdnSize(10.f);
        setFdnDecay(1000.f);
        m_fdn.setModulation(m_modulationDepth, m_modulationSpeed);

        setEqInLow(0.f);
        setEqInMid(0.f);
        setEqInHigh(0.f);
        setDrive(0.f);
        setEqOutLow(0.f);
        setEqOutMid(0.f);
        setEqOutHigh(0.f);
        setLevel(0.f);
        setPitcherShelfLow(0.f);
        setPitcherShelfHigh(0.f);
        setExtremeStereoTap(false);
        setWide(0.f);
        setReverbShelfLow(0.f);
        setReverbShelfHigh(0.f);
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

    // Distortion block (EqIn -> Drive -> EqOut -> Level), dry path only, between Pre Delay and
    // the diffuser chain input; the pitch taps are never distorted.
    void setEqInLow(const float valueDb)
    {
        for (auto& f : m_eqInLow)
        {
            f.computeCoefficients(sampleRate(), EqLowHz, EqShelfQ, valueDb);
        }
    }

    void setEqInMid(const float valueDb)
    {
        for (auto& f : m_eqInMid)
        {
            f.computeCoefficients(sampleRate(), EqMidHz, EqPeakQ, valueDb);
        }
    }

    void setEqInHigh(const float valueDb)
    {
        for (auto& f : m_eqInHigh)
        {
            f.computeCoefficients(sampleRate(), EqHighHz, EqShelfQ, valueDb);
        }
    }

    void setDrive(const float valueInPercentage)
    {
        const auto drive = valueInPercentage * 0.01f * DriveMaxPreGain;
        for (auto& d : m_drive)
        {
            d.setDrive(drive);
        }
    }

    void setEqOutLow(const float valueDb)
    {
        for (auto& f : m_eqOutLow)
        {
            f.computeCoefficients(sampleRate(), EqLowHz, EqShelfQ, valueDb);
        }
    }

    void setEqOutMid(const float valueDb)
    {
        for (auto& f : m_eqOutMid)
        {
            f.computeCoefficients(sampleRate(), EqMidHz, EqPeakQ, valueDb);
        }
    }

    void setEqOutHigh(const float valueDb)
    {
        for (auto& f : m_eqOutHigh)
        {
            f.computeCoefficients(sampleRate(), EqHighHz, EqShelfQ, valueDb);
        }
    }

    void setLevel(const float valueDb)
    {
        m_level = Convert::dbToGain(valueDb);
    }

    // Hi/Lo shelf shared by both pitcher paths, applied right after each pitcher and before the
    // 3-way mix into the diffuser chain.
    void setPitcherShelfLow(const float valueDb)
    {
        for (auto& f : m_pitch1ShelfLow)
        {
            f.computeCoefficients(sampleRate(), PitcherShelfLowHz, EqShelfQ, valueDb);
        }
        for (auto& f : m_pitch2ShelfLow)
        {
            f.computeCoefficients(sampleRate(), PitcherShelfLowHz, EqShelfQ, valueDb);
        }
    }

    void setPitcherShelfHigh(const float valueDb)
    {
        for (auto& f : m_pitch1ShelfHigh)
        {
            f.computeCoefficients(sampleRate(), PitcherShelfHighHz, EqShelfQ, valueDb);
        }
        for (auto& f : m_pitch2ShelfHigh)
        {
            f.computeCoefficients(sampleRate(), PitcherShelfHighHz, EqShelfQ, valueDb);
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
        const auto samples = msecsToSamples(msecs);
        for (auto& delay : m_pitch2Delay)
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

    void setTapSpan(const float valueInPercentage)
    {
        for (auto& chain : m_diffuser)
        {
            chain.setTapSpan(valueInPercentage);
        }
    }

    // Keeps the two independent L/R diffuser chains; only changes how each channel's own
    // tap-mix window is weighted (see DiffuserDelayChain::TapParity).
    void setExtremeStereoTap(const bool enabled)
    {
        m_extremeStereoTap = enabled;
        m_diffuser[0].setTapParity(enabled ? Chain::TapParity::EvenOnly : Chain::TapParity::All);
        m_diffuser[1].setTapParity(enabled ? Chain::TapParity::OddOnly : Chain::TapParity::All);
    }

    // Effective only when Extreme Stereo Tap is on: 0 collapses L/R to their mono sum, +-100
    // reaches the full/swapped tap-split image.
    void setWide(const float valueInPercentage)
    {
        m_wide = std::clamp(valueInPercentage, -100.f, 100.f) * 0.01f;
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
        logElementSizes();
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
        for (auto& pitcher : m_pitcher2)
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

    void setPitch2(const float semitones)
    {
        for (auto& pitcher : m_pitcher2)
        {
            pitcher.setPitch(semitones);
        }
    }

    void setPitchMode(const int mode)
    {
        for (auto& pitcher : m_pitcher)
        {
            pitcher.setPsolaEnabled(mode == 1);
            pitcher.setPhaseVocoderEnabled(mode == 2);
        }
        for (auto& pitcher : m_pitcher2)
        {
            pitcher.setPsolaEnabled(mode == 1);
            pitcher.setPhaseVocoderEnabled(mode == 2);
        }
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

    void setReverbShelfLow(const float valueDb)
    {
        for (auto& f : m_reverbShelfLow)
        {
            f.computeCoefficients(sampleRate(), ReverbShelfLowHz, EqShelfQ, valueDb);
        }
    }

    void setReverbShelfHigh(const float valueDb)
    {
        for (auto& f : m_reverbShelfHigh)
        {
            f.computeCoefficients(sampleRate(), ReverbShelfHighHz, EqShelfQ, valueDb);
        }
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
        // Three independent taps feed the diffuser: dry (Pre Delay, unpitched, distorted),
        // pitch1 (Pitch Delay) and pitch2 (Pitch 2 Delay), the latter two per channel and
        // shelved. Each is delayed straight from the raw input so their delay times don't
        // compound, letting the three arrive with independent time offsets relative to each
        // other.
        std::array<std::array<float, BlockSize>, 2> dryToDiffuser{};
        std::array<std::array<float, BlockSize>, 2> pitch1Data{};
        std::array<std::array<float, BlockSize>, 2> pitch2Data{};
        std::array<float, BlockSize> distortionScratch{};
        for (size_t c = 0; c < 2; ++c)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                dryToDiffuser[c][i] = in(i, c);
                pitch1Data[c][i] = in(i, c);
                pitch2Data[c][i] = in(i, c);
            }
            m_preDelay[c].processBlock(dryToDiffuser[c], dryToDiffuser[c]);

            m_eqInLow[c].processBlock(dryToDiffuser[c].data(), distortionScratch.data(), BlockSize);
            m_eqInMid[c].processBlock(distortionScratch.data(), distortionScratch.data(), BlockSize);
            m_eqInHigh[c].processBlock(distortionScratch.data(), distortionScratch.data(), BlockSize);
            m_drive[c].processBlock(distortionScratch.data(), distortionScratch.data(), BlockSize);
            m_eqOutLow[c].processBlock(distortionScratch.data(), distortionScratch.data(), BlockSize);
            m_eqOutMid[c].processBlock(distortionScratch.data(), distortionScratch.data(), BlockSize);
            m_eqOutHigh[c].processBlock(distortionScratch.data(), distortionScratch.data(), BlockSize);
            for (size_t i = 0; i < BlockSize; ++i)
            {
                dryToDiffuser[c][i] = distortionScratch[i] * m_level;
            }

            m_pitchDelay[c].processBlock(pitch1Data[c], pitch1Data[c]);
            m_pitcher[c].process(pitch1Data[c]);
            m_pitch1ShelfLow[c].processBlock(pitch1Data[c].data(), pitch1Data[c].data(), BlockSize);
            m_pitch1ShelfHigh[c].processBlock(pitch1Data[c].data(), pitch1Data[c].data(), BlockSize);

            m_pitch2Delay[c].processBlock(pitch2Data[c], pitch2Data[c]);
            m_pitcher2[c].process(pitch2Data[c]);
            m_pitch2ShelfLow[c].processBlock(pitch2Data[c].data(), pitch2Data[c].data(), BlockSize);
            m_pitch2ShelfHigh[c].processBlock(pitch2Data[c].data(), pitch2Data[c].data(), BlockSize);
        }

        std::array<std::array<float, BlockSize>, 2> wetData{};
        for (size_t c = 0; c < 2; ++c)
        {
            constexpr float third{1.f / 3.f};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                wetData[c][i] = third * (dryToDiffuser[c][i] + pitch1Data[c][i] + pitch2Data[c][i]);
            }
            m_diffuser[c].processBlock(wetData[c].data(), wetData[c].data(), BlockSize);
        }

        // Wide only applies in Extreme Stereo Tap mode; wetData is left untouched otherwise.
        // rawL+rawR is invariant under this crossfade, so the FDN feed below is unaffected.
        if (m_extremeStereoTap)
        {
            for (size_t i = 0; i < BlockSize; ++i)
            {
                const auto mono = 0.5f * (wetData[0][i] + wetData[1][i]);
                const auto rawL = wetData[0][i];
                const auto rawR = wetData[1][i];
                wetData[0][i] = mono + m_wide * (rawL - mono);
                wetData[1][i] = mono + m_wide * (rawR - mono);
            }
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
            m_reverbShelfLow[c].processBlock(fdnOut[c].data(), fdnOut[c].data(), BlockSize);
            m_reverbShelfHigh[c].processBlock(fdnOut[c].data(), fdnOut[c].data(), BlockSize);
        }

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

    // Per-element left/right delay lengths and their delta, in samples, for auditing the
    // stereo Size Spread control from the console.
    void logElementSizes() const
    {
        // const auto left = m_diffuser[0].getElementSizesInSamples();
        // const auto right = m_diffuser[1].getElementSizesInSamples();
        // std::cout << "element  left  right  delta\n";
        // for (size_t i = 0; i < m_elements; ++i)
        // {
        //     const auto delta = static_cast<long long>(left[i]) - static_cast<long long>(right[i]);
        //     std::cout << std::setw(7) << i << std::setw(6) << left[i] << std::setw(7) << right[i] << std::setw(7)
        //               << delta << "\n";
        // }
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
    std::array<PreDelay, 2> m_pitch2Delay{};
    std::array<Pitcher, 2> m_pitcher;
    std::array<Pitcher, 2> m_pitcher2;
    Fdn m_fdn;

    std::array<LoShelfFilter, 2> m_eqInLow{};
    std::array<PeakFilter, 2> m_eqInMid{};
    std::array<HiShelfFilter, 2> m_eqInHigh{};
    std::array<Drive, 2> m_drive{};
    std::array<LoShelfFilter, 2> m_eqOutLow{};
    std::array<PeakFilter, 2> m_eqOutMid{};
    std::array<HiShelfFilter, 2> m_eqOutHigh{};
    float m_level{1.f};

    std::array<LoShelfFilter, 2> m_pitch1ShelfLow{};
    std::array<HiShelfFilter, 2> m_pitch1ShelfHigh{};
    std::array<LoShelfFilter, 2> m_pitch2ShelfLow{};
    std::array<HiShelfFilter, 2> m_pitch2ShelfHigh{};

    bool m_extremeStereoTap{false};
    float m_wide{0.f};

    std::array<LoShelfFilter, 2> m_reverbShelfLow{};
    std::array<HiShelfFilter, 2> m_reverbShelfHigh{};
};