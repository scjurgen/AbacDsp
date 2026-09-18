#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Linear range remap. Ports: in -> out.
 * Parameters 0: inMin, 1: inMax, 2: outMin, 3: outMax (all default identity).
 */
class Map final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        const float inSpan = m_inMax - m_inMin;
        const float scale = inSpan != 0.0f ? (m_outMax - m_outMin) / inSpan : 0.0f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = m_outMin + (inputs[0][i] - m_inMin) * scale;
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
    float m_outMin{0.0f};
    float m_outMax{1.0f};
};

}
