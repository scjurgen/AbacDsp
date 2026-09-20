#pragma once

#include "Filters/Biquad.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief 4th-order Linkwitz-Riley crossover: each band is two cascaded
 * Butterworth (Q = 1/sqrt(2)) biquads at the same frequency.
 *
 * Both bands are -6 dB at the crossover and in phase there, so lowOut + highOut
 * has flat magnitude but is an allpass of the input (Butterworth 2nd-order
 * allpass, same frequency), not the input itself.
 * Ports: in -> lowOut, highOut. Parameter 0: frequencyHz (default 1000).
 */
class CrossoverLR4 final : public Node
{
  public:
    explicit CrossoverLR4(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        updateCoefficients();
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        m_lowStage1.processBlock(inputs[0], outputs[0], numSamples);
        m_lowStage2.processBlock(outputs[0], outputs[0], numSamples);
        m_highStage1.processBlock(inputs[0], outputs[1], numSamples);
        m_highStage2.processBlock(outputs[1], outputs[1], numSamples);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_frequencyHz = value;
            updateCoefficients();
        }
    }

  private:
    static constexpr float kQ{0.70710678f};

    void updateCoefficients() noexcept
    {
        m_lowStage1.computeCoefficients(m_sampleRate, m_frequencyHz, kQ, 0.0f);
        m_lowStage2.computeCoefficients(m_sampleRate, m_frequencyHz, kQ, 0.0f);
        m_highStage1.computeCoefficients(m_sampleRate, m_frequencyHz, kQ, 0.0f);
        m_highStage2.computeCoefficients(m_sampleRate, m_frequencyHz, kQ, 0.0f);
    }

    float m_sampleRate;
    float m_frequencyHz{1000.0f};
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass> m_lowStage1;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass> m_lowStage2;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::HighPass> m_highStage1;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::HighPass> m_highStage2;
};

}
