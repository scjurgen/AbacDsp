#pragma once

#include <cmath>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Exponential range remap. Ports: in -> out.
 * Parameters 0: inMin, 1: inMax, 2: outMin, 3: outMax (outMin/outMax must stay
 * positive - the exponential domain has no zero or negative endpoint).
 */
class ExpMap final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        const float inSpan = m_inMax - m_inMin;
        const float ratio = m_outMin > 0.0f ? m_outMax / m_outMin : 1.0f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float t = inSpan != 0.0f ? (inputs[0][i] - m_inMin) / inSpan : 0.0f;
            outputs[0][i] = m_outMin * std::pow(ratio, t);
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                m_inMin = value;
                break;
            case 1:
                m_inMax = value;
                break;
            case 2:
                m_outMin = value;
                break;
            case 3:
                m_outMax = value;
                break;
            default:
                break;
        }
    }

  private:
    float m_inMin{0.0f};
    float m_inMax{1.0f};
    float m_outMin{1.0f};
    float m_outMax{2.0f};
};

}
