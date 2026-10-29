#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class StubImpl final : public EffectBase
{
  public:
    explicit StubImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setDropBars(const size_t value)
    {
        m_dropBars = value;
    }
    void setMetroVolume(const float value)
    {
        m_metroVolume = std::pow(10.f, value / 20.f);
    }
    void setInputVolume(const float value)
    {
        m_inputVolume = std::pow(10.f, value / 20.f);
    }
    void setSubVolume(const float value)
    {
        m_subVolume = std::pow(10.f, value / 20.f);
    }
    void setOnOff(const bool value)
    {
        m_onOff = value;
    }
    void setHostSync(const bool value)
    {
        m_hostSync = value;
    }
    void setAnalysisMode(const bool value)
    {
        m_analysisMode = value;
    }
    void setAnalysisBars(const size_t value)
    {
        m_analysisBars = value;
    }
    void setLatencyComp(const float value)
    {
        m_latencyComp = value;
    }
    void setClickOut(const size_t value)
    {
        m_clickOut = value;
    }
    void setPreset(const size_t value)
    {
        m_preset = value;
    }
    void setVoicing(const size_t value)
    {
        m_voicing = value;
    }
    void setDrumKit(const size_t value)
    {
        m_drumKit = value;
    }
    void setSwingRatio(const float value)
    {
        m_swingRatio = value;
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
    float m_bpm{};
    size_t m_dropBars{};
    float m_metroVolume{};
    float m_inputVolume{};
    float m_subVolume{};
    bool m_onOff{};
    bool m_hostSync{};
    bool m_analysisMode{};
    size_t m_analysisBars{};
    float m_latencyComp{};
    size_t m_clickOut{};
    size_t m_preset{};
    size_t m_voicing{};
    size_t m_drumKit{};
    float m_swingRatio{};
};