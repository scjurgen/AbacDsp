#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class GenericImpl final : public EffectBase
{
  public:
    explicit GenericImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }
    void setWet(const float value)
    {
        m_wet = std::pow(10.f, value / 20.f);
    }
    void setPreDelay(const float value)
    {
        m_preDelay = value;
    }
    void setElements(const float value)
    {
        m_elements = value;
    }
    void setTapSpan(const float value)
    {
        m_tapSpan = value;
    }
    void setFeedback(const float value)
    {
        m_feedback = value;
    }
    void setBulge(const float value)
    {
        m_bulge = value;
    }
    void setBottomSize(const float value)
    {
        m_bottomSize = value;
    }
    void setTopSize(const float value)
    {
        m_topSize = value;
    }
    void setSizeSpread(const float value)
    {
        m_sizeSpread = value;
    }
    void setModulationDepth(const float value)
    {
        m_modulationDepth = value;
    }
    void setModulationSpeed(const float value)
    {
        m_modulationSpeed = value;
    }
    void setLowPass(const float value)
    {
        m_lowPass = value;
    }
    void setMix(const float value)
    {
        m_mix = value;
    }
    void setPitch(const float value)
    {
        m_pitch = value;
    }
    void setPitchDelay(const float value)
    {
        m_pitchDelay = value;
    }
    void setPitch2(const float value)
    {
        m_pitch2 = value;
    }
    void setPitch2Delay(const float value)
    {
        m_pitch2Delay = value;
    }
    void setPitchMode(const size_t value)
    {
        m_pitchMode = value;
    }
    void setFdnMix(const float value)
    {
        m_fdnMix = std::pow(10.f, value / 20.f);
    }
    void setFdnSize(const float value)
    {
        m_fdnSize = value;
    }
    void setFdnDecay(const float value)
    {
        m_fdnDecay = value;
    }
    void setDrive(const float value)
    {
        m_drive = value;
    }
    void setEqInLow(const float value)
    {
        m_eqInLow = std::pow(10.f, value / 20.f);
    }
    void setEqInMid(const float value)
    {
        m_eqInMid = std::pow(10.f, value / 20.f);
    }
    void setEqInHigh(const float value)
    {
        m_eqInHigh = std::pow(10.f, value / 20.f);
    }
    void setEqOutLow(const float value)
    {
        m_eqOutLow = std::pow(10.f, value / 20.f);
    }
    void setEqOutMid(const float value)
    {
        m_eqOutMid = std::pow(10.f, value / 20.f);
    }
    void setEqOutHigh(const float value)
    {
        m_eqOutHigh = std::pow(10.f, value / 20.f);
    }
    void setLevel(const float value)
    {
        m_level = std::pow(10.f, value / 20.f);
    }
    void setPitcherShelfLow(const float value)
    {
        m_pitcherShelfLow = std::pow(10.f, value / 20.f);
    }
    void setPitcherShelfHigh(const float value)
    {
        m_pitcherShelfHigh = std::pow(10.f, value / 20.f);
    }
    void setExtremeStereoTap(const bool value)
    {
        m_extremeStereoTap = value;
    }
    void setWide(const float value)
    {
        m_wide = value;
    }
    void setReverbShelfLow(const float value)
    {
        m_reverbShelfLow = std::pow(10.f, value / 20.f);
    }
    void setReverbShelfHigh(const float value)
    {
        m_reverbShelfHigh = std::pow(10.f, value / 20.f);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = in(i, 0);
            out(i, 1) = in(i, 1);
        }
    }

  private:
    float m_dry{};
    float m_wet{};
    float m_preDelay{};
    float m_elements{};
    float m_tapSpan{};
    float m_feedback{};
    float m_bulge{};
    float m_bottomSize{};
    float m_topSize{};
    float m_sizeSpread{};
    float m_modulationDepth{};
    float m_modulationSpeed{};
    float m_lowPass{};
    float m_mix{};
    float m_pitch{};
    float m_pitchDelay{};
    float m_pitch2{};
    float m_pitch2Delay{};
    size_t m_pitchMode{};
    float m_fdnMix{};
    float m_fdnSize{};
    float m_fdnDecay{};
    float m_drive{};
    float m_eqInLow{};
    float m_eqInMid{};
    float m_eqInHigh{};
    float m_eqOutLow{};
    float m_eqOutMid{};
    float m_eqOutHigh{};
    float m_level{};
    float m_pitcherShelfLow{};
    float m_pitcherShelfHigh{};
    bool m_extremeStereoTap{};
    float m_wide{};
    float m_reverbShelfLow{};
    float m_reverbShelfHigh{};
};