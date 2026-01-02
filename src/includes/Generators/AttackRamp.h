#pragma once

#include <cmath>
#include <array>

namespace AbacDsp
{

template <size_t BlockSize>
class AttackRamp
{
  public:
    enum class RampMode
    {
        Linear,
        Exponential
    };

    enum class State
    {
        Idle,
        Ramping,
        Active
    };

    explicit AttackRamp(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_timeMs(2.0f)
        , m_mode(RampMode::Exponential)
        , m_state(State::Idle)
        , m_currentValue(0.0f)
        , m_sampleCount(0)
        , m_blocksRemaining(0)
        , m_increment(0.0f)
    {
        updateRampParameters();
    }

    void setAttackTimeMs(const float timeMs)
    {
        m_timeMs = timeMs;
        updateRampParameters();
    }

    void setMode(RampMode mode)
    {
        m_mode = mode;
        updateRampParameters();
    }

    void trigger()
    {
        m_sampleCount = 0;
        m_currentValue = 1E-6f;
        m_state = State::Ramping;
        m_blocksRemaining = m_totalBlocks;
    }

    bool isActive() const
    {
        return m_state != State::Idle;
    }

    void processBlock(std::array<float, BlockSize>& block)
    {
        if (m_state == State::Idle)
        {
            return;
        }

        if (m_mode == RampMode::Linear)
        {
            processLinearBlock(block);
        }
        else
        {
            processExponentialBlock(block);
        }
    }

  private:
    float m_sampleRate;
    float m_timeMs;
    RampMode m_mode;
    State m_state;
    float m_currentValue;
    size_t m_sampleCount;
    size_t m_totalBlocks;
    size_t m_blocksRemaining;
    float m_increment;
    float m_exponentialFactor;

    void updateRampParameters()
    {
        size_t totalSamples = static_cast<size_t>((m_timeMs / 1000.0f) * m_sampleRate + 0.5f);
        totalSamples = std::max(BlockSize, totalSamples);

        m_totalBlocks = (totalSamples + BlockSize - 1) / BlockSize;
        m_increment = 1.0f / static_cast<float>(m_totalBlocks * BlockSize);

        // Exponential factor: 1.0^(1/totalSamples) = base^(1/totalSamples)
        // For target=1, start=1e-6: factor = 1E6^(1/totalSamples)
        m_exponentialFactor = std::pow(1.0f / 1E-6f, 1.0f / static_cast<float>(totalSamples));
    }

    void processLinearBlock(std::array<float, BlockSize>& block)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            block[i] *= m_currentValue;
            m_currentValue += m_increment;
        }
        m_blocksRemaining--;
        if (m_blocksRemaining == 0)
        {
            m_state = State::Active;
        }
    }

    void processExponentialBlock(std::array<float, BlockSize>& block)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            block[i] *= m_currentValue;
            m_currentValue *= m_exponentialFactor;
        }

        m_blocksRemaining--;
        if (m_blocksRemaining == 0)
        {
            m_state = State::Active;
        }
    }
};

}
