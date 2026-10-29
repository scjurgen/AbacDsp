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
    void setClickOn(const bool value)
    {
        m_clickOn = value;
    }
    void setLatencyComp(const float value)
    {
        m_latencyComp = value;
    }
    void setInstrument(const size_t value)
    {
        m_instrument = value;
    }
    void setInput(const size_t value)
    {
        m_input = value;
    }
    void setOutputVolume(const float value)
    {
        m_outputVolume = std::pow(10.f, value / 20.f);
    }
    void setInputGain(const float value)
    {
        m_inputGain = std::pow(10.f, value / 20.f);
    }
    void setInstrumentLevel(const float value)
    {
        m_instrumentLevel = std::pow(10.f, value / 20.f);
    }
    void setClickLevel(const float value)
    {
        m_clickLevel = std::pow(10.f, value / 20.f);
    }
    void setListen(const bool value)
    {
        m_listen = value;
    }
    void setPlay(const bool value)
    {
        m_play = value;
    }
    void setReplay(const bool value)
    {
        m_replay = value;
    }
    void setStop(const bool value)
    {
        m_stop = value;
    }
    void setNext(const bool value)
    {
        m_next = value;
    }
    void setReference(const bool value)
    {
        m_reference = value;
    }
    void setSaveTakes(const bool value)
    {
        m_saveTakes = value;
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
    bool m_clickOn{};
    float m_latencyComp{};
    size_t m_instrument{};
    size_t m_input{};
    float m_outputVolume{};
    float m_inputGain{};
    float m_instrumentLevel{};
    float m_clickLevel{};
    bool m_listen{};
    bool m_play{};
    bool m_replay{};
    bool m_stop{};
    bool m_next{};
    bool m_reference{};
    bool m_saveTakes{};
};