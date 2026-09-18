#pragma once

#include "Filters/Biquad.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::Biquad<BandPass>. Ports: in -> out.
 * Parameters 0: frequencyHz (default 1000), 1: Q (default 0.7071).
 */
class BandPass final : public Node
{
  public:
    explicit BandPass(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        updateCoefficients();
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        m_filter.processBlock(inputs[0], outputs[0], numSamples);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_frequencyHz = value;
        }
        else if (paramIndex == 1)
        {
            m_q = value;
        }
        else
        {
            return;
        }
        updateCoefficients();
    }

  private:
    void updateCoefficients() noexcept
    {
        m_filter.computeCoefficients(m_sampleRate, m_frequencyHz, m_q, 0.0f);
    }

    float m_sampleRate;
    float m_frequencyHz{1000.0f};
    float m_q{0.70710678f};
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::BandPass> m_filter;
};

}
