#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Linear transform. Ports: in -> out. Parameter 0: scale, 1: offset.
 */
class ScaleOffset final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] * m_scale + m_offset;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_scale = value;
        }
        else if (paramIndex == 1)
        {
            m_offset = value;
        }
    }

  private:
    float m_scale{1.0f};
    float m_offset{0.0f};
};

}
