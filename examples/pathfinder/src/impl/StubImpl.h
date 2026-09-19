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
        m_visualWavedata.resize(6000);
    }
    void setDepth(const float value)
    {
        m_depth = value;
    }
    void setSpeed(const float value)
    {
        m_speed = value;
    }
    void setAggressivity(const float value)
    {
        m_aggressivity = value;
    }
    void setCharacter(const float value)
    {
        m_character = value;
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

        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            m_currentSample++;
            if (m_currentSample >= m_visualWavedata.size())
            {
                m_currentSample = 0;
            }
        }
    }
    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata.resize(m_visualWavedata.size());
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    float m_depth{};
    float m_speed{};
    float m_aggressivity{};
    float m_character{};
    float m_luaParam1{};
    float m_luaParam2{};
    float m_luaParam3{};
    float m_luaParam4{};
    float m_luaParam5{};
    float m_luaParam6{};
    float m_luaParam7{};
    float m_luaParam8{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};