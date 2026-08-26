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
    void setTapeSpeed(const float value)
    {
        m_tapeSpeed = value;
    }
    void setBars(const float value)
    {
        m_bars = value;
    }
    void setInputGain(const float value)
    {
        m_inputGain = std::pow(10.f, value / 20.f);
    }
    void setGrooveLevel(const float value)
    {
        m_grooveLevel = std::pow(10.f, value / 20.f);
    }
    void setRecordA(const bool value)
    {
        m_recordA = value;
    }
    void setPlayA(const bool value)
    {
        m_playA = value;
    }
    void setClearA(const bool value)
    {
        m_clearA = value;
    }
    void setRecordB(const bool value)
    {
        m_recordB = value;
    }
    void setPlayB(const bool value)
    {
        m_playB = value;
    }
    void setClearB(const bool value)
    {
        m_clearB = value;
    }
    void setRecordC(const bool value)
    {
        m_recordC = value;
    }
    void setPlayC(const bool value)
    {
        m_playC = value;
    }
    void setClearC(const bool value)
    {
        m_clearC = value;
    }
    void setGroovePlay(const bool value)
    {
        m_groovePlay = value;
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setGrooveVariation(const float value)
    {
        m_grooveVariation = value;
    }
    void setTrackGainA(const float value)
    {
        m_trackGainA = std::pow(10.f, value / 20.f);
    }
    void setTrackGainB(const float value)
    {
        m_trackGainB = std::pow(10.f, value / 20.f);
    }
    void setTrackGainC(const float value)
    {
        m_trackGainC = std::pow(10.f, value / 20.f);
    }
    void setLuaParam1(const float value)
    {
        m_luaParam1 = value;
    }
    void setLuaParam2(const float value)
    {
        m_luaParam2 = value;
    }
    void setLuaParam3(const float value)
    {
        m_luaParam3 = value;
    }
    void setLuaParam4(const float value)
    {
        m_luaParam4 = value;
    }
    void setLuaParam5(const float value)
    {
        m_luaParam5 = value;
    }
    void setLuaParam6(const float value)
    {
        m_luaParam6 = value;
    }
    void setLuaParam7(const float value)
    {
        m_luaParam7 = value;
    }
    void setLuaParam8(const float value)
    {
        m_luaParam8 = value;
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
    float m_tapeSpeed{};
    float m_bars{};
    float m_inputGain{};
    float m_grooveLevel{};
    bool m_recordA{};
    bool m_playA{};
    bool m_clearA{};
    bool m_recordB{};
    bool m_playB{};
    bool m_clearB{};
    bool m_recordC{};
    bool m_playC{};
    bool m_clearC{};
    bool m_groovePlay{};
    float m_bpm{};
    float m_grooveVariation{};
    float m_trackGainA{};
    float m_trackGainB{};
    float m_trackGainC{};
    float m_luaParam1{};
    float m_luaParam2{};
    float m_luaParam3{};
    float m_luaParam4{};
    float m_luaParam5{};
    float m_luaParam6{};
    float m_luaParam7{};
    float m_luaParam8{};
};