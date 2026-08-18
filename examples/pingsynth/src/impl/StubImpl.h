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
    void setMode(const size_t value)
    {
        m_mode = value;
    }
    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }
    void setReverbLevel(const float value)
    {
        m_reverbLevel = std::pow(10.f, value / 20.f);
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
    size_t m_mode{};
    float m_vol{};
    float m_reverbLevel{};
    float m_luaParam1{};
    float m_luaParam2{};
    float m_luaParam3{};
    float m_luaParam4{};
    float m_luaParam5{};
    float m_luaParam6{};
    float m_luaParam7{};
    float m_luaParam8{};
};