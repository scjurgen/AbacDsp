#pragma once

#include "Filters/Biquad.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief 4th-order Linkwitz-Riley crossover: lowOut is two cascaded
 * Biquad<LowPass> (Butterworth Q), highOut is the exact complement
 * (in - lowOut). An independently-designed cascaded Biquad<HighPass> for
 * highOut does not reconstruct flat (verified empirically via explore/,
 * error peaking near the crossover frequency); deriving it as the direct
 * complement instead guarantees lowOut + highOut == in to float precision at
 * every frequency, by construction. Ports: in -> lowOut, highOut.
 * Parameter 0: frequencyHz (default 1000).
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
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[1][i] = inputs[0][i] - outputs[0][i];
        }
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
    }

    float m_sampleRate;
    float m_frequencyHz{1000.0f};
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass> m_lowStage1;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass> m_lowStage2;
};

}
