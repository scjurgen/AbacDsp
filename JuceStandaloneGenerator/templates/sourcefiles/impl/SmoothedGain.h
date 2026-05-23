#pragma once

#include <cmath>
#include <numbers>

class SmoothedGain
{
  public:
    explicit SmoothedGain(const float sampleRate)
        : m_smoothCoeff(std::exp(-std::numbers::ln2_v<float> / (0.1f * sampleRate)))
    {
    }

    void setGain(const float dB)
    {
        m_targetGain = std::pow(10.f, dB / 20.f);
    }

    float process()
    {
        m_currentGain = m_smoothCoeff * m_currentGain + (1.f - m_smoothCoeff) * m_targetGain;
        return m_currentGain;
    }

  private:
    float m_targetGain{1.f};
    float m_currentGain{1.f};
    float m_smoothCoeff{};
};