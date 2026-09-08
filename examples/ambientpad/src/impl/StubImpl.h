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
    void setLevel(const float value)
    {
        m_level = std::pow(10.f, value / 20.f);
    }
    void setNote(const float value)
    {
        m_note = value;
    }
    void setPlay(const bool value)
    {
        m_play = value;
    }
    void setMaterial(const float value)
    {
        m_material = value;
    }
    void setLight(const float value)
    {
        m_light = value;
    }
    void setMotion(const float value)
    {
        m_motion = value;
    }
    void setBreath(const float value)
    {
        m_breath = value;
    }
    void setStability(const float value)
    {
        m_stability = value;
    }
    void setBloom(const float value)
    {
        m_bloom = value;
    }
    void setHold(const bool value)
    {
        m_hold = value;
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
    float m_level{};
    float m_note{};
    bool m_play{};
    float m_material{};
    float m_light{};
    float m_motion{};
    float m_breath{};
    float m_stability{};
    float m_bloom{};
    bool m_hold{};
    float m_luaParam1{};
    float m_luaParam2{};
    float m_luaParam3{};
    float m_luaParam4{};
    float m_luaParam5{};
    float m_luaParam6{};
    float m_luaParam7{};
    float m_luaParam8{};
};