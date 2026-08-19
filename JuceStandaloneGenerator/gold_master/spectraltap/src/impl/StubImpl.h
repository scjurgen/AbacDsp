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
    void setDry(const float value)
    {
        m_dry = std::pow(10.f, value / 20.f);
    }
    void setWet(const float value)
    {
        m_wet = std::pow(10.f, value / 20.f);
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setHostSync(const bool value)
    {
        m_hostSync = value;
    }
    void setDivision(const size_t value)
    {
        m_division = value;
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
    float m_dry{};
    float m_wet{};
    float m_bpm{};
    bool m_hostSync{};
    size_t m_division{};
    float m_luaParam1{};
    float m_luaParam2{};
    float m_luaParam3{};
    float m_luaParam4{};
    float m_luaParam5{};
    float m_luaParam6{};
    float m_luaParam7{};
    float m_luaParam8{};
};