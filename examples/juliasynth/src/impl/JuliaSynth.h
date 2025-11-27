#pragma once

#include "EffectBase.h"
#include "Filters/OnePoleFilter.h"

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "Generators/JuliaWalk.h"

#include <cassert>
#include <cstdint>
#include <cmath>
#include <functional>

template <size_t BlockSize>
class JuliaSynth final : public EffectBase
{
  public:
    JuliaSynth(const float sampleRate)
        : EffectBase(sampleRate)
        , m_jw(sampleRate)
        , m_hp(sampleRate)
    {
        m_visualWavedata.resize(6000);
        m_preparedWavedata.resize(6000);
        m_hp.setCutoff(10.f);
    }
    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }
    void setPower(const float value)
    {
        m_power = value;
        m_jw.setPower(value);
        m_jw.setPower(value);
    }
    void setCx(const float value)
    {
        m_cx = value;
        m_jw.setCx(value);
    }
    void setCy(const float value)
    {
        m_cy = value;
        m_jw.setCy(value);
    }
    void setJx(const float value)
    {
        m_jx = value;
        m_jw.setJx(value);
    }
    void setJy(const float value)
    {
        m_jy = value;
        m_jw.setJy(value);
    }
    void setJrx(const float value)
    {
        m_jrx = value;
        m_jw.setJxRad(value);
    }
    void setJry(const float value)
    {
        m_jry = value;
        m_jw.setJyRad(value);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float v = m_jw.next() * m_vol;
            out(i, 0) = v;
            out(i, 1) = v;
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
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    float m_vol{};
    float m_power{};
    float m_cx{};
    float m_cy{};
    float m_jx{};
    float m_jy{};
    float m_jrx{};
    float m_jry{};

    size_t m_currentSample = 0;
    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    JuliaWalk m_jw;
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass> m_hp;
};