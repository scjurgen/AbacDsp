#pragma once

#include "EffectBase.h"
#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"

#include <cassert>
#include <cstdint>
#include <cmath>
#include <functional>

template <size_t BlockSize>
class GenericImpl final : public EffectBase
{
  public:
    GenericImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setSync(const bool value)
    {
        m_sync = value;
    }
    void setType(const size_t value)
    {
        m_type = value;
    }
    void setSolo(const size_t value)
    {
        m_solo = value;
    }
    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }
    void setReverbLevelWet(const float value)
    {
        m_reverbLevelWet = std::pow(10.f, value / 20.f);
    }
    void setReverbDecay(const float value)
    {
        m_reverbDecay = value;
    }
    void setReverbShimmer(const float value)
    {
        m_reverbShimmer = value;
    }
    void setVol1(const float value)
    {
        m_vol1 = std::pow(10.f, value / 20.f);
    }
    void setVol2(const float value)
    {
        m_vol2 = std::pow(10.f, value / 20.f);
    }
    void setVol3(const float value)
    {
        m_vol3 = std::pow(10.f, value / 20.f);
    }
    void setVol4(const float value)
    {
        m_vol4 = std::pow(10.f, value / 20.f);
    }
    void setVol5(const float value)
    {
        m_vol5 = std::pow(10.f, value / 20.f);
    }
    void setVol6(const float value)
    {
        m_vol6 = std::pow(10.f, value / 20.f);
    }
    void setVol7(const float value)
    {
        m_vol7 = std::pow(10.f, value / 20.f);
    }
    void setVol8(const float value)
    {
        m_vol8 = std::pow(10.f, value / 20.f);
    }
    void setVol9(const float value)
    {
        m_vol9 = std::pow(10.f, value / 20.f);
    }
    void setVol10(const float value)
    {
        m_vol10 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed1(const float value)
    {
        m_revFeed1 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed2(const float value)
    {
        m_revFeed2 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed3(const float value)
    {
        m_revFeed3 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed4(const float value)
    {
        m_revFeed4 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed5(const float value)
    {
        m_revFeed5 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed6(const float value)
    {
        m_revFeed6 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed7(const float value)
    {
        m_revFeed7 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed8(const float value)
    {
        m_revFeed8 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed9(const float value)
    {
        m_revFeed9 = std::pow(10.f, value / 20.f);
    }
    void setRevFeed10(const float value)
    {
        m_revFeed10 = std::pow(10.f, value / 20.f);
    }
    void setPitch1(const float value)
    {
        m_pitch1 = value;
    }
    void setPitch2(const float value)
    {
        m_pitch2 = value;
    }
    void setPitch3(const float value)
    {
        m_pitch3 = value;
    }
    void setPitch4(const float value)
    {
        m_pitch4 = value;
    }
    void setPitch5(const float value)
    {
        m_pitch5 = value;
    }
    void setPitch6(const float value)
    {
        m_pitch6 = value;
    }
    void setPitch7(const float value)
    {
        m_pitch7 = value;
    }
    void setPitch8(const float value)
    {
        m_pitch8 = value;
    }
    void setPitch9(const float value)
    {
        m_pitch9 = value;
    }
    void setPitch10(const float value)
    {
        m_pitch10 = value;
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
    bool m_sync{};
    size_t m_type{};
    size_t m_solo{};
    float m_vol{};
    float m_reverbLevelWet{};
    float m_reverbDecay{};
    float m_reverbShimmer{};
    float m_vol1{};
    float m_vol2{};
    float m_vol3{};
    float m_vol4{};
    float m_vol5{};
    float m_vol6{};
    float m_vol7{};
    float m_vol8{};
    float m_vol9{};
    float m_vol10{};
    float m_revFeed1{};
    float m_revFeed2{};
    float m_revFeed3{};
    float m_revFeed4{};
    float m_revFeed5{};
    float m_revFeed6{};
    float m_revFeed7{};
    float m_revFeed8{};
    float m_revFeed9{};
    float m_revFeed10{};
    float m_pitch1{};
    float m_pitch2{};
    float m_pitch3{};
    float m_pitch4{};
    float m_pitch5{};
    float m_pitch6{};
    float m_pitch7{};
    float m_pitch8{};
    float m_pitch9{};
    float m_pitch10{};
};